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
 *  │          set PA = AGC_PROX_IDLE_PA on all drivers  (dim search)    │
 *  │  else:                                                             │
 *  │      prox_high_cnt++,  prox_low_cnt = 0                            │
 *  │      if !on_body AND prox_high_cnt >= RECOVER_WINDOWS:             │
 *  │          on_body = true                                            │
 *  │          set PA = AGC_PROX_RESTART_PA on all drivers               │
 *  │          reset cooldown so AGC can start immediately               │
 *  │                                                                    │
 *  │  ── AGC STEP (only when on_body AND enabled AND cooldown == 0) ─── │
 *  │                                                                    │
 *  │  if   avg < AGC_THRESH_LOW  → fast PA increase toward target       │
 *  │  elif avg > AGC_THRESH_HIGH → fast PA decrease toward target       │
 *  │  else → no change                                                  │
 *  │                                                                    │
 *  │  On adjustment: cooldown = AGC_COOLDOWN_WINDOWS                    │
 *  └────────────────────────────────────────────────────────────────────┘
 */

#include "ppg_agc.h"
#include <string.h>

#define PRINT_AGC_INIT printf
#define PRINT_AGC_INFO printf
#define PRINT_AGC_ERROR printf
#define PRINT_AGC_IGNORE(...)

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
            // Update the value if register write was successful, so that
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
 * @brief Set all channels to On Body state once the search light detects proximity
 */
static void set_all_channels_onbody (ppg_agc_t *agc, max86171_t *dev)
{
    // Loop for each measurement i.e. AGC channel
    for (uint8_t c = 0u; c < AGC_NUM_CHANNELS; c++)
    {
        agc_channel_t *ch = &agc->ch[c];
        ch->on_body = true;
        set_pa_absolute(ch, dev, ch->AGC_PROX_RESTART_PA);
        PRINT_AGC_INFO("[PROX] ch%u  ON-BODY   PA->%u\r\n", (unsigned)c, (unsigned)ch->AGC_PROX_RESTART_PA);
        ch->cooldown_remain = 1u;
        ch->prox_high_cnt = AGC_PROX_RECOVER_WINDOWS;
        ch->prox_low_cnt = 0u;
        ch->window_sum = 0;
        ch->window_cnt = 0u;
    }
}

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * @brief Table to set individual percentage thresholds per AGC channel
 */
#define NUM_THRESHOLDS_PER_CHANNEL (4)
static uint8_t threshold_table[NUM_THRESHOLDS_PER_CHANNEL * AGC_NUM_CHANNELS] =
{
    AGC_LOW_THRESHOLD_PCT, AGC_HIGH_THRESHOLD_PCT, AGC_PROX_SIGNAL_PCT, AGC_PROX_RESTART_PA,
    AGC_LOW_THRESHOLD_PCT, AGC_HIGH_THRESHOLD_PCT, AGC_PROX_SIGNAL_PCT_SMALL, AGC_PROX_RESTART_PA,
    AGC_LOW_THRESHOLD_PCT, AGC_HIGH_THRESHOLD_PCT, AGC_PROX_SIGNAL_PCT, AGC_PROX_RESTART_PA,
    AGC_LOW_THRESHOLD_PCT, AGC_HIGH_THRESHOLD_PCT, AGC_PROX_SIGNAL_PCT_SMALL, AGC_PROX_RESTART_PA,
    AGC_LOW_THRESHOLD_PCT, AGC_HIGH_THRESHOLD_PCT, AGC_PROX_SIGNAL_PCT, AGC_PROX_RESTART_PA,
    AGC_LOW_THRESHOLD_PCT, AGC_HIGH_THRESHOLD_PCT, AGC_PROX_SIGNAL_PCT_SMALL, AGC_PROX_RESTART_PA,
    AGC_LOW_THRESHOLD_PCT, AGC_HIGH_THRESHOLD_PCT, AGC_PROX_SIGNAL_PCT, AGC_PROX_RESTART_PA,
    AGC_LOW_THRESHOLD_PCT, AGC_HIGH_THRESHOLD_PCT, AGC_PROX_SIGNAL_PCT_SMALL, AGC_PROX_RESTART_PA,
};

/**
 * @brief Function to set individual percentage thresholds for an AGC channel
 *
 * @param  chan  Channel number (0 to 7)
 * @param  high  Target range maximum percentage of full range
 * @param  low   Target range minimum percentage of full range
 */
static void set_channel_thresholds (uint8_t ch_idx, uint8_t low, uint8_t high)
{
    if (ch_idx >= AGC_NUM_CHANNELS)
        return;
    threshold_table[NUM_THRESHOLDS_PER_CHANNEL * ch_idx] = low;
    threshold_table[NUM_THRESHOLDS_PER_CHANNEL * ch_idx + 1] = high;
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
        /* in-range until first window evaluated */
        ch->agc_state = 1u;
        ch->num_drv   = 0;
        /* Thresholds */
        ch->AGC_THRESH_LOW = ((int32_t)((AGC_FULL_SCALE_POS * threshold_table[counter++]) / 100));
        ch->AGC_THRESH_HIGH = ((int32_t)((AGC_FULL_SCALE_POS * threshold_table[counter++]) / 100));
        ch->AGC_THRESH_PROX = ((int32_t)((AGC_FULL_SCALE_POS * threshold_table[counter++]) / 100));
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
 *        Return value is whether currents were adjusted in the registers or not
 */
bool ppg_agc_feed(ppg_agc_t *agc, max86171_t *dev, uint8_t ch_idx, const int32_t raw, bool ovf_error)
{
    if (ch_idx >= AGC_NUM_CHANNELS) return false;
    // No AGC for Meas8, rely on other channels Off Body to switch its current off
    if (ch_idx == (AGC_NUM_CHANNELS - 1)) return true;
    agc_channel_t *ch = &agc->ch[ch_idx];

    // Handle OVF error cases specially by lowering target window and resetting channel current
    if (ovf_error)
    {
        // Lower target range thresholds by 10% each, keep proximity threshold same
        uint8_t temp1 = threshold_table[NUM_THRESHOLDS_PER_CHANNEL * ch_idx], temp2;
        if (temp1 > AGC_LOW_THRESHOLD_PCT_MIN) temp1 -= 10;
        temp2 = temp1 + 10;
        PRINT_AGC_INFO("[OVF]  ch%d  low=%d  high=%d\n\r", ch_idx, temp1, temp2);
        // Make the change into the table so it's used for this channel next startup as well
        set_channel_thresholds (ch_idx, temp1, temp2);
        // And also update the runtime versions of the same
        ch->AGC_THRESH_LOW = ((int32_t)((AGC_FULL_SCALE_POS * temp1) / 100));
        ch->AGC_THRESH_HIGH = ((int32_t)((AGC_FULL_SCALE_POS * temp2) / 100));
        ch->window_sum = 0;
        ch->window_cnt = 0;
        ch->on_body = true;
        ch->prox_low_cnt = 0;
        ch->prox_high_cnt = 0;
        // Reduce the current for next time
        apply_pa_delta(ch, dev, -(int16_t)AGC_MAX_FAST_PA_STEP);
        ch->cooldown_remain = AGC_COOLDOWN_WINDOWS;
        return true;
    }

    /* Accumulate sample */
    ch->window_sum += (int64_t)raw;
    ch->window_cnt++;

    /* Wait for a full window */
    if (ch->window_cnt < AGC_WINDOW_SAMPLES) return false;

    /* ── Window complete: compute average and reset accumulator ─────────── */
    int32_t avg = (int32_t)(ch->window_sum / (int64_t)AGC_WINDOW_SAMPLES);
    ch->last_avg   = avg;
    ch->window_sum = 0;
    ch->window_cnt = 0u;
    PRINT_AGC_IGNORE("[UPD] ch%u  avg=%ld\r\n", (unsigned)ch_idx, (long)avg);

    bool hw_write = false;

    /* =====================================================================
     * PROXIMITY CHECK  (runs regardless of AGC enable flag)
     * ===================================================================== */
    if (avg < ch->AGC_THRESH_PROX) {
        /* Signal is very low — possible off-body */
        ch->prox_low_cnt++;
        if (ch->prox_low_cnt > AGC_PROX_CONFIRM_WINDOWS) {
            ch->prox_low_cnt = AGC_PROX_CONFIRM_WINDOWS;   /* saturate */
        }
        else
        {
        PRINT_AGC_IGNORE("[OFF]  ch%d  prox=%d\n\r", ch_idx, ch->prox_low_cnt);
        }
        ch->prox_high_cnt = 0u;

        if (ch->on_body && ch->prox_low_cnt >= AGC_PROX_CONFIRM_WINDOWS) {
            /* Transition: ON-BODY → OFF-BODY */
            ch->on_body = false;
            // Only Meas5 i.e. ch4 in AGC, is used for Proximity sensing
            uint8_t local_idle_pa = 0;
            if (ch_idx == 4)
            {
                local_idle_pa = AGC_PROX_IDLE_PA;
                // Act on last channel's behalf as AGC is not run for it
                ppg_agc_force_pa(agc, dev, AGC_NUM_CHANNELS - 1, 0);
            }
            set_pa_absolute(ch, dev, local_idle_pa);
            // AGC will not act when OFF-BODY but reset the variable anyway
            ch->cooldown_remain = 0u;
            hw_write = true;

            PRINT_AGC_INFO("[PROX] ch%u  OFF-BODY  avg=%ld  PA->%u\r\n",
                   (unsigned)ch_idx, (long)avg, local_idle_pa);
        }
    } else {
        /* Signal above proximity threshold */
        ch->prox_high_cnt++;
        if (ch->prox_high_cnt > AGC_PROX_RECOVER_WINDOWS) {
            ch->prox_high_cnt = AGC_PROX_RECOVER_WINDOWS;
        }
        else
        {
        PRINT_AGC_IGNORE("[ON]  ch%d  prox=%d\n\r", ch_idx, ch->prox_high_cnt);
        }
        ch->prox_low_cnt = 0u;

        if (!ch->on_body && ch->prox_high_cnt >= AGC_PROX_RECOVER_WINDOWS) {
            PRINT_AGC_INFO("[PROX] ch%u  ON-BODY   avg=%ld\r\n", (unsigned)ch_idx, (long)avg);
            /* Transition: OFF-BODY → ON-BODY */
            set_all_channels_onbody (agc, dev);
            hw_write = true;
        }
        // TODO: Once proximity is detected, shouldn't the remaining frame samples be discarded
        // because they're from the Off Body current setting.
    }

    /* =====================================================================
     * AGC STEP  (only when on skin and AGC enabled and cooldown elapsed)
     * ===================================================================== */
    if (!ch->on_body || !ch->enabled) {
        /* Off-body or user-disabled: no AGC adjustment, count down cooldown */
        if (ch->cooldown_remain > 0u) ch->cooldown_remain--;
        return hw_write;
    }

    /* Determine signal zone */
    uint8_t zone;
    // TODO: Consider marking below ch->AGC_THRESH_PROX also as zone=1
    // so as to not raise current when finger is taken off. But could
    // it have happened not because finger is removed but because AGC took
    // the current too low or contact is weak?
    // if      (avg < ch->AGC_THRESH_LOW && avg > ch->AGC_THRESH_PROX)  zone = 0u;   /* too dim  → raise PA */
    if      (avg < ch->AGC_THRESH_LOW)  zone = 0u;   /* too dim  → raise PA */
    else if (avg > ch->AGC_THRESH_HIGH) zone = 2u;   /* too bright → lower PA */
    else                            zone = 1u;   /* in target band        */

    if (zone != ch->agc_state)
    {
        PRINT_AGC_INFO("[UPD] ch%u  avg=%ld  zone=%d\r\n", (unsigned)ch_idx, (long)avg, zone);
    }
    ch->agc_state = zone;

    /* Respect cooldown */
    if (ch->cooldown_remain > 0u) {
        ch->cooldown_remain--;
        return hw_write;
    }

    /* In range: nothing to do */
    if (zone == 1u) return hw_write;

    /*
     * Apply a fast correction.
     *
     * We keep exactly the same decision criterion (outside the 80-90%
     * band), but instead of changing PA by 1 LSB, estimate where PA would
     * need to be to bring the signal to the target midpoint.  Only
     * AGC_FAST_CORRECTION_PCT of that estimated correction is applied.
     *
     * Approximation:
     *     PA_new_est ~= PA_current * target / avg
     *
     * The LED/PPG relationship is not perfectly linear, so the correction
     * is capped to avoid a large jump from an abnormal window.
     */
    int16_t delta = 0;
     /* Target point used for estimating the PA correction (midpoint of target band). */
    int32_t target = (ch->AGC_THRESH_LOW + ch->AGC_THRESH_HIGH) / 2;

    if (avg <= 0) {
        /* Avoid division by zero; proximity detection will handle this
         * condition if it persists, while this gives AGC a strong push. */
        delta = (int16_t)AGC_MAX_FAST_PA_STEP;
    } else {
        uint8_t pa = ch->drv[0].pa_cur;
        // Assuming that desired current is linear versus amplitude
        int32_t desired_pa = (pa * (int64_t)target) / avg;
        int32_t correction = desired_pa - pa;

        // Note: the casting of unsigned constant is necessary to avoid overflow
        // because the product is not casted to int32_t but to unsigned!!
        correction = (correction * (int32_t)AGC_FAST_CORRECTION_PCT) / 100;

        if (correction > (int16_t)AGC_MAX_FAST_PA_STEP)
            correction = (int16_t)AGC_MAX_FAST_PA_STEP;
        else if (correction < -(int16_t)AGC_MAX_FAST_PA_STEP)
            correction = -(int16_t)AGC_MAX_FAST_PA_STEP;

        delta = correction;
    }

    /*
     * PA=0 is a special case: proportional scaling cannot move it.
     * The minimum 1-LSB correction lets the AGC recover from zero.
     */
    if (delta == 0)
        delta = (zone == 0u) ? 1 : -1;

    bool adjusted = apply_pa_delta(ch, dev, delta);

    if (adjusted) {
        ch->cooldown_remain = AGC_COOLDOWN_WINDOWS;
        ch->adj_count++;
        hw_write = true;

        PRINT_AGC_INFO("[AGC]  ch%u  avg=%ld  PA->%u  adj#%u\r\n",
               (unsigned)ch_idx, (long)avg,
               (unsigned)ch->drv[0].pa_cur,
               (unsigned)ch->adj_count);
    }

    return hw_write;
}

/**
 * @brief Use a majority decision across channels to decide if the device
 *        is off the body or on.
 *        Call from app.c if needed to send this status over BLE along with the data
 */
bool ppg_agc_is_off_body(const ppg_agc_t *agc)
{
    uint8_t off_count = 0u;
    for (uint8_t c = 0u; c < AGC_NUM_CHANNELS; c++) {
        if (!agc->ch[c].on_body) off_count++;
    }
    return (off_count >= AGC_OFFBODY_MIN_CH);
}

#ifdef UNUSED

/**
 * @brief Returns if a certain channel thinks sensor is on the body or not
 */
bool ppg_agc_channel_on_body(const ppg_agc_t *agc, uint8_t ch_idx)
{
    if (ch_idx >= AGC_NUM_CHANNELS) return false;
    return agc->ch[ch_idx].on_body;
}

/**
 * @brief Returns the current PA setting for drv[0] of a channel
 *        As it is same for all driver, the first one is used
 */
uint8_t ppg_agc_get_pa(const ppg_agc_t *agc, uint8_t ch_idx)
{
    if (ch_idx >= AGC_NUM_CHANNELS || agc->ch[ch_idx].num_drv == 0u) return 0u;
    return agc->ch[ch_idx].drv[0].pa_cur;
}

/**
 * @brief Enable/disable a channel
 */
void ppg_agc_set_enable(ppg_agc_t *agc, uint8_t ch_idx, bool enable)
{
    if (ch_idx >= AGC_NUM_CHANNELS) return;
    agc->ch[ch_idx].enabled = enable;
}

#endif

/**
 * @brief Force the PA (current) setting for an AGC channel, for all its drivers
 */
void ppg_agc_force_pa(ppg_agc_t *agc, max86171_t *dev,
                      uint8_t ch_idx, uint8_t pa_val)
{
    if (ch_idx >= AGC_NUM_CHANNELS) return;
    agc_channel_t *ch = &agc->ch[ch_idx];

    set_pa_absolute(ch, dev, pa_val);
    PRINT_AGC_INFO("[FORCE]  ch%u  PA->%u\r\n", (unsigned)ch_idx, (unsigned)pa_val);

    /* Reset window so stale accumulator is discarded */
    ch->window_sum      = 0;
    ch->window_cnt      = 0u;
    ch->cooldown_remain = AGC_COOLDOWN_WINDOWS;
}

/* End of file */
