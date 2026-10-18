/*
 * @file max86171.h
 * @brief STM32-specific driver for MAX86171 Optical AFE (SPI only)
 *
 * Updated with corrected register addresses from datasheet Rev 4
 * Optimized for STM32CubeIDE with HAL library
 * SPI: SPI1
 * CS:  PA4 (A_SPI0_CSOPT_N)
 *
 * * LED1_DRV - CT_DBLP31.12 -  LED3 - 536nm Green
 * * LED2_DRV - SFH_7016        -  LED2 - 660nm Red
 * * LED3_DRV - SFH_7016        -  LED2 - 950nm IR
 * * LED4_DRV - CT_DBLP31.12 -  LED4 - 536nm Green
 * * LED5_DRV - CT_DBLP31.12 -  LED4 - 536nm Green
 * * LED6_DRV - SFH_7016        -  LED1 - 660nm Red
 * * LED7_DRV- SFH_7016         -  LED1 - 950nm IR
 * * LED8_DRV- SFH_7016         -  LED1 - 526nm Green
 * * LED9_DRV- SFH_7016         -  LED2 - 526nm Green
 */

#ifndef MAX86171_H
#define MAX86171_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "app.h"
#include "max86171_regs.h"
#include "em_gpio.h"
#include "pin_config.h"

/* Bitmask flags for MEAS channel selection */
#define MAX86171_MEAS1_EN  (1U << 0)
#define MAX86171_MEAS2_EN  (1U << 1)
#define MAX86171_MEAS3_EN  (1U << 2)
#define MAX86171_MEAS4_EN  (1U << 3)
#define MAX86171_MEAS5_EN  (1U << 4)
#define MAX86171_MEAS6_EN  (1U << 5)
#define MAX86171_MEAS7_EN  (1U << 6)
#define MAX86171_MEAS8_EN  (1U << 7)
#define MAX86171_MEAS9_EN  (1U << 8)
#define MAX86171_MEAS_ALL  (0x1FFU)

/* Physical map:
*   LED1_DRV  CT_DBLP31.12  	LED3   536nm Green
*   LED2_DRV  SFH_7016         	LED2   660nm Red
*   LED3_DRV  SFH_7016         	LED2   950nm IR
*   LED4_DRV  CT_DBLP31.12  	LED4   536nm Green
*   LED5_DRV  CT_DBLP31.12  	LED4   536nm Green
*   LED6_DRV  SFH_7016      	LED1   660nm Red
*   LED7_DRV  SFH_7016      	LED1   950nm IR
*   LED8_DRV  SFH_7016      	LED1   526nm Green
*   LED9_DRV  SFH_7016     		LED2   526nm Green
* ========================================================================= */

/* ── DRVA select codes ────────────────────────────────────────────────────
*  Code  →  LEDx_DRV
*  0x00  →  LED1_DRV  (536 nm Green, CT_DBLP31.12)
*  0x01  →  LED2_DRV  (660 nm Red,   SFH_7016)
*  0x02  →  LED4_DRV  (536 nm Green, CT_DBLP31.12)
*  0x03  →  LED7_DRV  (950 nm IR,    SFH_7016)
* ----------------------------------------------------------------------- */
#define DRVA_GREEN_536_CT     0x00u   /* LED1_DRV */
#define DRVA_RED_660          0x01u   /* LED2_DRV */
#define DRVA_GREEN_536_CT2    0x02u   /* LED4_DRV */
#define DRVA_IR_950           0x03u   /* LED7_DRV */

/* ── DRVB select codes ────────────────────────────────────────────────────
*  Code  →  LEDx_DRV
*  0x00  →  LED2_DRV  (660 nm Red,   SFH_7016)
*  0x01  →  LED3_DRV  (950 nm IR,    SFH_7016)
*  0x02  →  LED5_DRV  (536 nm Green, CT_DBLP31.12)
*  0x03  →  LED8_DRV  (526 nm Green, SFH_7016)
* ----------------------------------------------------------------------- */
#define DRVB_RED_660          0x00u   /* LED2_DRV */
#define DRVB_IR_950           0x01u   /* LED3_DRV */
#define DRVB_GREEN_536_CT     0x02u   /* LED5_DRV */
#define DRVB_GREEN_526_SFH    0x03u   /* LED8_DRV */

/* ── DRVC select codes ────────────────────────────────────────────────────
*  Code  →  LEDx_DRV
*  0x00  →  LED1_DRV  (536 nm Green, CT_DBLP31.12)
*  0x01  →  LED3_DRV  (950 nm IR,    SFH_7016)
*  0x02  →  LED6_DRV  (660 nm Red,   SFH_7016)
*  0x03  →  LED9_DRV  (526 nm Green, SFH_7016)
* ----------------------------------------------------------------------- */
#define DRVC_GREEN_536_CT     0x00u   /* LED1_DRV */
#define DRVC_IR_950           0x01u   /* LED3_DRV */
#define DRVC_RED_660          0x02u   /* LED6_DRV */
#define DRVC_GREEN_526_SFH    0x03u   /* LED9_DRV */

/* =============================
 *        Versioning
 * ============================= */
#define MAX86171_DRV_VERSION_MAJOR 1
#define MAX86171_DRV_VERSION_MINOR 1
#define MAX86171_DRV_VERSION_PATCH 0

/* =============================
 *        Error Codes
 * ============================= */
#define MAX86171_OK                      (0)
#define MAX86171_E_NULL                  (-1)
#define MAX86171_E_BUS                   (-2)
#define MAX86171_E_TIMEOUT               (-3)
#define MAX86171_E_ARG                   (-4)
#define MAX86171_E_STATE                 (-5)

/* =============================
 *        Device Handle
 * ============================= */
// This is not used at all yet struct is passed to every driver function.
// It has been retained as a dummy to avoid large-scale code changes.
typedef struct {
    uint8_t dummy;
} max86171_t;

#define MAX86171_BASE_CONFIG_SIZE  (27) // Everything except MEAS blocks
#define MAX86171_MEAS_REGS         7    // Size of one MEAS block
#define MAX86171_NUM_MEAS          9    // Use all 9 available measurements
#define MAX86171_NUM_PPG           2    // PPG1 + PPG2
#define MAX86171_NUM_FRAMES        9    // How often to have an interrupt
#define MAX_FIFO_SAMPLES           (MAX86171_NUM_MEAS*MAX86171_NUM_PPG*MAX86171_NUM_FRAMES)
                                        // Note: This assumes MAX86171_MEAS_ALL is used
/**
 * @brief Register configuration entry
 */
typedef struct {
    uint8_t address;    // Register address
    uint8_t value;      // Register value
} max86171_reg_config_t;

extern const max86171_reg_config_t g_max86171_base_config[MAX86171_BASE_CONFIG_SIZE];
extern max86171_reg_config_t g_max86171_meas_config[MAX86171_NUM_MEAS][MAX86171_MEAS_REGS];

/* =============================
 *        Public Types
 * ============================= */

/* LED range - From datasheet page 62 */
typedef enum {
    MAX86171_LED_RANGE_32mA = 0,
    MAX86171_LED_RANGE_64mA = 1,
    MAX86171_LED_RANGE_96mA = 2,
    MAX86171_LED_RANGE_128mA = 3
} max86171_led_range_t;

/* ADC range - From datasheet page 63 */
typedef enum {
    MAX86171_ADC_RANGE_4uA = 0,
    MAX86171_ADC_RANGE_8uA = 1,
    MAX86171_ADC_RANGE_16uA = 2,
    MAX86171_ADC_RANGE_32uA = 3
} max86171_adc_range_t;

/* Integration time - From datasheet page 62 */
typedef enum {
    MAX86171_TINT_14_6us = 0,
    MAX86171_TINT_29_2us = 1,
    MAX86171_TINT_58_6us = 2,
    MAX86171_TINT_117_1us = 3
} max86171_tint_t;

/* PD bias - From datasheet page 56-57 */
typedef enum {
    MAX86171_PD_BIAS_NOT_RECOMMENDED = 0,
    MAX86171_PD_BIAS_0_125pF = 1,         // 0-125pF (default)
    MAX86171_PD_BIAS_125_250pF = 2,       // 125-250pF
    MAX86171_PD_BIAS_250_500pF = 3        // 250-500pF
} max86171_pd_bias_t;

/* PD settling time - From datasheet page 64 */
typedef enum {
    MAX86171_PD_SETLNG_6_1us = 0,
    MAX86171_PD_SETLNG_12_1us = 1,
    MAX86171_PD_SETLNG_18_1us = 2,
    MAX86171_PD_SETLNG_24_1us = 3
} max86171_pd_setlng_t;

/* LED settling time - From datasheet page 64 */
typedef enum {
    MAX86171_LED_SETLNG_6_us = 0,
    MAX86171_LED_SETLNG_12_us = 1,
    MAX86171_LED_SETLNG_18_us = 2,
    MAX86171_LED_SETLNG_24_us = 3
} max86171_led_setlng_t;

/* DAC offset - From datasheet page 64 */
typedef enum {
    MAX86171_DACOFF_0_uA = 0,
    MAX86171_DACOFF_8_uA = 1,
    MAX86171_DACOFF_16_uA = 2,
    MAX86171_DACOFF_24_uA = 3
} max86171_dacoff_t;

/* Ambient light reject method - From datasheet page 63 */
typedef enum {
    MAX86171_FILT_SEL_CDM = 0,
    MAX86171_FILT_SEL_FDM = 1,
} max86171_filt_sel_t;

/* SINC3 filter selection - From datasheet pages 62-63 */
typedef enum {
    MAX86171_SINC3_SEL_OFF = 0,
    MAX86171_SINC3_SEL_ON = 1,
} max86171_sinc3_sel_t;

/* PPG1 PD selection - From datasheet pages 61-62 */
typedef enum {
    MAX86171_PPG1_PDSEL_PD1 = 0,
    MAX86171_PPG1_PDSEL_PD3 = 1,
} max86171_ppg1_pdsel_t;

/* PPG2 PD selection - From datasheet pages 61-62 */
typedef enum {
    MAX86171_PPG2_PDSEL_PD2 = 0,
    MAX86171_PPG2_PDSEL_PD4 = 1,
} max86171_ppg2_pdsel_t;

/* Direct ambient measurement selection - From datasheet page 60 */
typedef enum {
    MAX86171_MEAS_AMB_NORMAL = 0,
    MAX86171_MEAS_AMB_AMBIENT = 1,
} max86171_meas_amb_sel_t;

/* Measurement configuration */
typedef struct {
    uint8_t drvA_sel;            // LED mux select (0-3), see datasheet page 60
    uint8_t drvB_sel;            // LED mux select (0-3)
    uint8_t drvC_sel;            // LED mux select (0-3)
    uint8_t drvA_pa;             // LED current code (0-255)
    uint8_t drvB_pa;             // LED current code (0-255)
    uint8_t drvC_pa;             // LED current code (0-255)
    max86171_led_range_t led_range;
    max86171_adc_range_t ppg1_adc;
    max86171_adc_range_t ppg2_adc;
    max86171_tint_t tint;
    uint8_t ppg1_pdsel;          // PD index (0=PD1, 1=PD3)
    uint8_t ppg2_pdsel;          // PD index (0=PD2, 1=PD4)
    uint8_t averages;            // Number of averages (0-7): 2^n pulses
    bool use_cdm;                // true=CDM (Central Diff), false=FDM (Forward Diff)
    bool ambient_direct;         // Ambient measurement mode
} max86171_meas_cfg_t;

/* IRQ status */
typedef struct {
    bool int1;
    bool int2;
    uint32_t status1;
    uint32_t status2;
    uint32_t status3;
} max86171_irq_status_t;

/* FIFO tag values - From datasheet page 23 */
typedef enum {
    MAX86171_TAG_INVALID   = 0x0,
    MAX86171_TAG_MEAS1     = 0x1,
    MAX86171_TAG_MEAS2     = 0x2,
    MAX86171_TAG_MEAS3     = 0x3,
    MAX86171_TAG_MEAS4     = 0x4,
    MAX86171_TAG_MEAS5     = 0x5,
    MAX86171_TAG_MEAS6     = 0x6,
    MAX86171_TAG_MEAS7     = 0x7,
    MAX86171_TAG_MEAS8     = 0x8,
    MAX86171_TAG_MEAS9     = 0x9,
    MAX86171_TAG_DARK      = 0xA,
    MAX86171_TAG_ALC_OVF   = 0xB,
    MAX86171_TAG_EXP_OVF   = 0xC,
    MAX86171_TAG_PICKET    = 0xD,
    MAX86171_TAG_EMPTY     = 0xE
} max86171_tag_t;

/* Synchronization modes - From datasheet page 17 */
typedef enum {
    MAX86171_SYNC_INTERNAL = 0,        // Internal oscillator
    MAX86171_SYNC_EXTERNAL_TRIG = 1,   // External trigger on TRIG pin
    MAX86171_SYNC_EXTERNAL_CLOCK = 2   // External clock on TRIG pin
} max86171_sync_mode_t;

/* =============================
 *        Public API
 * ============================= */

/* Initialization */
int max86171_init(max86171_t *dev);
int max86171_soft_reset(max86171_t *dev);

/* FIFO operations */
int max86171_fifo_flush(max86171_t *dev);

/* Interrupts */
int max86171_status_read_clear(max86171_t *dev);

/* Low-level register access */
int max86171_write_reg(max86171_t *dev, uint8_t reg, const uint8_t *data, size_t len);
int max86171_read_reg(max86171_t *dev, uint8_t reg, uint8_t *data, size_t len);
int max86171_read_FIFO(max86171_t *dev, uint8_t *data, size_t len) ;

/* Helper functions */
static inline void max86171_cs_low(void) {
  GPIO_PinOutClear(CS_OPT_PORT, CS_OPT_PIN);
}

static inline void max86171_cs_high(void) {
  GPIO_PinOutSet(CS_OPT_PORT, CS_OPT_PIN);
}

/* Sign-extend 20-bit two's complement to int32 */
static inline int32_t max86171_sext20(uint32_t v20) {
    v20 &= 0xFFFFFU;
    if (v20 & (1U << 19)) {
        v20 |= 0xFFF00000U;
    }
    return (int32_t)v20;
}

/*
 * @brief Load configuration from table into MAX86171 device
 */
int max86171_load_config_from_table(max86171_t *dev, uint16_t meas_mask);

/*
 * @brief Verify loaded configuration by reading back registers
 */
int max86171_verify_config(max86171_t *dev,
                           uint16_t meas_mask,
                           uint8_t *mismatches,
                           size_t max_mismatches,
                           size_t *mismatch_count);

/*
 * @brief Get number of registers in configuration table
 */
size_t max86171_get_config_size(void);

/*
 * @brief Get register configuration entry by index
 */
int max86171_get_config_entry(size_t index, uint8_t *addr, uint8_t *value);

/*
 * @brief Set register configuration entry by index
 */
int max86171_set_config_current(size_t index, uint8_t value);

 /**
  * @brief Shutdown the device
  */
int max86171_shutdown(max86171_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* MAX86171_H */
