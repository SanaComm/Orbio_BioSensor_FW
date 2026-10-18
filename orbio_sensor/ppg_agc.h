/**
 * @file ppg_agc.h
 * @brief Automatic Gain Control (AGC) + Proximity (on-body) Detection
 *        for MAX86171 PPG channels
 *
 * =========================================================================
 * AGC ALGORITHM
 * =========================================================================
 * Each of the 8 active measurement channels (MEAS1-MEAS8) runs an
 * independent loop:
 *
 *   1. Accumulate the DC average over AGC_WINDOW_SAMPLES samples.
 *   2. Compare the average against two thresholds (% of 20-bit ADC full scale):
 *
 *        avg < AGC_LOW_THRESHOLD_PCT  (80%)  → increase PA by AGC_PA_STEP
 *        avg > AGC_HIGH_THRESHOLD_PCT (90%)  → decrease PA by AGC_PA_STEP
 *        80% <= avg <= 90%                   → in target band, no change
 *
 *   3. A cooldown of AGC_COOLDOWN_WINDOWS windows is enforced between
 *      consecutive adjustments to prevent oscillation.
 *
 * =========================================================================
 * PROXIMITY (ON-BODY) DETECTION
 * =========================================================================
 * Problem: when the sensor is lifted off the skin the PPG signal collapses
 * toward zero even though the LED current may be high.  Without detection
 * the AGC would blindly ramp LED current to the maximum ceiling and keep it
 * there, wasting power and producing meaningless data.
 *
 * Detection rule (evaluated once per window, same accumulator as AGC):
 *
 *   Signal below AGC_PROX_SIGNAL_PCT for AGC_PROX_CONFIRM_WINDOWS
 *   consecutive windows  →  declare OFF-BODY:
 *       • PA is reduced to AGC_PROX_IDLE_PA on all drivers
 *       • Normal AGC adjustments are suspended for this channel
 *       • ch.on_body = false
 *
 *   Recovery: signal must exceed AGC_PROX_SIGNAL_PCT for
 *   AGC_PROX_RECOVER_WINDOWS consecutive windows  →  declare ON-BODY:
 *       • PA is restored to AGC_PROX_RESTART_PA (mid-range starting point)
 *       • AGC resumes from that value
 *       • ch.on_body = true
 *
 * Global helper ppg_agc_is_off_body() returns true when a configurable
 * minimum number of channels (AGC_OFFBODY_MIN_CH) report off-body.
 *
 * HARDWARE MAPPING is taken from g_max86171_meas_config in max86171.h
 */

#ifndef PPG_AGC_H
#define PPG_AGC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "max86171.h"
#include "max86171_regs.h"   /* register addresses — no HAL dependency */

/* =========================================================================
 * AGC tuning constants
 * ========================================================================= */

/** Number of samples that form one evaluation window. */
#define AGC_WINDOW_SAMPLES          10u

/**
 * Target band as a percentage of the positive 20-bit ADC full scale
 * (0x7FFFF = 524 287).  AGC keeps the DC average inside [LOW, HIGH].
 */
#define AGC_LOW_THRESHOLD_PCT       80u   /* below -> raise LED   */
#define AGC_HIGH_THRESHOLD_PCT      90u   /* above -> lower LED   */

/**
 * Cooldown in windows between successive PA adjustments.
 * 100 Hz / 10-sample window = 10 windows/s  ->  cooldown ~= 300 ms.
 */
#define AGC_COOLDOWN_WINDOWS         3u

/** Step size per PA adjustment (1 LSB =~ 0.5 mA at 124 mA LED range). */
/** (1 LSB =~ 0.12 mA at 31 mA LED range). */
#define AGC_PA_STEP                  1u

/** Absolute PA register limits. */
#define AGC_PA_MIN                   0u
#define AGC_PA_MAX                 255u

/** Maximum LED drivers per channel (DRVA / DRVB / DRVC). */
#define AGC_MAX_DRIVERS_PER_CH       3u

/** Number of PPG channels managed (MEAS1..MEAS8). */
#define AGC_NUM_CHANNELS             8u


/* =========================================================================
 * Proximity detection tuning
 * ========================================================================= */

/**
 * Signal level below which the channel is considered "possibly off-body",
 * as a percentage of the 20-bit ADC positive full scale.
 *
 * Skin contact with any reasonable LED current returns hundreds of thousands
 * of counts.  Air returns near zero even at maximum LED current.
 * 15% = ~78 643 counts is conservative enough to survive motion artefacts.
 */
#define AGC_PROX_SIGNAL_PCT         15u

/**
 * Consecutive windows the signal must stay BELOW AGC_PROX_SIGNAL_PCT
 * before the channel is declared OFF-BODY (debounce against brief transients).
 * 3 windows x 10 samples x 10 ms/sample = 300 ms.
 */
#define AGC_PROX_CONFIRM_WINDOWS     3u

/**
 * Consecutive windows the signal must stay ABOVE AGC_PROX_SIGNAL_PCT
 * before the channel is declared ON-BODY again.
 */
#define AGC_PROX_RECOVER_WINDOWS     2u

/**
 * LED PA to use while off-body.
 * Keeps a dim "search light" so the sensor can re-detect skin without
 * burning power at full current.
 * A value of 5 is 0.6 mA in 31 mA and 2.4 mA in 124 mA range.
 * Bugfix: There will not be any "search light" if the setting is 0 as originally.
 */
#define AGC_PROX_IDLE_PA    5u

/**
 * PA to restore when skin contact is detected again.
 * Mid-range starting point lets AGC converge without large overshoot.
 * 30 LSB =~ 3.6 mA in 31 mA range and 14.4 mA in 124 mA range.
 */
#define AGC_PROX_RESTART_PA         30u

/**
 * Minimum number of channels that must report off-body before
 * ppg_agc_is_off_body() returns true.
 * Majority vote (5 of 8) prevents a single noisy channel from
 * triggering a false removal event.
 */
#define AGC_OFFBODY_MIN_CH           5u


/* =========================================================================
 * Derived constants (do not edit)
 * ========================================================================= */

/** 20-bit signed ADC positive full scale (524 287). */
#define AGC_FULL_SCALE_POS  ((int32_t)((1u << 19u) - 1u))

/** Absolute count thresholds derived from percentages above. */
#define AGC_THRESH_LOW   ((int32_t)((AGC_FULL_SCALE_POS * (int32_t)AGC_LOW_THRESHOLD_PCT)  / 100))
#define AGC_THRESH_HIGH  ((int32_t)((AGC_FULL_SCALE_POS * (int32_t)AGC_HIGH_THRESHOLD_PCT) / 100))
#define AGC_THRESH_PROX  ((int32_t)((AGC_FULL_SCALE_POS * (int32_t)AGC_PROX_SIGNAL_PCT)    / 100))

/* =========================================================================
 * Data types
 * ========================================================================= */

/** State of a single LED driver within a measurement channel. */
typedef struct {
    uint8_t pa_reg;   /**< MAX86171 register address (0x00 = not used). */
    uint8_t pa_cur;   /**< Current PA value written to hardware.        */
} agc_driver_t;

/** Per-channel AGC + proximity state. */
typedef struct {
    /* ── Sample accumulator ─────────────────────────────────────────────── */
    int64_t  window_sum;        /**< Running sum over the current window.   */
    uint16_t window_cnt;        /**< Samples accumulated so far.            */

    /* ── AGC cooldown ────────────────────────────────────────────────────── */
    uint8_t  cooldown_remain;   /**< Windows to wait before next PA step.   */

    /* ── LED driver configuration ────────────────────────────────────────── */
    agc_driver_t drv[AGC_MAX_DRIVERS_PER_CH];
    uint8_t      num_drv;       /**< Active entries in drv[].               */

    /* ── Proximity state ─────────────────────────────────────────────────── */
    bool    on_body;            /**< true = sensor touching skin.           */
    uint8_t prox_low_cnt;       /**< Consecutive windows below PROX level.  */
    uint8_t prox_high_cnt;      /**< Consecutive windows above PROX level.  */

    /* ── Diagnostics (read-only for callers) ─────────────────────────────── */
    int32_t last_avg;           /**< Average from the most recent window.   */
    uint8_t agc_state;          /**< 0 = low, 1 = in-range, 2 = high.      */
    uint8_t adj_count;          /**< Total PA adjustments since init.       */
    bool    enabled;            /**< false = freeze PA, skip AGC (not prox).*/
} agc_channel_t;

/** Top-level AGC context — one instance per device. */
typedef struct {
    agc_channel_t ch[AGC_NUM_CHANNELS];
} ppg_agc_t;

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * @brief  Initialise AGC + proximity state from the compile-time config table.
 *
 *         Call once after max86171_load_config_from_table() succeeds.
 *         Seeds PA shadow registers from g_max86171_meas_config values.
 *         All channels start with on_body = true.
 *
 * @param  agc  Pointer to the context to initialise.
 */
void ppg_agc_init(ppg_agc_t *agc);

/**
 * @brief  Feed one raw PPG sample into the AGC + proximity engine.
 *
 * @param  agc     AGC context.
 * @param  dev     MAX86171 device handle (used for SPI PA writes).
 * @param  ch_idx  Channel index 0..7  (0=MEAS1 .. 7=MEAS8).
 * @param  raw     signed raw sample.
 *
 * @return true  if a PA register write was issued this call.
 * @return false otherwise.
 */
bool ppg_agc_feed(ppg_agc_t *agc, max86171_t *dev, uint8_t ch_idx, const int32_t raw);

/**
 * @brief  Return true if the sensor is determined to be off the body.
 *
 *         Uses a majority vote: returns true when at least
 *         AGC_OFFBODY_MIN_CH channels report on_body == false.
 *
 * @param  agc  AGC context (read-only).
 */
bool ppg_agc_is_off_body(const ppg_agc_t *agc);

/**
 * @brief  Return the on_body flag for a single channel.
 * @param  agc     AGC context (read-only).
 * @param  ch_idx  Channel index 0..7.
 */
bool ppg_agc_channel_on_body(const ppg_agc_t *agc, uint8_t ch_idx);

/**
 * @brief  Query the current PA of the first active driver for a channel.
 * @param  agc     AGC context (read-only).
 * @param  ch_idx  Channel index 0..7.
 * @return Current PA value (0-255), or 0 if channel index is out of range.
 */
uint8_t ppg_agc_get_pa(const ppg_agc_t *agc, uint8_t ch_idx);

/**
 * @brief  Enable or disable AGC for a specific channel.
 *         Proximity detection continues running even when AGC is disabled.
 * @param  agc     AGC context.
 * @param  ch_idx  Channel index 0..7.
 * @param  enable  true = AGC active, false = freeze PA.
 */
void ppg_agc_set_enable(ppg_agc_t *agc, uint8_t ch_idx, bool enable);

/**
 * @brief  Hard-set the PA for all active drivers of a channel and write to HW.
 *         Resets the window accumulator and applies a fresh cooldown.
 *         Useful after a STOP/START to seed a known starting current.
 *
 * @param  agc     AGC context.
 * @param  dev     MAX86171 device handle.
 * @param  ch_idx  Channel index 0..7.
 * @param  pa_val  PA value to write (0-255).
 */
void ppg_agc_force_pa(ppg_agc_t *agc, max86171_t *dev,
                      uint8_t ch_idx, uint8_t pa_val);

#ifdef __cplusplus
}
#endif

#endif /* PPG_AGC_H */
