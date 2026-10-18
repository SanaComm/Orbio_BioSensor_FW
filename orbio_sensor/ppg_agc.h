/**
 * @file ppg_agc.h
 * @brief Automatic Gain Control (AGC) + Proximity (on-body) Detection
 *        for MAX86171 PPG channels. Does not run for disable channels.
 *
 * =========================================================================
 * AGC ALGORITHM
 * =========================================================================
 * Each of the active measurement channels (MEAS1-MEAS8) runs an
 * independent loop:
 *
 *   1. Accumulate the DC average over AGC_WINDOW_SAMPLES samples.
 *   2. Compare the average against two thresholds (% of 20-bit ADC full scale):
 *
 *        avg < low threshold                 → increase PA
 *        avg > high threshold                → decrease PA
 *        Else                                → in target band, no change
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
 *       • PA is reduced to AGC_PROX_SEARCHLIGHT_PA
 *       • Normal AGC adjustments are suspended for this channel
 *       • ch.on_body = false
 *
 *   Recovery: signal must exceed AGC_PROX_SIGNAL_PCT for
 *   AGC_PROX_RECOVER_WINDOWS consecutive windows  →  declare ON-BODY:
 *       • PA is restored to AGC_PROX_RESTART_PA
 *       • AGC resumes from that value
 *       • ch.on_body = true
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
 * Cooldown in windows between successive PA adjustments.
 * 100 Hz / 10-sample window = 10 windows/s  ->  cooldown ~= 300 ms.
 */
#define AGC_COOLDOWN_WINDOWS       3u

/**
 * Fast correction: when the average is outside the target band,
 * estimate the PA change required to move the average toward the target
 * midpoint, then apply this fraction of that correction.
 *
 * This preserves the existing decision criteria but avoids crawling 1 LSB
 * at a time. Deliberately conservative to reduce overshoot.
 */
#define AGC_FAST_CORRECTION_PCT    70u

/**
 * Safety cap on one fast correction.  The AGC still has the existing
 * cooldown between corrections.
 */
#define AGC_MAX_FAST_PA_STEP       8u

/** Absolute PA register limits. */
#define AGC_PA_MIN                 0u
#define AGC_PA_MAX                 100u  // Keep at a safe middle level so as to not go
                                         // all the way to 32 mA

/** Maximum LED drivers per channel (DRVA / DRVB / DRVC). */
#define AGC_MAX_DRIVERS_PER_CH     3u

/** Number of PPG channels managed (MEAS1..MEAS8). */
#define AGC_NUM_CHANNELS           8u


/* =========================================================================
 * Proximity detection tuning
 * ========================================================================= */

/** Measurement channel (0-based AGC index) that does proximity sensing (MEAS5). */
#define AGC_PROX_CH_NUM            4u

/**
 * Signal level below which the channel is considered "possibly off-body",
 * as a percentage of the 20-bit ADC positive full scale.
 *
 * Skin contact with any reasonable LED current returns hundreds of thousands
 * of counts.  Air returns near zero even at maximum LED current.
 * Don't keep it so low that even without proximity it's detected as on-body
 */
#define AGC_PROX_SIGNAL_PCT        2u  // For the stronger PD combo

/**
 * Consecutive windows the signal must stay BELOW AGC_PROX_SIGNAL_PCT
 * before the channel is declared OFF-BODY (debounce against brief transients).
 * 3 windows x 10 samples x 10 ms/sample = 300 ms.
 */
#define AGC_PROX_CONFIRM_WINDOWS   3u

/**
 * Consecutive windows the signal must stay ABOVE AGC_PROX_SIGNAL_PCT
 * before the channel is declared ON-BODY again.
 */
#define AGC_PROX_RECOVER_WINDOWS   2u

/**
 * LED PA to use while off-body for the Proximity detection measurement.
 * Keeps a dim "search light" so the sensor can re-detect skin without
 * burning power at full current.
 * LSB is 0.12 mA in 31 mA and 0.48 mA in 124 mA range.
 * Bugfix: There will not be any "search light" if the setting is 0 as originally.
 */
#define AGC_PROX_SEARCHLIGHT_PA    3u
#define AGC_OFFBODY_PA_NONPROX     AGC_PA_MIN

/**
 * LED PA to use while off-body for non-Proximity detection measurements.
 * Kept as non-zero to get around the ppg_init logic for now.
 */
#define AGC_PA_TABLE_INIT          1u

/* =========================================================================
 * Derived constants (do not edit)
 * ========================================================================= */

/** 20-bit signed ADC positive full scale (524 287). */
#define AGC_FULL_SCALE_POS  ((int32_t)((1u << 19u) - 1u))

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
    /* ── AGC thresholds ─────────────────────────────────────────────────── */
    int32_t  AGC_THRESH_LOW;    /**< Target band lower range.               */
    int32_t  AGC_THRESH_HIGH;   /**< Target band upper range.               */
    int32_t  AGC_THRESH_TARGET; /**< Target.                                */
    int32_t  AGC_PROX_RESTART_PA;   /**< Proximity start current.           */

    /* ── Sample accumulator ─────────────────────────────────────────────── */
    int64_t  window_sum;        /**< Running sum over the current window.   */
    uint16_t window_cnt;        /**< Samples accumulated so far.            */

    /* ── AGC cooldown ────────────────────────────────────────────────────── */
    uint16_t  cooldown_remain;  /**< Samples to wait before next PA step.   */

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
    bool    enabled;            /**< false = skip AGC                       */
} agc_channel_t;

/** Top-level AGC context — one instance per device. */
typedef struct {
    agc_channel_t ch[AGC_NUM_CHANNELS];
} ppg_agc_t;

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * @brief Move every channel to a fixed current.
 */
void set_all_channels_restart_pa (ppg_agc_t *agc, max86171_t *dev);

/**
 * @brief  Initialise AGC + proximity state from the compile-time config table.
 *
 *         Call once after max86171_load_config_from_table() succeeds.
 *         Seeds PA shadow registers from g_max86171_meas_config values.
 *         All channels start with on_body = true.
 *
 * @param  agc  Pointer to the context to initialise.
 */
void ppg_agc_init(ppg_agc_t *agc, max86171_t *dev, uint16_t meas_mask);

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
bool ppg_agc_feed(ppg_agc_t *agc, max86171_t *dev, uint8_t ch_idx, const int32_t raw, bool ovf_error);

#ifdef __cplusplus
}
#endif

#endif /* PPG_AGC_H */
