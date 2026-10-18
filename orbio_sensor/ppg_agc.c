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
 *  │  if   avg < AGC_THRESH_LOW  → PA += AGC_PA_STEP                    │
 *  │  elif avg > AGC_THRESH_HIGH → PA -= AGC_PA_STEP                    │
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
static bool apply_pa_delta(agc_channel_t *ch, max86171_t *dev, int8_t delta)
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

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * @brief Initialise the AGC struct. Currently called with PPG is started.
 */
void ppg_agc_init(ppg_agc_t *agc)
{
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
bool ppg_agc_feed(ppg_agc_t *agc, max86171_t *dev, uint8_t ch_idx, const int32_t raw)
{
    if (ch_idx >= AGC_NUM_CHANNELS) return false;
    agc_channel_t *ch = &agc->ch[ch_idx];

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

    bool hw_write = false;

    /* =====================================================================
     * PROXIMITY CHECK  (runs regardless of AGC enable flag)
     * ===================================================================== */
    if (avg < AGC_THRESH_PROX) {
        /* Signal is very low — possible off-body */
        ch->prox_low_cnt++;
        if (ch->prox_low_cnt > AGC_PROX_CONFIRM_WINDOWS) {
            ch->prox_low_cnt = AGC_PROX_CONFIRM_WINDOWS;   /* saturate */
        }
        ch->prox_high_cnt = 0u;

        if (ch->on_body && ch->prox_low_cnt >= AGC_PROX_CONFIRM_WINDOWS) {
            /* Transition: ON-BODY → OFF-BODY */
            ch->on_body = false;
            set_pa_absolute(ch, dev, AGC_PROX_IDLE_PA);
            ch->cooldown_remain = 0u;   /* let AGC act on very next window */
            hw_write = true;

            PRINT_AGC_INFO("[PROX] ch%u  OFF-BODY  avg=%ld  PA->%u\r\n",
                   (unsigned)ch_idx, (long)avg, (unsigned)AGC_PROX_IDLE_PA);
        }
    } else {
        /* Signal above proximity threshold */
        ch->prox_high_cnt++;
        if (ch->prox_high_cnt > AGC_PROX_RECOVER_WINDOWS) {
            ch->prox_high_cnt = AGC_PROX_RECOVER_WINDOWS;
        }
        ch->prox_low_cnt = 0u;

        if (!ch->on_body && ch->prox_high_cnt >= AGC_PROX_RECOVER_WINDOWS) {
            /* Transition: OFF-BODY → ON-BODY */
            ch->on_body = true;
            set_pa_absolute(ch, dev, AGC_PROX_RESTART_PA);
            ch->cooldown_remain = 0u;   /* let AGC act on very next window */
            hw_write = true;

            PRINT_AGC_INFO("[PROX] ch%u  ON-BODY   avg=%ld  PA->%u\r\n",
                   (unsigned)ch_idx, (long)avg, (unsigned)AGC_PROX_RESTART_PA);
        }
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
    if      (avg < AGC_THRESH_LOW)  zone = 0u;   /* too dim  → raise PA */
    else if (avg > AGC_THRESH_HIGH) zone = 2u;   /* too bright → lower PA */
    else                            zone = 1u;   /* in target band        */

    ch->agc_state = zone;

    /* Respect cooldown */
    if (ch->cooldown_remain > 0u) {
        ch->cooldown_remain--;
        return hw_write;
    }

    /* In range: nothing to do */
    if (zone == 1u) return hw_write;

    /* Apply adjustment */
    int8_t delta = (zone == 0u) ? (int8_t)AGC_PA_STEP : -(int8_t)AGC_PA_STEP;
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
 *        TODO: Call from app.c if needed to send this status over BLE along with the data
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

/**
 * @brief Force the PA (current) setting for an AGC channel, for all its drivers
 */
void ppg_agc_force_pa(ppg_agc_t *agc, max86171_t *dev,
                      uint8_t ch_idx, uint8_t pa_val)
{
    if (ch_idx >= AGC_NUM_CHANNELS) return;
    agc_channel_t *ch = &agc->ch[ch_idx];

    set_pa_absolute(ch, dev, pa_val);

    /* Reset window so stale accumulator is discarded */
    ch->window_sum      = 0;
    ch->window_cnt      = 0u;
    ch->cooldown_remain = AGC_COOLDOWN_WINDOWS;
}

#endif

/* End of file */
