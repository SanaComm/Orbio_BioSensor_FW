/**
 * @file ppg_agc.c
 * @brief Automatic Gain Control (AGC) + Proximity Detection for MAX86171
 *
 * ========================================================
 * ALGORITHM FLOW  (called once per window = AGC_WINDOW_SAMPLES samples)
 * ========================================================
 *
 *  ┌────────────────────────────────────────────────────────────────────┐
 *  │  Window complete  (window_cnt == AGC_WINDOW_SAMPLES)               │
 *  │                                                                    │
 *  │  avg = window_sum / AGC_WINDOW_SAMPLES                             │
 *  │                                                                    │
 *  │  ── PROXIMITY CHECK (runs always, even when AGC is suspended) ──   │
 *  │                                                                    │
 *  │  if avg < AGC_THRESH_PROX:                                         │
 *  │      prox_low_cnt++,  prox_high_cnt = 0                            │
 *  │      if on_body AND prox_low_cnt >= CONFIRM_WINDOWS:               │
 *  │          on_body = false                                           │
 *  │          set PA = AGC_PROX_IDLE_PA on prox channel                 │
 *  │          set PA = AGC_OFFBODY_PA_NONPROX (0) on other channels     │
 *  │  else:                                                             │
 *  │      prox_high_cnt++,  prox_low_cnt = 0                            │
 *  │      if !on_body AND prox_high_cnt >= RECOVER_WINDOWS:             │
 *  │          on_body = true                                            │
 *  │          set PA = AGC_PROX_RESTART_PA on all drivers               │
 *  │          set cooldown so AGC is delayed by a window                │
 *  │                                                                    │
 *  │  ── AGC STEP (only when on_body AND enabled AND cooldown == 0) ─── │
 *  │                                                                    │
 *  │  if   avg < AGC_THRESH_LOW  → fast PA increase toward target       │
 *  │  elif avg > AGC_THRESH_HIGH → fast PA decrease toward target       │
 *  │  else → no change                                                  │
 *  │                                                                    │
 *  │  On adjustment: cooldown = AGC_COOLDOWN_SAMPLES                    │
 *  └────────────────────────────────────────────────────────────────────┘
 */

#include "ppg_agc.h"
#include <string.h>

#define PRINT_AGC_INIT printf
#define PRINT_AGC_INFO printf
#define PRINT_AGC_ERROR printf
#define PRINT_AGC_IGNORE(...)

/* =========================================================================
 * Local configuration
 * ========================================================================= */

/** Cooldown is counted in samples so it does not stretch with the long window. */
#define AGC_COOLDOWN_SAMPLES        (AGC_COOLDOWN_WINDOWS * AGC_WINDOW_SAMPLES)

/* =========================================================================
 * Static driver-to-register initialisation table
 * =========================================================================
 */
typedef struct { uint8_t pa_reg; uint8_t pa_init; } drv_init_t;
typedef struct { drv_init_t drv[AGC_MAX_DRIVERS_PER_CH]; uint8_t num; } ch_init_t;

/* =========================================================================
 * Private helpers
 * ========================================================================= */

static inline int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi){
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

#define scale_fullscale_by_percentage(x) ((int32_t)((AGC_FULL_SCALE_POS * (x)) / 100))

/* =========================================================================
 * Private API
 * ========================================================================= */

/**
 * @brief Set the same PA value on all active drivers of a channel
 *        and write each register to the MAX86171 via SPI.
 *        Called when moving from on-body to off-body and vice versa
 *        and from ppg_agc_force_pa().
 */
static void set_pa_absolute(agc_channel_t *ch, max86171_t *dev, uint8_t pa_val){
    for (uint8_t d = 0u; d < ch->num_drv; d++) {
        int rc = max86171_write_reg(dev, ch->drv[d].pa_reg, &pa_val, 1u);
        if (rc)
        { 
            PRINT_AGC_ERROR ("set_pa_absolute max86171_write_reg fail\n\r");
        }
        else
        {
            // Update the value only if register write was successful, so that
            // the hardware and variable are the same
            ch->drv[d].pa_cur = pa_val;
        }
    }
}

/**
 * @brief Apply a signed delta to all active drivers, clamped to [PA_MIN, PA_MAX].
 *        Returns true if at least one register actually changed.
 */
static bool apply_pa_delta(agc_channel_t *ch, max86171_t *dev, int16_t delta)
{
    bool changed = false;

    for (uint8_t d = 0u; d < ch->num_drv; d++) {
        int16_t new_pa = (int16_t)ch->drv[d].pa_cur + (int16_t)delta;
        uint8_t u8_pa  = (uint8_t)clamp_i32((int32_t)new_pa,
                                             (int32_t)AGC_PA_MIN,
                                             (int32_t)AGC_PA_MAX);
        if (u8_pa != ch->drv[d].pa_cur) {
            int rc = max86171_write_reg(dev, ch->drv[d].pa_reg, &u8_pa, 1u);
            if (rc)
            {
                PRINT_AGC_ERROR ("apply_pa_delta max86171_write_reg fail\n\r");
            }
            else
            {
                // Update the value if register write was successful, so that
                // the hardware and variable are the same
                ch->drv[d].pa_cur = u8_pa;
                changed = true;
            }
        }
    }
    return changed;
}

/**
 * @brief Discard the accumulator and force the fast (short) window.
 *        Must be called whenever PA is changed outside the normal AGC step,
 *        otherwise a stale in-range state would select the 100-sample window
 *        exactly when fast convergence is needed.
 */
static inline void reset_window_and_state(agc_channel_t *ch)
{
    ch->window_sum = 0;
    ch->window_cnt = 0u;
    ch->agc_state  = 0u;
}

/**
 * @brief Move every channel to OFF-BODY (called when the proximity channel
 *        detects removal).  Proximity channel keeps a dim search light; the
 *        others are switched off (MEAS8 included).
 */
static void set_all_channels_offbody(ppg_agc_t *agc, max86171_t *dev)
{
    for (uint8_t c = 0u; c < AGC_NUM_CHANNELS; c++)
    {
        agc_channel_t *ch = &agc->ch[c];
        uint8_t idle_pa = (c == AGC_PROX_CH_NUM) ? AGC_PROX_IDLE_PA : AGC_OFFBODY_PA_NONPROX;

        ch->on_body = false;
        set_pa_absolute(ch, dev, idle_pa);
        // Channel is fully reset, settings will be same as in ppg_agc_init
        ch->prox_low_cnt  = 0u;
        ch->prox_high_cnt = 0u;
        // AGC will not be active for any channel so cooldown can be reset for them
        ch->cooldown_remain = 0u;
        reset_window_and_state(ch);
        PRINT_AGC_INFO("[PROX] ch%u  OFF-BODY  PA->%u\r\n", (unsigned)c, (unsigned)idle_pa);
    }
}

/**
 * @brief Move every channel that is currently OFF-BODY back to ON-BODY once the
 *        search light detects proximity.  Channels that are already on-body
 *        are left alone so their converged PA is not disturbed.
 */
static void set_all_channels_onbody (ppg_agc_t *agc, max86171_t *dev)
{
    // Loop for each measurement i.e. AGC channel
    for (uint8_t c = 0u; c < AGC_NUM_CHANNELS; c++)
    {
        agc_channel_t *ch = &agc->ch[c];
        if (ch->on_body) continue;

        ch->on_body = true;
        set_pa_absolute(ch, dev, ch->AGC_PROX_RESTART_PA);
        PRINT_AGC_INFO("[PROX] ch%u  ON-BODY   PA->%u\r\n", (unsigned)c, (unsigned)ch->AGC_PROX_RESTART_PA);
        // Skip one short window of samples taken at the old (idle) current.
        // Once proximity is detected, the remaining frame samples are
        // from the Off Body current setting and should be discarded.
        // Setting agc_state = 0 below takes care of this.
        ch->cooldown_remain = AGC_WINDOW_SAMPLES;
        ch->prox_high_cnt = AGC_PROX_RECOVER_WINDOWS;
        ch->prox_low_cnt = 0u;
        reset_window_and_state(ch);
    }
}

/**
 * @brief Table to set individual percentage thresholds per AGC channel
 *        {LOW%, HIGH%, TARGET%, PROX%, PROX_RESTART_PA}
 */
#define NUM_THRESHOLDS_PER_CHANNEL (5)
static const uint8_t threshold_table[NUM_THRESHOLDS_PER_CHANNEL * AGC_NUM_CHANNELS] =
{
    AGC_LOW_THRESHOLD_PCT, AGC_HIGH_THRESHOLD_PCT, AGC_TARGET_PCT, AGC_PROX_SIGNAL_PCT,       AGC_PROX_START_PA,
    AGC_LOW_THRESHOLD_PCT, AGC_HIGH_THRESHOLD_PCT, AGC_TARGET_PCT, AGC_PROX_SIGNAL_PCT_SMALL, AGC_PROX_START_PA,
    AGC_LOW_THRESHOLD_PCT, AGC_HIGH_THRESHOLD_PCT, AGC_TARGET_PCT, AGC_PROX_SIGNAL_PCT,       AGC_PROX_START_PA,
    AGC_LOW_THRESHOLD_PCT, AGC_HIGH_THRESHOLD_PCT, AGC_TARGET_PCT, AGC_PROX_SIGNAL_PCT_SMALL, AGC_PROX_START_PA,
    AGC_LOW_THRESHOLD_PCT, AGC_HIGH_THRESHOLD_PCT, AGC_TARGET_PCT, AGC_PROX_SIGNAL_PCT,       AGC_PROX_START_PA,
    AGC_LOW_THRESHOLD_PCT, AGC_HIGH_THRESHOLD_PCT, AGC_TARGET_PCT, AGC_PROX_SIGNAL_PCT_SMALL, AGC_PROX_START_PA,
    AGC_LOW_THRESHOLD_PCT, AGC_HIGH_THRESHOLD_PCT, AGC_TARGET_PCT, AGC_PROX_SIGNAL_PCT,       AGC_PROX_START_PA,
    AGC_LOW_THRESHOLD_PCT, AGC_HIGH_THRESHOLD_PCT, AGC_TARGET_PCT, AGC_PROX_SIGNAL_PCT_SMALL, AGC_PROX_START_PA,
};

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * @brief Move every channel to a fixed current.
 */
void set_all_channels_restart_pa (ppg_agc_t *agc, max86171_t *dev)
{
    // Loop for each measurement i.e. AGC channel
    for (uint8_t c = 0u; c < AGC_NUM_CHANNELS; c++)
    {
        agc_channel_t *ch = &agc->ch[c];
        set_pa_absolute(ch, dev, ch->AGC_PROX_RESTART_PA);
        PRINT_AGC_INFO("[AGCOFF] ch%u  PA->%u\r\n", (unsigned)c, (unsigned)ch->AGC_PROX_RESTART_PA);
    }
}

/**
 * @brief Initialise the AGC struct. Currently called with PPG is started.
 */
void ppg_agc_init(ppg_agc_t *agc)
{
    int counter = 0;
    memset(agc, 0, sizeof(*agc));

    // Loop for each measurement i.e. AGC channel
    for (uint8_t c = 0u; c < AGC_NUM_CHANNELS; c++)
    {
        agc_channel_t *ch = &agc->ch[c];
        // All channels are enabled by default
        ch->enabled   = true;
        /* assume contact on startup */
        ch->on_body   = true;
        /* Not in-range by default */
        ch->agc_state = 0u;
        ch->num_drv   = 0;
        /* Thresholds */
        ch->AGC_THRESH_LOW = scale_fullscale_by_percentage(threshold_table[counter++]);
        ch->AGC_THRESH_HIGH = scale_fullscale_by_percentage(threshold_table[counter++]);
        ch->AGC_THRESH_TARGET = scale_fullscale_by_percentage(threshold_table[counter++]);
        ch->AGC_THRESH_PROX = scale_fullscale_by_percentage(threshold_table[counter++]);
        ch->AGC_PROX_RESTART_PA = threshold_table[counter++];

        // Loop to find active DRVx from g_max86171_meas_config within a measurement
        // Bugfix: this loop was added so that the current values are taken from a central place
        // rather than having a using table which can be outdated
        for (uint8_t d = 0u; d < AGC_MAX_DRIVERS_PER_CH; d++)
        {
            uint8_t addr, value;
            // The offsets below work for the way the config table is built: okay for now
            max86171_get_config_entry(MAX86171_BASE_CONFIG_SIZE + c * MAX86171_MEAS_REGS + MAX86171_MEAS_REGS - 3 + d, &addr, &value);
            if (value)
            {
                // This chooses the active DRVx
                ch->drv[ch->num_drv].pa_reg = addr;
                // This sets the current of this DRVx driver (0 to 255)
                ch->drv[ch->num_drv].pa_cur = value;
                ch->num_drv++;
                PRINT_AGC_INIT ("AGC %d: DRVx %d (0x%02x) is active and set to %d\n\r", c, d, addr, value);
            }
        }
    }
}

/**
 * @brief It is called whenever a new frame of samples is collected for
 *        every averaged pair of PPG1/PPG2 samples, for each tag i.e. ch_idx
 *        Return value is true if any PA register was written.
 */
bool ppg_agc_feed(ppg_agc_t *agc, max86171_t *dev, uint8_t ch_idx, const int32_t raw, bool ovf_error)
{
    if (ch_idx >= AGC_NUM_CHANNELS) return false;
    // No AGC for Meas8: it follows the on/off-body state set by the other channels.
    // Nothing was written here, so report false.
    if (ch_idx == (AGC_NUM_CHANNELS - 1)) return false;

    agc_channel_t *ch = &agc->ch[ch_idx];
    const bool is_prox_ch = (ch_idx == AGC_PROX_CH_NUM);

    /* Non-proximity channels simply follow the proximity channel: while
     * off-body their LEDs are off, the samples are meaningless, and they are
     * brought back by set_all_channels_onbody(). */
    if (!is_prox_ch && !ch->on_body) return false;

    /* Cooldown is counted per sample. Capture the state at the START of this
     * sample so a window completing on the last cooldown sample is still skipped
     * (same behaviour as counting whole windows). */
    const bool in_cooldown = (ch->cooldown_remain > 0u);
    if (in_cooldown) ch->cooldown_remain--;

    // Handle OVF error cases specially by lowering target window and resetting channel current
    // TODO: Right now due to low thresholds they don't actually get changed by this logic
    if (ovf_error)
    {
        bool written = false;
        // Lower target range thresholds by 10% each, keep proximity threshold same
        uint8_t temp1 = threshold_table[NUM_THRESHOLDS_PER_CHANNEL * ch_idx];
        if (temp1 > AGC_LOW_THRESHOLD_PCT_MIN)
        {
            temp1 -= AGC_OVF_ADJUST_PCT;
            // Maintain relative offsets for other thresholds
            uint8_t temp2 = temp1 + (AGC_HIGH_THRESHOLD_PCT - AGC_LOW_THRESHOLD_PCT);
            uint8_t temp3 = temp1 + (AGC_TARGET_PCT - AGC_LOW_THRESHOLD_PCT);
            PRINT_AGC_INFO("[OVF]  ch%d  low=%d  high=%d  target=%d\n\r", ch_idx, temp1, temp2, temp3);
            // And also update the runtime versions of the same
            ch->AGC_THRESH_LOW = scale_fullscale_by_percentage(temp1);
            ch->AGC_THRESH_HIGH = scale_fullscale_by_percentage(temp2);
            ch->AGC_THRESH_TARGET = scale_fullscale_by_percentage(temp3);
            reset_window_and_state(ch);
            ch->on_body = true;
            ch->prox_low_cnt = 0;
            ch->prox_high_cnt = 0;
            // Reduce the current for next time
            written = apply_pa_delta(ch, dev, -(int16_t)AGC_MAX_FAST_PA_STEP);
            // If current was changed then wait for AGC cooldown
            if (written)
                ch->cooldown_remain = AGC_COOLDOWN_SAMPLES;
        }
        return written;
    }

    /* ── Accumulate sample ──────────────────────────────────────────────── */
    ch->window_sum += (int64_t)raw;
    ch->window_cnt++;

    // proximity channel: always 10-sample windows, so that detecting on-body -> off-body will not get slowed down
    // other channels: 10 samples while converging, 100 samples when in range
    // A motion blip or brief pressure change can push a 10-sample average out of band. That triggers a PA step computed
    // from an unrepresentative avg, which then has to be undone. A 100-sample window dilutes the blip, and if the channel
    // does leave the band, the correction is based on a more representative level.
    uint16_t agc_window_samples = AGC_WINDOW_SAMPLES;
    if (!is_prox_ch && ch->agc_state == 1) agc_window_samples = AGC_WINDOW_SAMPLES_IN_RANGE;

    /* Wait for a full window */
    if (ch->window_cnt < agc_window_samples) return false;

    /* ── Window complete: compute average and reset accumulator ─────────── */
    int32_t avg = (int32_t)(ch->window_sum / (int64_t)agc_window_samples);
    ch->last_avg   = avg;
    ch->window_sum = 0;
    ch->window_cnt = 0u;
    PRINT_AGC_IGNORE("[UPD] ch%u  avg=%ld\r\n", (unsigned)ch_idx, (long)avg);

    /* =====================================================================
     * PROXIMITY CHECK  (proximity channel only; runs regardless of AGC enable)
     * ===================================================================== */

    if (is_prox_ch)
    {
        if (avg < ch->AGC_THRESH_PROX) {
            /* Signal is very low — possible off-body */
            if (ch->prox_low_cnt < AGC_PROX_CONFIRM_WINDOWS) ch->prox_low_cnt++;
            ch->prox_high_cnt = 0u;

            if (ch->on_body && ch->prox_low_cnt >= AGC_PROX_CONFIRM_WINDOWS) {
                /* Transition: ON-BODY -> OFF-BODY for every channel */
                PRINT_AGC_INFO("[PROX] ch%u  OFF-BODY  avg=%ld\r\n", (unsigned)ch_idx, (long)avg);
                set_all_channels_offbody(agc, dev);
                return true;
            }
        } else {
            /* Signal above proximity threshold */
            if (ch->prox_high_cnt < AGC_PROX_RECOVER_WINDOWS) ch->prox_high_cnt++;
            ch->prox_low_cnt = 0u;

            if (!ch->on_body && ch->prox_high_cnt >= AGC_PROX_RECOVER_WINDOWS) {
                /* Transition: OFF-BODY -> ON-BODY for every off-body channel.
                 * 'avg' was measured at the idle current so it is not used for AGC. */
                PRINT_AGC_INFO("[PROX] ch%u  ON-BODY   avg=%ld\r\n", (unsigned)ch_idx, (long)avg);
                set_all_channels_onbody(agc, dev);
                return true;
            }
        }
    }

    /* =====================================================================
     * AGC STEP  (only when on skin, AGC enabled and cooldown elapsed)
     * ===================================================================== */
    if (!ch->on_body || !ch->enabled) return false;

    /* Windows finishing inside the cooldown may still contain samples from the
     * previous current: skip them entirely (do not update zone/window length). */
    if (in_cooldown) return false;

    /* Determine signal zone */
    uint8_t zone;
    // TODO: Consider marking below ch->AGC_THRESH_PROX also as zone=1
    // so as to not raise current when finger is taken off. But could
    // it have happened not because finger is removed but because AGC took
    // the current too low or contact is weak?
    // if      (avg < ch->AGC_THRESH_LOW && avg > ch->AGC_THRESH_PROX)  zone = 0u;   /* too dim  → raise PA */
    if      (avg < ch->AGC_THRESH_LOW)  zone = 0u;   /* too dim  → raise PA */
    else if (avg > ch->AGC_THRESH_HIGH) zone = 2u;   /* too bright → lower PA */
    else                                zone = 1u;   /* in target band         */

    if (zone != ch->agc_state)
    {
        PRINT_AGC_INFO("[ZONE] ch%u  avg=%ld  zone=%d\r\n", (unsigned)ch_idx, (long)avg, zone);
    }
    ch->agc_state = zone;

    /* In range: nothing to do (next window will be the long one) */
    if (zone == 1u) return false;

    /*
     * Apply a fast correction.
     *
     * Approximation:  PA_new_est ~= PA_current * target / avg
     * Only AGC_FAST_CORRECTION_PCT of the estimated change is applied and it is
     * capped at +/- AGC_MAX_FAST_PA_STEP. The delta is computed from drv[0] and
     * applied to all drivers, which assumes they track each other.
     *
     * The LED/PPG relationship is not perfectly linear, so the correction
     * is capped to avoid a large jump from an abnormal window.
     */
    int16_t delta = 0;
     /* Target point used for estimating the PA correction */
    const int32_t target = ch->AGC_THRESH_TARGET;

    if (avg <= 0) {
        /* Avoid division by zero; proximity detection handles this if it persists */
        delta = (int16_t)AGC_MAX_FAST_PA_STEP;
    } else {
        int64_t pa         = (int64_t)ch->drv[0].pa_cur;
        // Assuming that desired current is linear versus amplitude
        int64_t desired_pa = (pa * (int64_t)target) / (int64_t)avg;
        int64_t correction = ((desired_pa - pa) * (int64_t)AGC_FAST_CORRECTION_PCT) / 100;

        // Note: the casting of unsigned constant is necessary to avoid overflow
        // because the product is not casted to int32_t but to unsigned!!
        if      (correction >  (int64_t)AGC_MAX_FAST_PA_STEP) correction =  (int64_t)AGC_MAX_FAST_PA_STEP;
        else if (correction < -(int64_t)AGC_MAX_FAST_PA_STEP) correction = -(int64_t)AGC_MAX_FAST_PA_STEP;

        delta = (int16_t)correction;
    }

    /*
     * PA=0 is a special case: proportional scaling cannot move it.
     * The minimum 1-LSB correction lets the AGC recover from zero.
     */
    if (delta == 0)
        delta = (zone == 0u) ? 1 : -1;

    bool adjusted = apply_pa_delta(ch, dev, delta);

    if (adjusted) {
        // Skip cooldown samples so that old samples are discarded.
        // Window variables have already been reset and agc_state
        // is already not 1, so windowing will go back to the
        // shorter window for a faster response.
        ch->cooldown_remain = AGC_COOLDOWN_SAMPLES;
        ch->adj_count++;

        PRINT_AGC_INFO("[AGC]  ch%u  avg=%ld  PA->%u  adj#%u\r\n",
               (unsigned)ch_idx, (long)avg,
               (unsigned)ch->drv[0].pa_cur,
               (unsigned)ch->adj_count);
    }

    return adjusted;
}

/**
 * @brief Use a majority decision across channels to decide if the device
 *        is off the body or on.
 *        Call from app.c if needed to send this status over BLE along with the data
 *        Note: channels now mirror the proximity channel, so this vote is
 *        effectively the proximity channel's state.
 */
bool ppg_agc_is_off_body(const ppg_agc_t *agc)
{
    uint8_t off_count = 0u;
    for (uint8_t c = 0u; c < AGC_NUM_CHANNELS - 1; c++) {
        if (!agc->ch[c].on_body) off_count++;
    }
    return (off_count >= AGC_OFFBODY_MIN_CH);
}

/* End of file */
