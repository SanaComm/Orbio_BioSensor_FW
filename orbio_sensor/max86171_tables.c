
/*
 * @file max86171_tables.c
 * @brief STM32-specific driver for MAX86171 Optical AFE (SPI only)
 */

#include "max86171.h"
#include "max86171_regs.h"
#include "ppg_agc.h"

/* Base config: everything except MEAS blocks */
const max86171_reg_config_t g_max86171_base_config[MAX86171_BASE_CONFIG_SIZE] = {

    // FIFO Configuration
    {MAX86171_REG_FIFO_RD_PTR,  0x00},                    /* Reset read pointer */
    {MAX86171_REG_FIFO_CFG1,    256 - MAX_FIFO_SAMPLES},  /* A_FULL threshold */
    {MAX86171_REG_FIFO_CFG2,    /* 0x0A = 0x10 */
        MAX86171_F_MAKE(MAX86171_FIFO_RO,           0) |  /* no rollover */
        MAX86171_F_MAKE(MAX86171_FIFO_A_FULL_TYPE,  1) |  /* A_FULL = no repeat */
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

    {MAX86171_REG_SYS_CFG3,     /* 0x0E = 0x00 */
        MAX86171_F_MAKE(MAX86171_SYS_MEAS1_CONFIG_SEL, 0) |  /* unique config per measurement */
        MAX86171_F_MAKE(MAX86171_SYS_COLLECT_RAW_DATA, 0) |  /* computed data and not raw */
        MAX86171_F_MAKE(MAX86171_SYS_ALC_DISABLE,      0)},  /* ALC enabled */

    {MAX86171_REG_PD_BIAS,      /* 0x0F = 0x55 */
        MAX86171_F_MAKE(MAX86171_PD1_BIAS,          MAX86171_PD_BIAS_0_125pF) |  /* PD1 bias = 65pF */
        MAX86171_F_MAKE(MAX86171_PD2_BIAS,          MAX86171_PD_BIAS_0_125pF) |  /* PD2 bias = 65pF */
        MAX86171_F_MAKE(MAX86171_PD3_BIAS,          MAX86171_PD_BIAS_0_125pF) |  /* PD3 bias = 65pF */
        MAX86171_F_MAKE(MAX86171_PD4_BIAS,          MAX86171_PD_BIAS_0_125pF)},  /* PD4 bias = 65pF */

    // Pin Configuration
    {MAX86171_REG_PIN_FUNC_CFG,     0x00},                /* defaults */
    {MAX86171_REG_OUTPUT_PIN_CFG,   0x00},                /* defaults */

    // Frame Rate Clock Configuration (100 Hz)
    // Bugfix: Was incorrectly calculated based on 32.768kHz but was actually set to 32kHz
    // FR_CLK = 32000 Hz / DIV → DIV=320 → 32000/320 = 100 Hz
    // At 100Hz frame rate, this means 9 measurements, including averaging, and ambient cancelling
    // have to fit within 10 msec.
    // 6 channels have MEAS_AVER = 2, 2 channels have MEAS_AVER = 1, 1 channel has MEAS_AVER = 0
    // So number of pulses will be 6*(2*2^2+1)+2*(2*2^1+1)+1*(2*2^0+1) = 67
    // MEAS_TINT = 117.1 us, MEAS_PD_SETLNG = 12.1 us, giving total pulse width = 117.1+12.1 = 129.2 us
    // 67 pulses * 129.2 us per pulse = 8.656 msec
    // Add tINIT1 = 7 * tCLOCK at the start of a frame and tINIT = 3 * tCLOCK for each measurement
    // = 7 / 32000 + (9 * 3) / 32000 = 34/32000 = 1.0625 msec
    // TOTAL = 8.656+1.0625 = 9.7185 msec, which fits in the frame time of 10 msec
    // If MEAS7/MEAS8 are changed to MEAS_AVER = 2, the timing will exceed 10 msec

    {MAX86171_REG_FR_CLK_FREQ_SEL,  /* 0x15 = 0x00 */
        MAX86171_F_MAKE(MAX86171_FR_CLK_FINE_TUNE,  0) |  /* no fine tune */
        MAX86171_F_MAKE(MAX86171_FR_CLK_SEL,        0)},  /* internal 32kHz */

    {MAX86171_REG_FR_CLK_DIV_MSB,   /* 0x16 = 0x01 */
        MAX86171_F_MAKE(MAX86171_FR_CLK_DIV_H,      0x01)},  /* divider MSB */

    {MAX86171_REG_FR_CLK_DIV_LSB,   /* 0x17 = 0x40 */
        MAX86171_F_MAKE(MAX86171_FR_CLK_DIV_L,      0x40)},  /* divider LSB → total=0x0140=320 */

    // Threshold Configuration
    {MAX86171_REG_THRESH_MEAS_SEL,  0x00},  /* threshold on MEAS1 */
    {MAX86171_REG_THRESH_HYST,      0x00},  /* no hysteresis */
    {MAX86171_REG_THRESH1_HI,       0xFF},  /* HI threshold 1 = max */
    {MAX86171_REG_THRESH1_LO,       0x00},  /* LO threshold 1 = min */
    {MAX86171_REG_THRESH2_HI,       0xFF},  /* HI threshold 2 = max */
    {MAX86171_REG_THRESH2_LO,       0x00},  /* LO threshold 2 = min */

    // Picket Fence
    {MAX86171_REG_PF_MEAS_SEL,      0x00},  /* PF on MEAS1 */
    {MAX86171_REG_PF_CFG,           0x40},  /* PF enabled, default config */

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
 * 2.  What is the logic behind PPG1/PPG2 ADC full range settings of 16uA?
 */

max86171_reg_config_t g_max86171_meas_config[MAX86171_NUM_MEAS][MAX86171_MEAS_REGS] = {

// MEAS1 [0] — 536nm Green CT (center)
{
    {MAX86171_REG_MEAS1_SELECTS,
        MAX86171_F_MAKE(MAX86171_MEAS_DRVA_SEL, DRVA_GREEN_536_CT)  |  /* LED1_DRV – 536nm Green CT */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVB_SEL, DRVB_GREEN_536_CT)  |  /* LED5_DRV – 536nm Green CT */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVC_SEL, DRVC_GREEN_536_CT)  |  /* LED1_DRV – 536nm Green CT */
        MAX86171_F_MAKE(MAX86171_MEAS_AMB,      MAX86171_MEAS_AMB_NORMAL)},

    {MAX86171_REG_MEAS1_CFG1,
        MAX86171_F_MAKE(MAX86171_MEAS_AVER,         2)  | /* A total of 9 interleaved dark and exposure measurements are taken and average is loaded to FIFO */
        MAX86171_F_MAKE(MAX86171_MEAS_TINT,         MAX86171_TINT_117_1us)  | /* Measurement integration time = 117.1us */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_PDSEL,   MAX86171_PPG1_PDSEL_PD1)  | /* PD1 / PD2 selected as inputs */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_PDSEL,   MAX86171_PPG2_PDSEL_PD2)},

    {MAX86171_REG_MEAS1_CFG2,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_ADC_RGE, MAX86171_ADC_RANGE_16uA) | /* PPG1 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_ADC_RGE, MAX86171_ADC_RANGE_16uA) | /* PPG2 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_RGE,      MAX86171_LED_RANGE_32mA) | /* LED full scale range 32 mA */
        MAX86171_F_MAKE(MAX86171_MEAS_FILT_SEL,     MAX86171_FILT_SEL_CDM) |   /* Ambient light estimation by CDM (Central Difference Method) */
        MAX86171_F_MAKE(MAX86171_MEAS_SINC3_SEL,    MAX86171_SINC3_SEL_OFF)},  /* SINC3 filter not used */

    {MAX86171_REG_MEAS1_CFG3,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_DACOFF,  MAX86171_DACOFF_0_uA) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_DACOFF,  MAX86171_DACOFF_0_uA) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_SETLNG,   MAX86171_LED_SETLNG_12_us) | /* Default 12us */
        MAX86171_F_MAKE(MAX86171_MEAS_PD_SETLNG,    MAX86171_PD_SETLNG_12_1us)}, /* Default 12.1us */

    {MAX86171_REG_MEAS1_DRVA_PA,   0x00u},             /* LED1_DRV disabled   */
    {MAX86171_REG_MEAS1_DRVB_PA,   AGC_PROX_IDLE_PA},  /* LED5_DRV            */
    {MAX86171_REG_MEAS1_DRVC_PA,   AGC_PROX_IDLE_PA},  /* LED1_DRV            */
},

// MEAS2 [1] — 536nm Green CT (center)
{
    {MAX86171_REG_MEAS2_SELECTS,
        MAX86171_F_MAKE(MAX86171_MEAS_DRVA_SEL, DRVA_GREEN_536_CT)  |  /* LED1_DRV – 536nm Green CT */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVB_SEL, DRVB_GREEN_536_CT)  |  /* LED5_DRV – 536nm Green CT */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVC_SEL, DRVC_GREEN_536_CT)  |  /* LED1_DRV – 536nm Green CT */
        MAX86171_F_MAKE(MAX86171_MEAS_AMB,      MAX86171_MEAS_AMB_NORMAL)},

    {MAX86171_REG_MEAS2_CFG1,
        MAX86171_F_MAKE(MAX86171_MEAS_AVER,         2) | /* A total of 9 interleaved dark and exposure measurements are taken and average is loaded to FIFO */
        MAX86171_F_MAKE(MAX86171_MEAS_TINT,         MAX86171_TINT_117_1us) | /* Measurement integration time = 117.1us */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_PDSEL,   MAX86171_PPG1_PDSEL_PD3) | /* PD3 / PD4 selected as inputs  */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_PDSEL,   MAX86171_PPG2_PDSEL_PD4)},

    {MAX86171_REG_MEAS2_CFG2,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_ADC_RGE, MAX86171_ADC_RANGE_16uA) | /* PPG1 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_ADC_RGE, MAX86171_ADC_RANGE_16uA) | /* PPG2 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_RGE,      MAX86171_LED_RANGE_32mA) | /* LED full scale range 32 mA */
        MAX86171_F_MAKE(MAX86171_MEAS_FILT_SEL,     MAX86171_FILT_SEL_CDM) |   /* Ambient light estimation by CDM (Central Difference Method) */
        MAX86171_F_MAKE(MAX86171_MEAS_SINC3_SEL,    MAX86171_SINC3_SEL_OFF)},  /* SINC3 filter not used */

    {MAX86171_REG_MEAS2_CFG3,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_DACOFF,  MAX86171_DACOFF_0_uA) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_DACOFF,  MAX86171_DACOFF_0_uA) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_SETLNG,   MAX86171_LED_SETLNG_12_us) | /* Default 12us */
        MAX86171_F_MAKE(MAX86171_MEAS_PD_SETLNG,    MAX86171_PD_SETLNG_12_1us)}, /* Default 12.1us */

    {MAX86171_REG_MEAS2_DRVA_PA,   0x00u},             /* LED1_DRV disabled   */
    {MAX86171_REG_MEAS2_DRVB_PA,   AGC_PROX_IDLE_PA},  /* LED5_DRV            */
    {MAX86171_REG_MEAS2_DRVC_PA,   AGC_PROX_IDLE_PA},  /* LED1_DRV            */
},

// MEAS3 [2] — 950nm IR
{
    {MAX86171_REG_MEAS3_SELECTS,
        MAX86171_F_MAKE(MAX86171_MEAS_DRVA_SEL, DRVA_IR_950)        |  /* LED7_DRV – 950nm IR SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVB_SEL, DRVB_IR_950)        |  /* LED3_DRV – 950nm IR SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVC_SEL, DRVC_IR_950)        |  /* LED3_DRV – 950nm IR SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_AMB,      MAX86171_MEAS_AMB_NORMAL)},

    {MAX86171_REG_MEAS3_CFG1,
        MAX86171_F_MAKE(MAX86171_MEAS_AVER,         2) | /* A total of 9 interleaved dark and exposure measurements are taken and average is loaded to FIFO */
        MAX86171_F_MAKE(MAX86171_MEAS_TINT,         MAX86171_TINT_117_1us) | /* Measurement integration time = 117.1us */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_PDSEL,   MAX86171_PPG1_PDSEL_PD1) | /* PD1 / PD2 selected as inputs  */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_PDSEL,   MAX86171_PPG2_PDSEL_PD2)},

    {MAX86171_REG_MEAS3_CFG2,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_ADC_RGE, MAX86171_ADC_RANGE_16uA) | /* PPG1 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_ADC_RGE, MAX86171_ADC_RANGE_16uA) | /* PPG2 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_RGE,      MAX86171_LED_RANGE_32mA) | /* LED full scale range 32 mA */
        MAX86171_F_MAKE(MAX86171_MEAS_FILT_SEL,     MAX86171_FILT_SEL_CDM) |   /* Ambient light estimation by CDM (Central Difference Method) */
        MAX86171_F_MAKE(MAX86171_MEAS_SINC3_SEL,    MAX86171_SINC3_SEL_OFF)},  /* SINC3 filter not used */

    {MAX86171_REG_MEAS3_CFG3,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_DACOFF,  MAX86171_DACOFF_0_uA) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_DACOFF,  MAX86171_DACOFF_0_uA) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_SETLNG,   MAX86171_LED_SETLNG_12_us) | /* Default 12us */
        MAX86171_F_MAKE(MAX86171_MEAS_PD_SETLNG,    MAX86171_PD_SETLNG_12_1us)}, /* Default 12.1us */

    {MAX86171_REG_MEAS3_DRVA_PA,   AGC_PROX_IDLE_PA},  /* LED7_DRV            */
    {MAX86171_REG_MEAS3_DRVB_PA,   0x00u},             /* LED3_DRV disabled   */
    {MAX86171_REG_MEAS3_DRVC_PA,   AGC_PROX_IDLE_PA},  /* LED3_DRV            */
},

// MEAS4 [3] — 950nm IR
{
    {MAX86171_REG_MEAS4_SELECTS,
        MAX86171_F_MAKE(MAX86171_MEAS_DRVA_SEL, DRVA_IR_950)        |  /* LED7_DRV – 950nm IR SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVB_SEL, DRVB_IR_950)        |  /* LED3_DRV – 950nm IR SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVC_SEL, DRVC_IR_950)        |  /* LED3_DRV – 950nm IR SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_AMB,      MAX86171_MEAS_AMB_NORMAL)},

    {MAX86171_REG_MEAS4_CFG1,
        MAX86171_F_MAKE(MAX86171_MEAS_AVER,         2) | /* A total of 9 interleaved dark and exposure measurements are taken and average is loaded to FIFO */
        MAX86171_F_MAKE(MAX86171_MEAS_TINT,         MAX86171_TINT_117_1us) | /* Measurement integration time = 117.1us */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_PDSEL,   MAX86171_PPG1_PDSEL_PD3) | /* PD3 / PD4 selected as inputs  */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_PDSEL,   MAX86171_PPG2_PDSEL_PD4)},

    {MAX86171_REG_MEAS4_CFG2,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_ADC_RGE, MAX86171_ADC_RANGE_16uA) | /* PPG1 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_ADC_RGE, MAX86171_ADC_RANGE_16uA) | /* PPG2 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_RGE,      MAX86171_LED_RANGE_32mA) | /* LED full scale range 32 mA */
        MAX86171_F_MAKE(MAX86171_MEAS_FILT_SEL,     MAX86171_FILT_SEL_CDM) |   /* Ambient light estimation by CDM (Central Difference Method) */
        MAX86171_F_MAKE(MAX86171_MEAS_SINC3_SEL,    MAX86171_SINC3_SEL_OFF)},  /* SINC3 filter not used */

    {MAX86171_REG_MEAS4_CFG3,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_DACOFF,  MAX86171_DACOFF_0_uA) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_DACOFF,  MAX86171_DACOFF_0_uA) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_SETLNG,   MAX86171_LED_SETLNG_12_us) | /* Default 12us */
        MAX86171_F_MAKE(MAX86171_MEAS_PD_SETLNG,    MAX86171_PD_SETLNG_12_1us)}, /* Default 12.1us */

    {MAX86171_REG_MEAS4_DRVA_PA,   AGC_PROX_IDLE_PA},  /* LED7_DRV            */
    {MAX86171_REG_MEAS4_DRVB_PA,   0x00u},             /* LED3_DRV disabled   */
    {MAX86171_REG_MEAS4_DRVC_PA,   AGC_PROX_IDLE_PA},  /* LED3_DRV            */
},

// MEAS5 [4] — 660nm Red
{
    {MAX86171_REG_MEAS5_SELECTS,
        MAX86171_F_MAKE(MAX86171_MEAS_DRVA_SEL, DRVA_RED_660)       |  /* LED2_DRV – 660nm Red SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVB_SEL, DRVB_RED_660)       |  /* LED2_DRV – 660nm Red SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVC_SEL, DRVC_RED_660)       |  /* LED6_DRV – 660nm Red SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_AMB,      MAX86171_MEAS_AMB_NORMAL)},

    {MAX86171_REG_MEAS5_CFG1,
        MAX86171_F_MAKE(MAX86171_MEAS_AVER,         2) | /* A total of 9 interleaved dark and exposure measurements are taken and average is loaded to FIFO */
        MAX86171_F_MAKE(MAX86171_MEAS_TINT,         MAX86171_TINT_117_1us) | /* Measurement integration time = 117.1us */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_PDSEL,   MAX86171_PPG1_PDSEL_PD1) | /* PD1 / PD2 selected as inputs  */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_PDSEL,   MAX86171_PPG2_PDSEL_PD2)},

    {MAX86171_REG_MEAS5_CFG2,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_ADC_RGE, MAX86171_ADC_RANGE_16uA) | /* PPG1 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_ADC_RGE, MAX86171_ADC_RANGE_16uA) | /* PPG2 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_RGE,      MAX86171_LED_RANGE_32mA) | /* LED full scale range 32 mA */
        MAX86171_F_MAKE(MAX86171_MEAS_FILT_SEL,     MAX86171_FILT_SEL_CDM) |   /* Ambient light estimation by CDM (Central Difference Method) */
        MAX86171_F_MAKE(MAX86171_MEAS_SINC3_SEL,    MAX86171_SINC3_SEL_OFF)},  /* SINC3 filter not used */

    {MAX86171_REG_MEAS5_CFG3,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_DACOFF,  MAX86171_DACOFF_0_uA) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_DACOFF,  MAX86171_DACOFF_0_uA) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_SETLNG,   MAX86171_LED_SETLNG_12_us) | /* Default 12us */
        MAX86171_F_MAKE(MAX86171_MEAS_PD_SETLNG,    MAX86171_PD_SETLNG_12_1us)}, /* Default 12.1us */

    {MAX86171_REG_MEAS5_DRVA_PA,   AGC_PROX_IDLE_PA},  /* LED2_DRV            */
    {MAX86171_REG_MEAS5_DRVB_PA,   0x00u},             /* LED2_DRV disabled   */
    {MAX86171_REG_MEAS5_DRVC_PA,   AGC_PROX_IDLE_PA},  /* LED6_DRV            */
},

// MEAS6 [5] — 660nm Red
{
    {MAX86171_REG_MEAS6_SELECTS,
        MAX86171_F_MAKE(MAX86171_MEAS_DRVA_SEL, DRVA_RED_660)       |  /* LED2_DRV – 660nm Red SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVB_SEL, DRVB_RED_660)       |  /* LED2_DRV – 660nm Red SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVC_SEL, DRVC_RED_660)       |  /* LED6_DRV – 660nm Red SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_AMB,      MAX86171_MEAS_AMB_NORMAL)},

    {MAX86171_REG_MEAS6_CFG1,
        MAX86171_F_MAKE(MAX86171_MEAS_AVER,         2) | /* A total of 9 interleaved dark and exposure measurements are taken and average is loaded to FIFO */
        MAX86171_F_MAKE(MAX86171_MEAS_TINT,         MAX86171_TINT_117_1us) | /* Measurement integration time = 117.1us */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_PDSEL,   MAX86171_PPG1_PDSEL_PD3) | /* PD3 / PD4 selected as inputs  */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_PDSEL,   MAX86171_PPG2_PDSEL_PD4)},

    {MAX86171_REG_MEAS6_CFG2,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_ADC_RGE, MAX86171_ADC_RANGE_16uA) | /* PPG1 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_ADC_RGE, MAX86171_ADC_RANGE_16uA) | /* PPG2 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_RGE,      MAX86171_LED_RANGE_32mA) | /* LED full scale range 32 mA */
        MAX86171_F_MAKE(MAX86171_MEAS_FILT_SEL,     MAX86171_FILT_SEL_CDM) |   /* Ambient light estimation by CDM (Central Difference Method) */
        MAX86171_F_MAKE(MAX86171_MEAS_SINC3_SEL,    MAX86171_SINC3_SEL_OFF)},  /* SINC3 filter not used */

    {MAX86171_REG_MEAS6_CFG3,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_DACOFF,  MAX86171_DACOFF_0_uA) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_DACOFF,  MAX86171_DACOFF_0_uA) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_SETLNG,   MAX86171_LED_SETLNG_12_us) | /* Default 12us */
        MAX86171_F_MAKE(MAX86171_MEAS_PD_SETLNG,    MAX86171_PD_SETLNG_12_1us)}, /* Default 12.1us */

    {MAX86171_REG_MEAS6_DRVA_PA,   AGC_PROX_IDLE_PA},  /* LED2_DRV            */
    {MAX86171_REG_MEAS6_DRVB_PA,   0x00u},             /* LED2_DRV disabled   */
    {MAX86171_REG_MEAS6_DRVC_PA,   AGC_PROX_IDLE_PA},  /* LED6_DRV            */
},

// MEAS7 [6] — 526nm Green SFH (outer)
// MEAS7 & MEAS8 have MEAS_AVER = 1 and not 2 to fit within overall frame timing else we get INVALID_CFG error
{
    {MAX86171_REG_MEAS7_SELECTS,
        MAX86171_F_MAKE(MAX86171_MEAS_DRVA_SEL, DRVA_GREEN_536_CT)  |  /* LED1_DRV – 536nm Green CT  */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVB_SEL, DRVB_GREEN_526_SFH) |  /* LED8_DRV – 526nm Green SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVC_SEL, DRVC_GREEN_526_SFH) |  /* LED9_DRV – 526nm Green SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_AMB,      MAX86171_MEAS_AMB_NORMAL)},

    {MAX86171_REG_MEAS7_CFG1,
        MAX86171_F_MAKE(MAX86171_MEAS_AVER,         1) | /* A total of 5 interleaved dark and exposure measurements are taken and average is loaded to FIFO */
        MAX86171_F_MAKE(MAX86171_MEAS_TINT,         MAX86171_TINT_117_1us) | /* Measurement integration time = 117.1us */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_PDSEL,   MAX86171_PPG1_PDSEL_PD1) | /* PD1 / PD2 selected as inputs  */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_PDSEL,   MAX86171_PPG2_PDSEL_PD2)},

    {MAX86171_REG_MEAS7_CFG2,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_ADC_RGE, MAX86171_ADC_RANGE_16uA) | /* PPG1 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_ADC_RGE, MAX86171_ADC_RANGE_16uA) | /* PPG2 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_RGE,      MAX86171_LED_RANGE_32mA) | /* LED full scale range 32 mA */
        MAX86171_F_MAKE(MAX86171_MEAS_FILT_SEL,     MAX86171_FILT_SEL_CDM) |   /* Ambient light estimation by CDM (Central Difference Method) */
        MAX86171_F_MAKE(MAX86171_MEAS_SINC3_SEL,    MAX86171_SINC3_SEL_OFF)},  /* SINC3 filter not used */

    {MAX86171_REG_MEAS7_CFG3,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_DACOFF,  MAX86171_DACOFF_0_uA) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_DACOFF,  MAX86171_DACOFF_0_uA) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_SETLNG,   MAX86171_LED_SETLNG_12_us) | /* Default 12us */
        MAX86171_F_MAKE(MAX86171_MEAS_PD_SETLNG,    MAX86171_PD_SETLNG_12_1us)}, /* Default 12.1us */

    {MAX86171_REG_MEAS7_DRVA_PA,   0x00u},             /* LED1_DRV disabled   */
    {MAX86171_REG_MEAS7_DRVB_PA,   AGC_PROX_IDLE_PA},  /* LED8_DRV            */
    {MAX86171_REG_MEAS7_DRVC_PA,   AGC_PROX_IDLE_PA},  /* LED9_DRV            */
},

// MEAS8 [7] — 526nm Green SFH (outer)
{
    {MAX86171_REG_MEAS8_SELECTS,
        MAX86171_F_MAKE(MAX86171_MEAS_DRVA_SEL, DRVA_GREEN_536_CT)  |  /* LED1_DRV – 536nm Green CT  */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVB_SEL, DRVB_GREEN_526_SFH) |  /* LED8_DRV – 526nm Green SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVC_SEL, DRVC_GREEN_526_SFH) |  /* LED9_DRV – 526nm Green SFH */
        MAX86171_F_MAKE(MAX86171_MEAS_AMB,      MAX86171_MEAS_AMB_NORMAL)},

    {MAX86171_REG_MEAS8_CFG1,
        MAX86171_F_MAKE(MAX86171_MEAS_AVER,         1) | /* A total of 5 interleaved dark and exposure measurements are taken and average is loaded to FIFO */
        MAX86171_F_MAKE(MAX86171_MEAS_TINT,         MAX86171_TINT_117_1us) | /* Measurement integration time = 117.1us */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_PDSEL,   MAX86171_PPG1_PDSEL_PD3) | /* PD3 / PD4 selected as inputs  */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_PDSEL,   MAX86171_PPG2_PDSEL_PD4)},

    {MAX86171_REG_MEAS8_CFG2,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_ADC_RGE, MAX86171_ADC_RANGE_16uA) | /* PPG1 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_ADC_RGE, MAX86171_ADC_RANGE_16uA) | /* PPG2 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_RGE,      MAX86171_LED_RANGE_32mA) | /* LED full scale range 32 mA */
        MAX86171_F_MAKE(MAX86171_MEAS_FILT_SEL,     MAX86171_FILT_SEL_CDM) |   /* Ambient light estimation by CDM (Central Difference Method) */
        MAX86171_F_MAKE(MAX86171_MEAS_SINC3_SEL,    MAX86171_SINC3_SEL_OFF)},  /* SINC3 filter not used */

    {MAX86171_REG_MEAS8_CFG3,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_DACOFF,  MAX86171_DACOFF_0_uA) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_DACOFF,  MAX86171_DACOFF_0_uA) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_SETLNG,   MAX86171_LED_SETLNG_12_us) | /* Default 12us */
        MAX86171_F_MAKE(MAX86171_MEAS_PD_SETLNG,    MAX86171_PD_SETLNG_12_1us)}, /* Default 12.1us */

    {MAX86171_REG_MEAS8_DRVA_PA,   0x00u},             /* LED1_DRV disabled   */
    {MAX86171_REG_MEAS8_DRVB_PA,   AGC_PROX_IDLE_PA},  /* LED8_DRV            */
    {MAX86171_REG_MEAS8_DRVC_PA,   AGC_PROX_IDLE_PA},  /* LED9_DRV            */
},

// MEAS9 [8] — Ambient (all drivers PA=0)
// TODO: If MEAS_AMB is set to 1, the channel constantly gives ALC tag error
{
    {MAX86171_REG_MEAS9_SELECTS,
        MAX86171_F_MAKE(MAX86171_MEAS_DRVA_SEL, DRVA_GREEN_536_CT)  |  /* LED1_DRV – PA=0, disabled */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVB_SEL, DRVB_RED_660)       |  /* LED2_DRV – PA=0, disabled */
        MAX86171_F_MAKE(MAX86171_MEAS_DRVC_SEL, DRVC_GREEN_536_CT)  |  /* LED1_DRV – PA=0, disabled */
        MAX86171_F_MAKE(MAX86171_MEAS_AMB,      MAX86171_MEAS_AMB_NORMAL)},

    {MAX86171_REG_MEAS9_CFG1,
        MAX86171_F_MAKE(MAX86171_MEAS_AVER,         0) | /* A total of 3 interleaved dark and exposure measurements are taken and average is loaded to FIFO */
        MAX86171_F_MAKE(MAX86171_MEAS_TINT,         MAX86171_TINT_117_1us) | /* Measurement integration time = 117.1us */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_PDSEL,   MAX86171_PPG1_PDSEL_PD1) | /* PD1 / PD2 selected as inputs  */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_PDSEL,   MAX86171_PPG2_PDSEL_PD2)},

    {MAX86171_REG_MEAS9_CFG2,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_ADC_RGE, MAX86171_ADC_RANGE_16uA) | /* PPG1 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_ADC_RGE, MAX86171_ADC_RANGE_16uA) | /* PPG2 ADC full range = 16uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_RGE,      MAX86171_LED_RANGE_32mA) | /* LED full scale range 32 mA */
        MAX86171_F_MAKE(MAX86171_MEAS_FILT_SEL,     MAX86171_FILT_SEL_CDM) |   /* Ambient light estimation by CDM (Central Difference Method) */
        MAX86171_F_MAKE(MAX86171_MEAS_SINC3_SEL,    MAX86171_SINC3_SEL_OFF)},  /* SINC3 filter not used */

    {MAX86171_REG_MEAS9_CFG3,
        MAX86171_F_MAKE(MAX86171_MEAS_PPG1_DACOFF,  MAX86171_DACOFF_0_uA) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_PPG2_DACOFF,  MAX86171_DACOFF_0_uA) | /* DAC offset current = 0 uA */
        MAX86171_F_MAKE(MAX86171_MEAS_LED_SETLNG,   MAX86171_LED_SETLNG_12_us) | /* Default 12us */
        MAX86171_F_MAKE(MAX86171_MEAS_PD_SETLNG,    MAX86171_PD_SETLNG_12_1us)}, /* Default 12.1us */

    {MAX86171_REG_MEAS9_DRVA_PA,   0x00u},  /* all LEDs off – ambient only */
    {MAX86171_REG_MEAS9_DRVB_PA,   0x00u},
    {MAX86171_REG_MEAS9_DRVC_PA,   0x00u},
},
};

/* End of file */
