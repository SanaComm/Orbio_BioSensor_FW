/**
 * @file ppg_agc.c
 * @brief Automatic Gain Control (AGC) + Proximity Detection for MAX86171
 *        Does not run for disabled channels
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
 *  │  if avg < fullscale(AGC_PROX_SIGNAL_PCT):                          │
 *  │      prox_low_cnt++,  prox_high_cnt = 0                            │
 *  │      if on_body AND prox_low_cnt >= CONFIRM_WINDOWS:               │
 *  │          on_body = false                                           │
 *  │          set PA = AGC_PROX_SEARCHLIGHT_PA on prox channel          │
 *  │          set PA = AGC_OFFBODY_PA_NONPROX (0) on other channels     │
 *  │  else:                                                             │
 *  │      prox_high_cnt++,  prox_low_cnt = 0                            │
 *  │      if !on_body AND prox_high_cnt >= RECOVER_WINDOWS:             │
 *  │          on_body = true                                            │
 *  │          set PA = AGC_PROX_RESTART_PA on all drivers               │
 *  │          set cooldown so AGC is delayed by a window                │
 *  │                                                                    │
 *  │  ── AGC STEP (only when on_body AND cooldown == 0) ───             │
 *  │          (also skipped while prox channel's prox_low_cnt > 0)      │
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

// Cooldown is counted in samples.
// It is a stale sample guard to prevent samples taken with older setting
// from being used.
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
 *        and from set_all_channels_restart_pa() which is used in AGC off case.
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
 * @brief Discard the accumulator.
 *        Must be called whenever PA is changed outside the normal AGC step,
 *        otherwise we have stale window variables.
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
        uint8_t idle_pa = (c == AGC_PROX_CH_NUM) ? AGC_PROX_SEARCHLIGHT_PA : AGC_OFFBODY_PA_NONPROX;

        // First check if the channel is enabled
        if (!ch->enabled) continue;
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

        // First check if the channel is enabled
        if (!ch->enabled) continue;

        // Already on-body, don't change settings
        if (ch->on_body) continue;

        ch->on_body = true;
        set_pa_absolute(ch, dev, ch->AGC_PROX_RESTART_PA);
        PRINT_AGC_INFO("[PROX] ch%u  ON-BODY   PA->%u\r\n", (unsigned)c, (unsigned)ch->AGC_PROX_RESTART_PA);
        // Skip one window of samples taken at the old (idle) current.
        // Once proximity is detected, the remaining frame samples are
        // from the Off Body current setting and should be discarded.
        ch->cooldown_remain = AGC_WINDOW_SAMPLES;
        ch->prox_high_cnt = AGC_PROX_RECOVER_WINDOWS;
        ch->prox_low_cnt = 0u;
        reset_window_and_state(ch);
    }
}

/**
 * @brief Table to set individual percentage thresholds per AGC channel
 *        {THRESH_LOW, THRESH_HIGH, THRESH_TARGET, PROX_RESTART_PA}
 *        Positive 20-bit ADC full scale (0x7FFFF = 524 287)
 *        Earlier used to be percentages, but that makes it difficult to
 *        set values due to integer %ages and also calculation is not
 *        intuitively visible.
 *
 *        Defined for all channels as enabled channels may change.
 */
#define NUM_SETTINGS_PER_CHANNEL (4)
static const uint32_t settings_table[NUM_SETTINGS_PER_CHANNEL * AGC_NUM_CHANNELS] =
{
    50000, 200000, 100000, 16, // MEAS1 (mid sensitive)
    30000, 200000,  60000, 24, // MEAS2 (least sensitive)
    50000, 200000, 100000,  8, // MEAS3 (most sensitive)
    50000, 200000, 100000, 16, // MEAS4 (mid sensitive)
    50000, 200000, 100000,  8, // MEAS5 (most sensitive)
    30000, 200000,  60000, 24, // MEAS6 (least sensitive)
    35000, 200000,  70000, 24, // MEAS7 (least sensitive)
    35000, 200000,  70000, 24, // MEAS8 (AGC inactive, targets not used, PROX_RESTART_PA is needed)
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
        // First check if the channel is enabled
        if (!ch->enabled) continue;
        set_pa_absolute(ch, dev, ch->AGC_PROX_RESTART_PA);
        PRINT_AGC_INFO("[AGCOFF] ch%u  PA->%u\r\n", (unsigned)c, (unsigned)ch->AGC_PROX_RESTART_PA);
    }
}

/**
 * @brief Initialise the AGC struct. Currently called with PPG is started.
 */
void ppg_agc_init(ppg_agc_t *agc, max86171_t *dev, uint16_t meas_mask)
{
    int counter = 0;
    memset(agc, 0, sizeof(*agc));

    if ((meas_mask & (1 << AGC_PROX_CH_NUM)) == 0)
    {
        PRINT_AGC_ERROR ("ppg_agc_init ERROR: Prox sensing channel is not enabled!\n\r");
        return;
    }
    // Loop for each measurement i.e. AGC channel
    // Defined and set for all channels.
    // Enabled channels may change during development so do not change this to continue
    // on disabled channels else it will go out of step.
    for (uint8_t c = 0u; c < AGC_NUM_CHANNELS; c++)
    {
        agc_channel_t *ch = &agc->ch[c];
        // Mark if channel's AGC is enabled
        ch->enabled   = (meas_mask >> c) & 1u;
        /* assume contact on startup */
        ch->on_body   = true;
        /* Not in-range by default */
        ch->agc_state = 0u;
        ch->num_drv   = 0;
        /* Thresholds */
        ch->AGC_THRESH_LOW = settings_table[counter++];
        ch->AGC_THRESH_HIGH = settings_table[counter++];
        ch->AGC_THRESH_TARGET = settings_table[counter++];
        ch->AGC_PROX_RESTART_PA = settings_table[counter++];

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
        // Hack was needed to setup AGC shadow structure, undo now that it is done
        if (c != AGC_PROX_CH_NUM) set_pa_absolute(ch, dev, AGC_PA_MIN);
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

    agc_channel_t *ch = &agc->ch[ch_idx];
    const bool is_prox_ch = (ch_idx == AGC_PROX_CH_NUM);

    // First check if the channel is enabled
    if (!ch->enabled) return false;

    /* Non-proximity channels simply follow the proximity channel: while
     * off-body their LEDs are off, the samples are meaningless, and they are
     * brought back by set_all_channels_onbody(). */
    if (!is_prox_ch && !ch->on_body) return false;

    /* Cooldown is counted per sample. Capture the state at the START of this
     * sample so a window completing on the last cooldown sample is skipped */
    const bool in_cooldown = (ch->cooldown_remain > 0u);
    if (in_cooldown) ch->cooldown_remain--;

    // NOTE: Right now OVF error is not handled.
    // In the future it will be used to reduce the target ranges and currents
    // but the initial objective is to set them so that there is no OVF.
    if (ovf_error)
    {
        PRINT_AGC_INFO("[OVF]  ch%d  low=%ld  high=%ld  target=%ld\n\r", ch_idx, ch->AGC_THRESH_LOW, ch->AGC_THRESH_HIGH, ch->AGC_THRESH_TARGET);
        return false;
    }

    /* ── Accumulate sample ──────────────────────────────────────────────── */
    ch->window_sum += (int64_t)raw;
    ch->window_cnt++;

    /* Wait for a full window */
    if (ch->window_cnt < AGC_WINDOW_SAMPLES) return false;

    /* ── Window complete: compute average and reset accumulator ─────────── */
    int32_t avg = (int32_t)(ch->window_sum / (int64_t)AGC_WINDOW_SAMPLES);
    ch->last_avg   = avg;
    ch->window_sum = 0;
    ch->window_cnt = 0u;
    PRINT_AGC_INFO("[UPD] ch%u  avg=%ld\r\n", (unsigned)ch_idx, (long)avg);

    /* =====================================================================
     * PROXIMITY CHECK  (proximity channel only; runs regardless of AGC enable)
     * ===================================================================== */

    if (is_prox_ch)
    {
        if (avg < scale_fullscale_by_percentage(AGC_PROX_SIGNAL_PCT)) {
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
     * AGC STEP  (only when on skin and cooldown elapsed)
     * ===================================================================== */
    if (!ch->on_body) return false;

    /* Windows finishing inside the cooldown may still contain samples from the
     * previous current: skip them entirely */
    if (in_cooldown) return false;

    /* Freeze the AGC while the proximity channel is below its skin
     * threshold. On the prox channel, prox_low_cnt > 0 means the latest window
     * was below AGC_PROX_SIGNAL_PCT (the else branch above resets it to 0).
     * This could be finger removal, so do not chase the falling signal by
     * raising current. Either the next windows confirm OFF-BODY, or the signal
     * recovers (prox_low_cnt = 0) and the AGC resumes normally.
     * Gating on the PROX channel (not on this channel's own avg) is deliberate:
     * a non-prox channel can legitimately read "too dim" (e.g. right after init,
     * where it is parked at AGC_PA_MIN, or after restart), so its own avg cannot
     * tell finger removal from normal ramp-up. */
    if (agc->ch[AGC_PROX_CH_NUM].prox_low_cnt > 0u) return false;

    /* Determine signal zone */
    uint8_t zone;
    if      (avg < ch->AGC_THRESH_LOW)  zone = 0u;   /* too dim  → raise PA */
    else if (avg > ch->AGC_THRESH_HIGH) zone = 2u;   /* too bright → lower PA */
    else                                zone = 1u;   /* in target band         */

    if (zone != ch->agc_state)
    {
        PRINT_AGC_INFO("[ZONE] ch%u  avg=%ld  zone=%d\r\n", (unsigned)ch_idx, (long)avg, zone);
    }
    ch->agc_state = zone;

    /* In range: nothing to do */
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
     * is capped to avoid a large jump.
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
        // Window variables have already been reset.
        ch->cooldown_remain = AGC_COOLDOWN_SAMPLES;
        ch->adj_count++;

        PRINT_AGC_INFO("[AGC]  ch%u  avg=%ld  PA->%u  adj#%u\r\n",
               (unsigned)ch_idx, (long)avg,
               (unsigned)ch->drv[0].pa_cur,
               (unsigned)ch->adj_count);
    }

    return adjusted;
}

/* End of file */
