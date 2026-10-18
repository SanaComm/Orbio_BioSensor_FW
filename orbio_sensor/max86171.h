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

#define MAX86171_BASE_CONFIG_SIZE  (sizeof(g_max86171_base_config) / sizeof(g_max86171_base_config[0]))
#define MAX86171_MEAS_REGS         7                         // registers per MEAS block
#define MAX86171_NUM_MEAS          9
#define MAX86171_NUM_PPG           2
#define MAX86171_NUM_FRAMES        10
#define MAX_FIFO_SAMPLES           (MAX86171_NUM_MEAS*MAX86171_NUM_PPG*MAX86171_NUM_FRAMES)
/**
 * @brief Register configuration entry
 */
typedef struct {
    uint8_t address;    // Register address
    uint8_t value;      // Register value
} max86171_reg_config_t;

/* Base config: everything except MEAS blocks */
static const max86171_reg_config_t g_max86171_base_config[] = {

    // FIFO Configuration
    {MAX86171_REG_FIFO_RD_PTR,  0x00},                    /* Reset read pointer */
    {MAX86171_REG_FIFO_CFG1,    256 - MAX_FIFO_SAMPLES},  /* A_FULL threshold */
    {MAX86171_REG_FIFO_CFG2,    /* 0x0A = 0x10 */
        MAX86171_F_MAKE(MAX86171_FIFO_RO,           0) |  /* no rollover */
        MAX86171_F_MAKE(MAX86171_FIFO_A_FULL_TYPE,  0) |  /* A_FULL = repeat */
        MAX86171_F_MAKE(MAX86171_FIFO_STAT_CLR,     0) |  /* stat clear disabled */
        MAX86171_F_MAKE(MAX86171_FIFO_FLUSH,        1)},  /* flush FIFO on init */

    // System Configuration
	{MAX86171_REG_SYS_CFG1,     /* 0x0C = 0x02 */
        MAX86171_F_MAKE(MAX86171_SYS_RESET,         0) |  /* no reset */
        MAX86171_F_MAKE(MAX86171_SYS_SHDN,          1) |  /* start in shutdown */
        MAX86171_F_MAKE(MAX86171_SYS_PPG1_PWRDN,    0) |  /* PPG1 active */
        MAX86171_F_MAKE(MAX86171_SYS_PPG2_PWRDN,    0) |  /* PPG2 active */
        MAX86171_F_MAKE(MAX86171_SYS_SYNC_MODE,     0) |  /* no sync */
        MAX86171_F_MAKE(MAX86171_SYS_SW_FORCE_SYNC, 0) |  /* no forced sync */
        MAX86171_F_MAKE(MAX86171_SYS_MEAS9_EN,      0)},  /* MEAS9 disabled */

    {MAX86171_REG_SYS_CFG3,     0x00},                              /* defaults */

    {MAX86171_REG_PD_BIAS,                                              /* 0x0F = 0x55 */
        MAX86171_F_MAKE(MAX86171_PD1_BIAS,          1) |  /* PD1 bias = 65pF */
        MAX86171_F_MAKE(MAX86171_PD2_BIAS,          1) |  /* PD2 bias = 65pF */
        MAX86171_F_MAKE(MAX86171_PD3_BIAS,          1) |  /* PD3 bias = 65pF */
        MAX86171_F_MAKE(MAX86171_PD4_BIAS,          1)},  /* PD4 bias = 65pF */

    // Pin Configuration
    {MAX86171_REG_PIN_FUNC_CFG,     0x00},                /* defaults */
    {MAX86171_REG_OUTPUT_PIN_CFG,   0x00},                /* defaults */

    // Frame Rate Clock Configuration (100 Hz)
    // Bugfix: Was incorrectly calculated based on 32.768kHz but was actually set to 32kHz
    // FR_CLK = 32000 Hz / DIV → DIV=320 → 32000/320 = 100 Hz
    // At 100Hz frame rate, this means 9 measurements, including averaging, and ambient cancelling
    // have to fit within 10 msec.
    // 6 channels have MEAS_AVER = 2, 2 channels have MEAS_AVER = 1, 1 channel has MEAS_AVER = 0
    // So number of pulse will be 6*(2*2^2+1)+2*(2*2^1+1)+1*(2*2^0+1) = 67
    // MEAS_TINT = 117.1 us, MEAS_PD_SETLNG = 12.1 us, giving total pulse width = 117.1+12.1 = 129.2 us
    // 67 pulses * 129.2 us per pulse = 8.656 msec
    // Add tINIT1 = 7 * tCLOCK at the start of a frame and tINIT = 3 * tCLOCK for each measurement
    // = 7 / 32000 + (9 * 3) / 32000 = 34/32000 = 1.0625 msec
    // TOTAL = 8.656+1.0625 = 9.7185 msec, which fits in the frame time of 10 msec
    {MAX86171_REG_FR_CLK_FREQ_SEL,  /* 0x15 = 0x00 */
        MAX86171_F_MAKE(MAX86171_FR_CLK_FINE_TUNE,  0) |  /* no fine tune */
        MAX86171_F_MAKE(MAX86171_FR_CLK_SEL,        0)},  /* internal 32kHz */

    {MAX86171_REG_FR_CLK_DIV_MSB,   /* 0x16 = 0x01 */
        MAX86171_F_MAKE(MAX86171_FR_CLK_DIV_H,      0x01)},  /* divider MSB */

    {MAX86171_REG_FR_CLK_DIV_LSB,   /* 0x17 = 0x40 */
        MAX86171_F_MAKE(MAX86171_FR_CLK_DIV_L,      0x40)},  /* divider LSB → total=0x0140=320 */

    // Threshold Configuration
    {MAX86171_REG_THRESH_MEAS_SEL,  0x00},  /* threshold on MEAS1 */
    {MAX86171_REG_THRESH_HYST,          0x00},  /* no hysteresis */
    {MAX86171_REG_THRESH1_HI,             0xFF},  /* HI threshold 1 = max */
    {MAX86171_REG_THRESH1_LO,            0x00},  /* LO threshold 1 = min */
    {MAX86171_REG_THRESH2_HI,             0xFF},  /* HI threshold 2 = max */
    {MAX86171_REG_THRESH2_LO,            0x00},  /* LO threshold 2 = min */

    // Picket Fence
    {MAX86171_REG_PF_MEAS_SEL,      0x00},  /* PF on MEAS1 */
    {MAX86171_REG_PF_CFG,               0x40},  /* PF enabled, default config */

    // Interrupt Configuration
    {MAX86171_REG_INT1_EN1,     /* 0x78 = 0x80 */
        MAX86171_F_MAKE(MAX86171_INT_LED_TX,        0) |
        MAX86171_F_MAKE(MAX86171_INT_THRESH1_HILO,  0) |
        MAX86171_F_MAKE(MAX86171_INT_THRESH2_HILO,  0) |
        MAX86171_F_MAKE(MAX86171_INT_EXP_OVF,       0) |
        MAX86171_F_MAKE(MAX86171_INT_ALC_OVF,       0) |
        MAX86171_F_MAKE(MAX86171_INT_FIFO_DATA_RDY, 0) |
        MAX86171_F_MAKE(MAX86171_INT_FRAME_RDY,     0) |
        MAX86171_F_MAKE(MAX86171_INT_A_FULL,        1)},  /* A_FULL interrupt only */

    {MAX86171_REG_INT1_EN2,     0x00},  /* INT1 EN2 all disabled */
    {MAX86171_REG_INT1_EN3,     0x00},  /* INT1 EN3 all disabled */
    {0x7B,                      0x00},  /* reserved / pad */
    {MAX86171_REG_INT2_EN1,     0x00},  /* INT2 all disabled */
    {MAX86171_REG_INT2_EN2,     0x00},
    {MAX86171_REG_INT2_EN3,     0x00},
    {0x7F,                      0x00},  /* reserved / pad */
};

/*
 * * LED1_DRV - CT_DBLP31.12 - LED3 - 536nm Green
 * * LED2_DRV - SFH_7016     - LED2 - 660nm Red
 * * LED3_DRV - SFH_7016     - LED2 - 950nm IR
 * * LED4_DRV - CT_DBLP31.12 - LED4 - 536nm Green
 * * LED5_DRV - CT_DBLP31.12 - LED4 - 536nm Green
 * * LED6_DRV - SFH_7016     - LED1 - 660nm Red
 * * LED7_DRV - SFH_7016     - LED1 - 950nm IR
 * * LED8_DRV - SFH_7016     - LED1 - 526nm Green
 * * LED9_DRV - SFH_7016     - LED2 - 526nm Green
 */

/*
MEASx_DRVA       		MEASx_DRVB       		    MEASx_DRVC
Code  LEDx_DRV Pin 	    Code	LEDx_DRV Pin		Code	LEDx_DRV Pin
00 	LED1_DRV			00		LED2_DRV			00		LED1_DRV
01 	LED2_DRV			01		LED3_DRV			01		LED3_DRV
10 	LED4_DRV 			10 		LED5_DRV 			10     	LED6_DRV
11 	LED7_DRV 			11      LED8_DRV			11		LED9_DRV
*/

/*
 * TODO: configuration doubts
 * 1.  Why not set SINC3 = 1 as we have TINT = 117.1us which allows this setting
 * 2.  Why do MEAS7/MEAS8 have MEAS_AVER = 1 and not 2 which will still fit within overall frame timing
 * 3.  What is the logic behind the initial current settings here, given they will change via AGC?
 * 4.  What is the logic behind PPG1/PPG2 ADC full range settings of 16uA?
 * 5.  Why do MEAS3/MEAS4 drive the same IR LED3 with two DRVx instead of two different IR LEDs?
 * 6.  The current values given in comments for every DRVx setting do not match calculations.
 */

static const max86171_reg_config_t g_max86171_meas_config[MAX86171_NUM_MEAS][MAX86171_MEAS_REGS] = {

// MEAS1 [0] — 536nm Green CT (center)
{
    {MAX86171_REG_MEAS1_SELECTS,
        MAX86171_F_MAKE(MAX86171_MEAS_DRVA_SEL, DRVA_GREEN_536_CT)  |  /* LED1_DRV – 536nm Green CT */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVB_SEL, DRVB_GREEN_536_CT)  |  /* LED5_DRV – 536nm Green CT */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVC_SEL, DRVC_GREEN_536_CT)  |  /* LED1_DRV – 536nm Green CT */
        MAX86171_F_MAKE(MAX86171_MEAS_AMB,      0)},

    {MAX86171_REG_MEAS1_CFG1,
        MAX86171_F_MAKE(MAX86171_MEAS_AVER,         2)  | /* A total of 9 interleaved dark and exposure measurements are taken and average is loaded to FIFO */
        MAX86171_F_MAKE(MAX86171_MEAS_TINT,         3)  | /* Measurement integration time = 117.1us */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_PDSEL,   0)  | /* PD1 / PD2 selected as inputs */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_PDSEL,   0)},

    {MAX86171_REG_MEAS1_CFG2,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_ADC_RGE, 2) | /* PPG1 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_ADC_RGE, 2) | /* PPG2 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_RGE,      3) | /* LED full scale range 128 mA */
        MAX86171_F_MAKE(MAX86171_MEAS_FILT_SEL,     0) | /* Ambient light estimation by CDM (Central Difference Method) */
        MAX86171_F_MAKE(MAX86171_MEAS_SINC3_SEL,    0)}, /* SINC3 filter not used */

    {MAX86171_REG_MEAS1_CFG3,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_DACOFF,  0) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_DACOFF,  0) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_SETLNG,   1) | /* Default 12us */
        MAX86171_F_MAKE(MAX86171_MEAS_PD_SETLNG,    1)}, /* Default 12.1us */

    {MAX86171_REG_MEAS1_DRVA_PA,   0x00u},  /* LED1_DRV disabled   */
    {MAX86171_REG_MEAS1_DRVB_PA,   0x04u},  /* LED5_DRV ~0.12mA    */
    {MAX86171_REG_MEAS1_DRVC_PA,   0x04u},  /* LED1_DRV ~0.12mA    */
},

// MEAS2 [1] — 536nm Green CT (center)
{
    {MAX86171_REG_MEAS2_SELECTS,
        MAX86171_F_MAKE(MAX86171_MEAS_DRVA_SEL, DRVA_GREEN_536_CT)  |  /* LED1_DRV – 536nm Green CT */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVB_SEL, DRVB_GREEN_536_CT)  |  /* LED5_DRV – 536nm Green CT */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVC_SEL, DRVC_GREEN_536_CT)  |  /* LED1_DRV – 536nm Green CT */
        MAX86171_F_MAKE(MAX86171_MEAS_AMB,      0)},

    {MAX86171_REG_MEAS2_CFG1,
        MAX86171_F_MAKE(MAX86171_MEAS_AVER,         2) | /* A total of 9 interleaved dark and exposure measurements are taken and average is loaded to FIFO */
        MAX86171_F_MAKE(MAX86171_MEAS_TINT,         3) | /* Measurement integration time = 117.1us */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_PDSEL,   1) | /* PD3 / PD4 selected as inputs  */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_PDSEL,   1)},

    {MAX86171_REG_MEAS2_CFG2,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_ADC_RGE, 2) | /* PPG1 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_ADC_RGE, 2) | /* PPG2 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_RGE,      3) | /* LED full scale range 128 mA */
        MAX86171_F_MAKE(MAX86171_MEAS_FILT_SEL,     0) | /* Ambient light estimation by CDM (Central Difference Method) */
        MAX86171_F_MAKE(MAX86171_MEAS_SINC3_SEL,    0)}, /* SINC3 filter not used */

    {MAX86171_REG_MEAS2_CFG3,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_DACOFF,  0) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_DACOFF,  0) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_SETLNG,   1) | /* Default 12us */
        MAX86171_F_MAKE(MAX86171_MEAS_PD_SETLNG,    1)}, /* Default 12.1us */

        {MAX86171_REG_MEAS2_DRVA_PA,   0x00u},  /* LED1_DRV disabled   */
        {MAX86171_REG_MEAS2_DRVB_PA,   0x04u},  /* LED5_DRV ~0.12mA    */
        {MAX86171_REG_MEAS2_DRVC_PA,   0x04u},  /* LED1_DRV ~0.12mA    */
},

// MEAS3 [2] — 950nm IR
{
    {MAX86171_REG_MEAS3_SELECTS,
        MAX86171_F_MAKE(MAX86171_MEAS_DRVA_SEL, DRVA_IR_950)        |  /* LED7_DRV – 950nm IR SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVB_SEL, DRVB_IR_950)        |  /* LED3_DRV – 950nm IR SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVC_SEL, DRVC_IR_950)        |  /* LED3_DRV – 950nm IR SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_AMB,      0)},

    {MAX86171_REG_MEAS3_CFG1,
        MAX86171_F_MAKE(MAX86171_MEAS_AVER,         2) | /* A total of 9 interleaved dark and exposure measurements are taken and average is loaded to FIFO */
        MAX86171_F_MAKE(MAX86171_MEAS_TINT,         3) | /* Measurement integration time = 117.1us */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_PDSEL,   0) | /* PD1 / PD2 selected as inputs  */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_PDSEL,   0)},

    {MAX86171_REG_MEAS3_CFG2,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_ADC_RGE, 2) | /* PPG1 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_ADC_RGE, 2) | /* PPG2 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_RGE,      3) | /* LED full scale range 128 mA */
        MAX86171_F_MAKE(MAX86171_MEAS_FILT_SEL,     0) | /* Ambient light estimation by CDM (Central Difference Method) */
        MAX86171_F_MAKE(MAX86171_MEAS_SINC3_SEL,    0)}, /* SINC3 filter not used */

    {MAX86171_REG_MEAS3_CFG3,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_DACOFF,  0) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_DACOFF,  0) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_SETLNG,   1) | /* Default 12us */
        MAX86171_F_MAKE(MAX86171_MEAS_PD_SETLNG,    1)}, /* Default 12.1us */

    {MAX86171_REG_MEAS3_DRVA_PA,   0x00u},  /* LED7_DRV disabled   */
    {MAX86171_REG_MEAS3_DRVB_PA,   0x06u},  /* LED3_DRV ~0.24mA    */
    {MAX86171_REG_MEAS3_DRVC_PA,   0x06u},  /* LED3_DRV ~0.24mA    */
},

// MEAS4 [3] — 950nm IR
{
    {MAX86171_REG_MEAS4_SELECTS,
        MAX86171_F_MAKE(MAX86171_MEAS_DRVA_SEL, DRVA_IR_950)        |  /* LED7_DRV – 950nm IR SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVB_SEL, DRVB_IR_950)        |  /* LED3_DRV – 950nm IR SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVC_SEL, DRVC_IR_950)        |  /* LED3_DRV – 950nm IR SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_AMB,      0)},

    {MAX86171_REG_MEAS4_CFG1,
        MAX86171_F_MAKE(MAX86171_MEAS_AVER,         2) | /* A total of 9 interleaved dark and exposure measurements are taken and average is loaded to FIFO */
        MAX86171_F_MAKE(MAX86171_MEAS_TINT,         3) | /* Measurement integration time = 117.1us */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_PDSEL,   1) | /* PD3 / PD4 selected as inputs  */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_PDSEL,   1)},

    {MAX86171_REG_MEAS4_CFG2,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_ADC_RGE, 2) | /* PPG1 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_ADC_RGE, 2) | /* PPG2 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_RGE,      3) | /* LED full scale range 128 mA */
        MAX86171_F_MAKE(MAX86171_MEAS_FILT_SEL,     0) | /* Ambient light estimation by CDM (Central Difference Method) */
        MAX86171_F_MAKE(MAX86171_MEAS_SINC3_SEL,    0)}, /* SINC3 filter not used */

    {MAX86171_REG_MEAS4_CFG3,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_DACOFF,  0) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_DACOFF,  0) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_SETLNG,   1) | /* Default 12us */
        MAX86171_F_MAKE(MAX86171_MEAS_PD_SETLNG,    1)}, /* Default 12.1us */

    {MAX86171_REG_MEAS4_DRVA_PA,   0x00u},  /* LED7_DRV disabled   */
    {MAX86171_REG_MEAS4_DRVB_PA,   0x06u},  /* LED3_DRV ~0.24mA    */
    {MAX86171_REG_MEAS4_DRVC_PA,   0x06u},  /* LED3_DRV ~0.24mA    */
},

// MEAS5 [4] — 660nm Red
{
    {MAX86171_REG_MEAS5_SELECTS,
        MAX86171_F_MAKE(MAX86171_MEAS_DRVA_SEL, DRVA_RED_660)       |  /* LED2_DRV – 660nm Red SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVB_SEL, DRVB_RED_660)       |  /* LED2_DRV – 660nm Red SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVC_SEL, DRVC_RED_660)       |  /* LED6_DRV – 660nm Red SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_AMB,      0)},

    {MAX86171_REG_MEAS5_CFG1,
        MAX86171_F_MAKE(MAX86171_MEAS_AVER,         2) | /* A total of 9 interleaved dark and exposure measurements are taken and average is loaded to FIFO */
        MAX86171_F_MAKE(MAX86171_MEAS_TINT,         3) | /* Measurement integration time = 117.1us */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_PDSEL,   0) | /* PD1 / PD2 selected as inputs  */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_PDSEL,   0)},

    {MAX86171_REG_MEAS5_CFG2,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_ADC_RGE, 2) | /* PPG1 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_ADC_RGE, 2) | /* PPG2 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_RGE,      3) | /* LED full scale range 128 mA */
        MAX86171_F_MAKE(MAX86171_MEAS_FILT_SEL,     0) | /* Ambient light estimation by CDM (Central Difference Method) */
        MAX86171_F_MAKE(MAX86171_MEAS_SINC3_SEL,    0)}, /* SINC3 filter not used */

    {MAX86171_REG_MEAS5_CFG3,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_DACOFF,  0) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_DACOFF,  0) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_SETLNG,   1) | /* Default 12us */
        MAX86171_F_MAKE(MAX86171_MEAS_PD_SETLNG,    1)}, /* Default 12.1us */

    {MAX86171_REG_MEAS5_DRVA_PA,   0x05u},  /* LED2_DRV ~0.36mA    */
    {MAX86171_REG_MEAS5_DRVB_PA,   0x00u},  /* LED2_DRV disabled   */
    {MAX86171_REG_MEAS5_DRVC_PA,   0x05u},  /* LED6_DRV ~0.36mA    */
},

// MEAS6 [5] — 660nm Red
{
    {MAX86171_REG_MEAS6_SELECTS,
        MAX86171_F_MAKE(MAX86171_MEAS_DRVA_SEL, DRVA_RED_660)       |  /* LED2_DRV – 660nm Red SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVB_SEL, DRVB_RED_660)       |  /* LED2_DRV – 660nm Red SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVC_SEL, DRVC_RED_660)       |  /* LED6_DRV – 660nm Red SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_AMB,      0)},

    {MAX86171_REG_MEAS6_CFG1,
        MAX86171_F_MAKE(MAX86171_MEAS_AVER,         2) | /* A total of 9 interleaved dark and exposure measurements are taken and average is loaded to FIFO */
        MAX86171_F_MAKE(MAX86171_MEAS_TINT,         3) | /* Measurement integration time = 117.1us */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_PDSEL,   1) | /* PD3 / PD4 selected as inputs  */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_PDSEL,   1)},

    {MAX86171_REG_MEAS6_CFG2,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_ADC_RGE, 2) | /* PPG1 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_ADC_RGE, 2) | /* PPG2 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_RGE,      3) | /* LED full scale range 128 mA */
        MAX86171_F_MAKE(MAX86171_MEAS_FILT_SEL,     0) | /* Ambient light estimation by CDM (Central Difference Method) */
        MAX86171_F_MAKE(MAX86171_MEAS_SINC3_SEL,    0)}, /* SINC3 filter not used */

    {MAX86171_REG_MEAS6_CFG3,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_DACOFF,  0) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_DACOFF,  0) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_SETLNG,   1) | /* Default 12us */
        MAX86171_F_MAKE(MAX86171_MEAS_PD_SETLNG,    1)}, /* Default 12.1us */

    {MAX86171_REG_MEAS6_DRVA_PA,   0x05u},  /* LED2_DRV ~0.36mA    */
    {MAX86171_REG_MEAS6_DRVB_PA,   0x00u},  /* LED2_DRV disabled   */
    {MAX86171_REG_MEAS6_DRVC_PA,   0x05u},  /* LED6_DRV ~0.36mA    */
},

// MEAS7 [6] — 526nm Green SFH (outer)
{
    {MAX86171_REG_MEAS7_SELECTS,
        MAX86171_F_MAKE(MAX86171_MEAS_DRVA_SEL, DRVA_GREEN_536_CT)  |  /* LED1_DRV – 536nm Green CT  */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVB_SEL, DRVB_GREEN_526_SFH) |  /* LED8_DRV – 526nm Green SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVC_SEL, DRVC_GREEN_526_SFH) |  /* LED9_DRV – 526nm Green SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_AMB,      0)},

    {MAX86171_REG_MEAS7_CFG1,
        MAX86171_F_MAKE(MAX86171_MEAS_AVER,         1) | /* A total of 5 interleaved dark and exposure measurements are taken and average is loaded to FIFO */
        MAX86171_F_MAKE(MAX86171_MEAS_TINT,         3) | /* Measurement integration time = 117.1us */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_PDSEL,   0) | /* PD1 / PD2 selected as inputs  */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_PDSEL,   0)},

    {MAX86171_REG_MEAS7_CFG2,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_ADC_RGE, 2) | /* PPG1 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_ADC_RGE, 2) | /* PPG2 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_RGE,      3) | /* LED full scale range 128 mA */
        MAX86171_F_MAKE(MAX86171_MEAS_FILT_SEL,     0) | /* Ambient light estimation by CDM (Central Difference Method) */
        MAX86171_F_MAKE(MAX86171_MEAS_SINC3_SEL,    0)}, /* SINC3 filter not used */

    {MAX86171_REG_MEAS7_CFG3,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_DACOFF,  0) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_DACOFF,  0) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_SETLNG,   1) | /* Default 12us */
        MAX86171_F_MAKE(MAX86171_MEAS_PD_SETLNG,    1)}, /* Default 12.1us */

    {MAX86171_REG_MEAS7_DRVA_PA,   0x00u},  /* LED1_DRV disabled   */
    {MAX86171_REG_MEAS7_DRVB_PA,   0x08u},  /* LED8_DRV ~0.48mA    */
    {MAX86171_REG_MEAS7_DRVC_PA,   0x08u},  /* LED9_DRV ~0.48mA    */
},

// MEAS8 [7] — 526nm Green SFH (outer)
{
    {MAX86171_REG_MEAS8_SELECTS,
        MAX86171_F_MAKE(MAX86171_MEAS_DRVA_SEL, DRVA_GREEN_536_CT)  |  /* LED1_DRV – 536nm Green CT  */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVB_SEL, DRVB_GREEN_526_SFH) |  /* LED8_DRV – 526nm Green SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVC_SEL, DRVC_GREEN_526_SFH) |  /* LED9_DRV – 526nm Green SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_AMB,      0)},

    {MAX86171_REG_MEAS8_CFG1,
        MAX86171_F_MAKE(MAX86171_MEAS_AVER,         1) | /* A total of 5 interleaved dark and exposure measurements are taken and average is loaded to FIFO */
        MAX86171_F_MAKE(MAX86171_MEAS_TINT,         3) | /* Measurement integration time = 117.1us */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_PDSEL,   1) | /* PD3 / PD4 selected as inputs  */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_PDSEL,   1)},

    {MAX86171_REG_MEAS8_CFG2,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_ADC_RGE, 2) | /* PPG1 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_ADC_RGE, 2) | /* PPG2 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_RGE,      3) | /* LED full scale range 128 mA */
        MAX86171_F_MAKE(MAX86171_MEAS_FILT_SEL,     0) | /* Ambient light estimation by CDM (Central Difference Method) */
        MAX86171_F_MAKE(MAX86171_MEAS_SINC3_SEL,    0)}, /* SINC3 filter not used */

    {MAX86171_REG_MEAS8_CFG3,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_DACOFF,  0) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_DACOFF,  0) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_SETLNG,   1) | /* Default 12us */
        MAX86171_F_MAKE(MAX86171_MEAS_PD_SETLNG,    1)}, /* Default 12.1us */

    {MAX86171_REG_MEAS8_DRVA_PA,   0x00u},  /* LED1_DRV disabled   */
    {MAX86171_REG_MEAS8_DRVB_PA,   0x08u},  /* LED8_DRV ~0.48mA    */
    {MAX86171_REG_MEAS8_DRVC_PA,   0x08u},  /* LED9_DRV ~0.48mA    */
},

// MEAS9 [8] — Ambient (all drivers PA=0)
// Bugfix: MEAS_AMB corrected to 1
{
    {MAX86171_REG_MEAS9_SELECTS,
        MAX86171_F_MAKE(MAX86171_MEAS_DRVA_SEL, DRVA_GREEN_536_CT)  |  /* LED1_DRV – PA=0, disabled */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVB_SEL, DRVB_RED_660)       |  /* LED2_DRV – PA=0, disabled */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVC_SEL, DRVC_GREEN_536_CT)  |  /* LED1_DRV – PA=0, disabled */
        MAX86171_F_MAKE(MAX86171_MEAS_AMB,      1)},

    {MAX86171_REG_MEAS9_CFG1,
        MAX86171_F_MAKE(MAX86171_MEAS_AVER,         0) | /* A total of 3 interleaved dark and exposure measurements are taken and average is loaded to FIFO */
        MAX86171_F_MAKE(MAX86171_MEAS_TINT,         3) | /* Measurement integration time = 117.1us */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_PDSEL,   0) | /* PD1 / PD2 selected as inputs  */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_PDSEL,   0)},

    {MAX86171_REG_MEAS9_CFG2,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_ADC_RGE, 2) | /* PPG1 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_ADC_RGE, 2) | /* PPG2 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_RGE,      3) | /* LED full scale range 128 mA */
        MAX86171_F_MAKE(MAX86171_MEAS_FILT_SEL,     0) | /* Ambient light estimation by CDM (Central Difference Method) */
        MAX86171_F_MAKE(MAX86171_MEAS_SINC3_SEL,    0)}, /* SINC3 filter not used */

    {MAX86171_REG_MEAS9_CFG3,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_DACOFF,  0) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_DACOFF,  0) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_SETLNG,   1) | /* Default 12us */
        MAX86171_F_MAKE(MAX86171_MEAS_PD_SETLNG,    1)}, /* Default 12.1us */

    {MAX86171_REG_MEAS9_DRVA_PA,   0x00u},  /* all LEDs off – ambient only */
    {MAX86171_REG_MEAS9_DRVB_PA,   0x00u},
    {MAX86171_REG_MEAS9_DRVC_PA,   0x00u},
},
};

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

/* Clock & frame-rate */
int max86171_set_sync_mode(max86171_t *dev, int mode);
int max86171_set_frame_rate(max86171_t *dev, uint32_t fps, bool use_32768Hz);

/* FIFO operations */
int max86171_fifo_flush(max86171_t *dev);
int max86171_fifo_discard_until_first_tag(max86171_t *dev, uint8_t first_tag);
int max86171_fifo_read_samples(max86171_t *dev,
                               uint16_t max_items,
                               int *out_count,
                               uint8_t *tags,
                               int32_t *ch1,
                               int32_t *ch2);

/* Measurement configuration */
int max86171_configure_measurement(max86171_t *dev, int meas_idx, const max86171_meas_cfg_t *cfg);
int max86171_enable_measurements(max86171_t *dev, uint16_t meas_bitmask);

/* PD/LED utilities */
int max86171_set_pd_bias(max86171_t *dev, int pd_idx, max86171_pd_bias_t bias);
int max86171_set_led_range(max86171_t *dev, max86171_led_range_t range);

/* Interrupts */
int max86171_irq_config(max86171_t *dev, uint32_t int1_en_mask, uint32_t int2_en_mask);
int max86171_irq_read_clear(max86171_t *dev, max86171_irq_status_t *out);

/* Low-level register access */
int max86171_write_reg(max86171_t *dev, uint8_t reg, const uint8_t *data, size_t len);
int max86171_read_reg(max86171_t *dev, uint8_t reg, uint8_t *data, size_t len);
int max86171_rmw_bits(max86171_t *dev, uint8_t reg, uint8_t mask, uint8_t value_shifted);

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


int max86171_read_FIFO(max86171_t *dev, uint8_t *data, size_t len) ;

int max86171_set_led_current(max86171_t *dev, uint8_t meas_idx, uint8_t pa);
int max86171_set_avg(max86171_t *dev, uint8_t meas_idx, uint8_t avg);

/*
 * @brief Get number of registers in configuration table
 */
 size_t max86171_get_config_size(void);

/*
 * @brief Get register configuration entry by index
 */
 int max86171_get_config_entry(size_t index, uint8_t *addr, uint8_t *value);
 /**
  * @brief Shutdown the device
  */
 int max86171_shutdown(max86171_t *dev);
 /**
  * @brief Start the device running (after configuration already loaded)
  */
 int max86171_start_running(max86171_t *dev);
#ifdef __cplusplus
}
#endif

#endif /* MAX86171_H */
