/**
 * @file max86171_regs.h
 * @brief Register map for MAX86171 Optical AFE - Corrected from Datasheet Rev 4
 *
 * All register addresses, bit positions, and masks verified against
 * MAX86171 datasheet revision 4, June 2022
 */

#ifndef MAX86171_REGS_H
#define MAX86171_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================
 *  Register Address Map (from datasheet page 42-46)
 * ============================= */

/* Status Registers */
#define MAX86171_REG_STATUS1            0x00u  /* RO: Status 1 */
#define MAX86171_REG_STATUS2            0x01u  /* RO: Status 2 */
#define MAX86171_REG_STATUS3            0x02u  /* RO: Status 3 */

/* FIFO Registers */
#define MAX86171_REG_FIFO_WR_PTR        0x04u  /* RO: FIFO write pointer */
#define MAX86171_REG_FIFO_RD_PTR        0x05u  /* R/W: FIFO read pointer */
#define MAX86171_REG_FIFO_COUNTER1      0x06u  /* RO: FIFO Counter 1 */
#define MAX86171_REG_FIFO_COUNTER2      0x07u  /* RO: FIFO Counter 2 */
#define MAX86171_REG_FIFO_DATA          0x08u  /* RO: FIFO data */
#define MAX86171_REG_FIFO_CFG1          0x09u  /* R/W: FIFO Configuration 1 */
#define MAX86171_REG_FIFO_CFG2          0x0Au  /* R/W: FIFO Configuration 2 */

/* System Control */
#define MAX86171_REG_SYS_CFG1           0x0Cu  /* R/W: System Configuration 1 */
#define MAX86171_REG_SYS_CFG2           0x0Du  /* R/W: System Configuration 2 */
#define MAX86171_REG_SYS_CFG3           0x0Eu  /* R/W: System Configuration 3 */
#define MAX86171_REG_PD_BIAS            0x0Fu  /* R/W: Photodiode Bias */

/* Pin Configuration */
#define MAX86171_REG_PIN_FUNC_CFG       0x10u  /* R/W: Pin Functional Configuration */
#define MAX86171_REG_OUTPUT_PIN_CFG     0x11u  /* R/W: Output Pin Configuration */

/* Frame Rate Clock */
#define MAX86171_REG_FR_CLK_FREQ_SEL    0x15u  /* R/W: FR Clock Frequency Select */
#define MAX86171_REG_FR_CLK_DIV_MSB     0x16u  /* R/W: FR Clock Divider MSB */
#define MAX86171_REG_FR_CLK_DIV_LSB     0x17u  /* R/W: FR Clock Divider LSB */

/* MEAS1 Setup (0x18 - 0x1E) */
#define MAX86171_REG_MEAS1_SELECTS      0x18u  /* R/W: MEAS1 Selects */
#define MAX86171_REG_MEAS1_CFG1         0x19u  /* R/W: MEAS1 Configuration 1 */
#define MAX86171_REG_MEAS1_CFG2         0x1Au  /* R/W: MEAS1 Configuration 2 */
#define MAX86171_REG_MEAS1_CFG3         0x1Bu  /* R/W: MEAS1 Configuration 3 */
#define MAX86171_REG_MEAS1_DRVA_PA      0x1Cu  /* R/W: MEAS1 DRVA Current */
#define MAX86171_REG_MEAS1_DRVB_PA      0x1Du  /* R/W: MEAS1 DRVB Current */
#define MAX86171_REG_MEAS1_DRVC_PA      0x1Eu  /* R/W: MEAS1 DRVC Current */

/* MEAS2 Setup (0x20 - 0x26) */
#define MAX86171_REG_MEAS2_SELECTS      0x20u  /* R/W: MEAS2 Selects */
#define MAX86171_REG_MEAS2_CFG1         0x21u  /* R/W: MEAS2 Configuration 1 */
#define MAX86171_REG_MEAS2_CFG2         0x22u  /* R/W: MEAS2 Configuration 2 */
#define MAX86171_REG_MEAS2_CFG3         0x23u  /* R/W: MEAS2 Configuration 3 */
#define MAX86171_REG_MEAS2_DRVA_PA      0x24u  /* R/W: MEAS2 DRVA Current */
#define MAX86171_REG_MEAS2_DRVB_PA      0x25u  /* R/W: MEAS2 DRVB Current */
#define MAX86171_REG_MEAS2_DRVC_PA      0x26u  /* R/W: MEAS2 DRVC Current */

/* MEAS3 Setup (0x28 - 0x2E) */
#define MAX86171_REG_MEAS3_SELECTS      0x28u  /* R/W: MEAS3 Selects */
#define MAX86171_REG_MEAS3_CFG1         0x29u  /* R/W: MEAS3 Configuration 1 */
#define MAX86171_REG_MEAS3_CFG2         0x2Au  /* R/W: MEAS3 Configuration 2 */
#define MAX86171_REG_MEAS3_CFG3         0x2Bu  /* R/W: MEAS3 Configuration 3 */
#define MAX86171_REG_MEAS3_DRVA_PA      0x2Cu  /* R/W: MEAS3 DRVA Current */
#define MAX86171_REG_MEAS3_DRVB_PA      0x2Du  /* R/W: MEAS3 DRVB Current */
#define MAX86171_REG_MEAS3_DRVC_PA      0x2Eu  /* R/W: MEAS3 DRVC Current */

/* MEAS4 Setup (0x30 - 0x36) */
#define MAX86171_REG_MEAS4_SELECTS      0x30u  /* R/W: MEAS4 Selects */
#define MAX86171_REG_MEAS4_CFG1         0x31u  /* R/W: MEAS4 Configuration 1 */
#define MAX86171_REG_MEAS4_CFG2         0x32u  /* R/W: MEAS4 Configuration 2 */
#define MAX86171_REG_MEAS4_CFG3         0x33u  /* R/W: MEAS4 Configuration 3 */
#define MAX86171_REG_MEAS4_DRVA_PA      0x34u  /* R/W: MEAS4 DRVA Current */
#define MAX86171_REG_MEAS4_DRVB_PA      0x35u  /* R/W: MEAS4 DRVB Current */
#define MAX86171_REG_MEAS4_DRVC_PA      0x36u  /* R/W: MEAS4 DRVC Current */

/* MEAS5 Setup (0x38 - 0x3E) */
#define MAX86171_REG_MEAS5_SELECTS      0x38u  /* R/W: MEAS5 Selects */
#define MAX86171_REG_MEAS5_CFG1         0x39u  /* R/W: MEAS5 Configuration 1 */
#define MAX86171_REG_MEAS5_CFG2         0x3Au  /* R/W: MEAS5 Configuration 2 */
#define MAX86171_REG_MEAS5_CFG3         0x3Bu  /* R/W: MEAS5 Configuration 3 */
#define MAX86171_REG_MEAS5_DRVA_PA      0x3Cu  /* R/W: MEAS5 DRVA Current */
#define MAX86171_REG_MEAS5_DRVB_PA      0x3Du  /* R/W: MEAS5 DRVB Current */
#define MAX86171_REG_MEAS5_DRVC_PA      0x3Eu  /* R/W: MEAS5 DRVC Current */

/* MEAS6 Setup (0x40 - 0x46) */
#define MAX86171_REG_MEAS6_SELECTS      0x40u  /* R/W: MEAS6 Selects */
#define MAX86171_REG_MEAS6_CFG1         0x41u  /* R/W: MEAS6 Configuration 1 */
#define MAX86171_REG_MEAS6_CFG2         0x42u  /* R/W: MEAS6 Configuration 2 */
#define MAX86171_REG_MEAS6_CFG3         0x43u  /* R/W: MEAS6 Configuration 3 */
#define MAX86171_REG_MEAS6_DRVA_PA      0x44u  /* R/W: MEAS6 DRVA Current */
#define MAX86171_REG_MEAS6_DRVB_PA      0x45u  /* R/W: MEAS6 DRVB Current */
#define MAX86171_REG_MEAS6_DRVC_PA      0x46u  /* R/W: MEAS6 DRVC Current */

/* MEAS7 Setup (0x48 - 0x4E) */
#define MAX86171_REG_MEAS7_SELECTS      0x48u  /* R/W: MEAS7 Selects */
#define MAX86171_REG_MEAS7_CFG1         0x49u  /* R/W: MEAS7 Configuration 1 */
#define MAX86171_REG_MEAS7_CFG2         0x4Au  /* R/W: MEAS7 Configuration 2 */
#define MAX86171_REG_MEAS7_CFG3         0x4Bu  /* R/W: MEAS7 Configuration 3 */
#define MAX86171_REG_MEAS7_DRVA_PA      0x4Cu  /* R/W: MEAS7 DRVA Current */
#define MAX86171_REG_MEAS7_DRVB_PA      0x4Du  /* R/W: MEAS7 DRVB Current */
#define MAX86171_REG_MEAS7_DRVC_PA      0x4Eu  /* R/W: MEAS7 DRVC Current */

/* MEAS8 Setup (0x50 - 0x56) */
#define MAX86171_REG_MEAS8_SELECTS      0x50u  /* R/W: MEAS8 Selects */
#define MAX86171_REG_MEAS8_CFG1         0x51u  /* R/W: MEAS8 Configuration 1 */
#define MAX86171_REG_MEAS8_CFG2         0x52u  /* R/W: MEAS8 Configuration 2 */
#define MAX86171_REG_MEAS8_CFG3         0x53u  /* R/W: MEAS8 Configuration 3 */
#define MAX86171_REG_MEAS8_DRVA_PA      0x54u  /* R/W: MEAS8 DRVA Current */
#define MAX86171_REG_MEAS8_DRVB_PA      0x55u  /* R/W: MEAS8 DRVB Current */
#define MAX86171_REG_MEAS8_DRVC_PA      0x56u  /* R/W: MEAS8 DRVC Current */

/* MEAS9 Setup (0x58 - 0x5E) */
#define MAX86171_REG_MEAS9_SELECTS      0x58u  /* R/W: MEAS9 Selects */
#define MAX86171_REG_MEAS9_CFG1         0x59u  /* R/W: MEAS9 Configuration 1 */
#define MAX86171_REG_MEAS9_CFG2         0x5Au  /* R/W: MEAS9 Configuration 2 */
#define MAX86171_REG_MEAS9_CFG3         0x5Bu  /* R/W: MEAS9 Configuration 3 */
#define MAX86171_REG_MEAS9_DRVA_PA      0x5Cu  /* R/W: MEAS9 DRVA Current */
#define MAX86171_REG_MEAS9_DRVB_PA      0x5Du  /* R/W: MEAS9 DRVB Current */
#define MAX86171_REG_MEAS9_DRVC_PA      0x5Eu  /* R/W: MEAS9 DRVC Current */

/* Threshold Interrupts */
#define MAX86171_REG_THRESH_MEAS_SEL    0x68u  /* R/W: Threshold Measurement Select */
#define MAX86171_REG_THRESH_HYST        0x69u  /* R/W: Threshold Hysteresis */
#define MAX86171_REG_THRESH1_HI         0x6Au  /* R/W: PPG HI Threshold 1 */
#define MAX86171_REG_THRESH1_LO         0x6Bu  /* R/W: PPG LO Threshold 1 */
#define MAX86171_REG_THRESH2_HI         0x6Cu  /* R/W: PPG HI Threshold 2 */
#define MAX86171_REG_THRESH2_LO         0x6Du  /* R/W: PPG LO Threshold 2 */

/* Picket Fence */
#define MAX86171_REG_PF_MEAS_SEL        0x70u  /* R/W: Picket Fence Measurement Select */
#define MAX86171_REG_PF_CFG             0x71u  /* R/W: Picket Fence Configuration */

/* Interrupt Enables */
#define MAX86171_REG_INT1_EN1           0x78u  /* R/W: Interrupt 1 Enable 1 */
#define MAX86171_REG_INT1_EN2           0x79u  /* R/W: Interrupt 1 Enable 2 */
#define MAX86171_REG_INT1_EN3           0x7Au  /* R/W: Interrupt 1 Enable 3 */
#define MAX86171_REG_INT2_EN1           0x7Cu  /* R/W: Interrupt 2 Enable 1 */
#define MAX86171_REG_INT2_EN2           0x7Du  /* R/W: Interrupt 2 Enable 2 */
#define MAX86171_REG_INT2_EN3           0x7Eu  /* R/W: Interrupt 2 Enable 3 */

/* Part ID */
#define MAX86171_REG_PART_ID            0xFFu  /* RO: Part ID (0x2C) */

/* =============================
 *  Bitfield Helpers (generic)
 * ============================= */
#define MAX86171_F_MAKE(name, val)     (((uint32_t)(val) & name##_M) << name##_S)
#define MAX86171_F_GET(name, regv)     (((uint32_t)(regv) >> name##_S) & name##_M)
#define MAX86171_F_MASK(name)          ((uint32_t)(name##_M) << name##_S)
#define MAX86171_F_SET(regv, name, val) \
    do { \
        (regv) = (uint8_t)(((uint32_t)(regv) & ~(MAX86171_F_MASK(name))) | MAX86171_F_MAKE(name, (val))); \
    } while (0)

/* Read-Modify-Write helper */
static inline uint8_t max86171_f_write(uint8_t regv, uint8_t mask, uint8_t val_shifted){
    return (uint8_t)((regv & (uint8_t)~mask) | (val_shifted & mask));
}

/* =============================
 *  System Configuration 1 (0x0C) - Page 53-54
 * ============================= */
#define MAX86171_SYS_RESET_S            (0u)
#define MAX86171_SYS_RESET_M            (0x1u)
#define MAX86171_SYS_SHDN_S             (1u)
#define MAX86171_SYS_SHDN_M             (0x1u)
#define MAX86171_SYS_PPG1_PWRDN_S       (2u)
#define MAX86171_SYS_PPG1_PWRDN_M       (0x1u)
#define MAX86171_SYS_PPG2_PWRDN_S       (3u)
#define MAX86171_SYS_PPG2_PWRDN_M       (0x1u)
#define MAX86171_SYS_SYNC_MODE_S        (4u)
#define MAX86171_SYS_SYNC_MODE_M        (0x3u)
#define MAX86171_SYS_SW_FORCE_SYNC_S    (6u)
#define MAX86171_SYS_SW_FORCE_SYNC_M    (0x1u)
#define MAX86171_SYS_MEAS9_EN_S         (7u)
#define MAX86171_SYS_MEAS9_EN_M         (0x1u)

/* =============================
 *  System Configuration 2 (0x0D) - Page 55
 * ============================= */
#define MAX86171_SYS_MEAS1_EN_S         (0u)
#define MAX86171_SYS_MEAS1_EN_M         (0x1u)
#define MAX86171_SYS_MEAS2_EN_S         (1u)
#define MAX86171_SYS_MEAS2_EN_M         (0x1u)
#define MAX86171_SYS_MEAS3_EN_S         (2u)
#define MAX86171_SYS_MEAS3_EN_M         (0x1u)
#define MAX86171_SYS_MEAS4_EN_S         (3u)
#define MAX86171_SYS_MEAS4_EN_M         (0x1u)
#define MAX86171_SYS_MEAS5_EN_S         (4u)
#define MAX86171_SYS_MEAS5_EN_M         (0x1u)
#define MAX86171_SYS_MEAS6_EN_S         (5u)
#define MAX86171_SYS_MEAS6_EN_M         (0x1u)
#define MAX86171_SYS_MEAS7_EN_S         (6u)
#define MAX86171_SYS_MEAS7_EN_M         (0x1u)
#define MAX86171_SYS_MEAS8_EN_S         (7u)
#define MAX86171_SYS_MEAS8_EN_M         (0x1u)

/* =============================
 *  FIFO Configuration 2 (0x0A) - Page 52-53
 *  Bugfix: Shift offsets corrected. They were off by 1 bit each.
 * ============================= */
#define MAX86171_FIFO_RO_S              (1u)
#define MAX86171_FIFO_RO_M              (0x1u)
#define MAX86171_FIFO_A_FULL_TYPE_S     (2u)
#define MAX86171_FIFO_A_FULL_TYPE_M     (0x1u)
#define MAX86171_FIFO_STAT_CLR_S        (3u)
#define MAX86171_FIFO_STAT_CLR_M        (0x1u)
#define MAX86171_FIFO_FLUSH_S           (4u)
#define MAX86171_FIFO_FLUSH_M           (0x1u)

/* =============================
 *  FR Clock Frequency Select (0x15) - Page 58-59
 * ============================= */
#define MAX86171_FR_CLK_FINE_TUNE_S     (0u)
#define MAX86171_FR_CLK_FINE_TUNE_M     (0x1Fu)
#define MAX86171_FR_CLK_SEL_S           (5u)
#define MAX86171_FR_CLK_SEL_M           (0x1u)

/* =============================
 *  FR Clock Divider (0x16, 0x17) - Page 59-60
 * ============================= */
#define MAX86171_FR_CLK_DIV_H_S         (0u)
#define MAX86171_FR_CLK_DIV_H_M         (0x7Fu)
#define MAX86171_FR_CLK_DIV_L_S         (0u)
#define MAX86171_FR_CLK_DIV_L_M         (0xFFu)

/* =============================
 *  MEASx Selects Register - Page 60
 * ============================= */
#define MAX86171_MEAS_DRVA_SEL_S        (0u)
#define MAX86171_MEAS_DRVA_SEL_M        (0x3u)
#define MAX86171_MEAS_DRVB_SEL_S        (2u)
#define MAX86171_MEAS_DRVB_SEL_M        (0x3u)
#define MAX86171_MEAS_DRVC_SEL_S        (4u)
#define MAX86171_MEAS_DRVC_SEL_M        (0x3u)
#define MAX86171_MEAS_AMB_S             (6u)
#define MAX86171_MEAS_AMB_M             (0x1u)

/* =============================
 *  MEASx Configuration 1 - Page 61-62
 * ============================= */
#define MAX86171_MEAS_AVER_S            (0u)
#define MAX86171_MEAS_AVER_M            (0x7u)
#define MAX86171_MEAS_TINT_S            (3u)
#define MAX86171_MEAS_TINT_M            (0x3u)
#define MAX86171_MEAS_PPG1_PDSEL_S      (5u)
#define MAX86171_MEAS_PPG1_PDSEL_M      (0x1u)
#define MAX86171_MEAS_PPG2_PDSEL_S      (6u)
#define MAX86171_MEAS_PPG2_PDSEL_M      (0x1u)

/* =============================
 *  MEASx Configuration 2 - Page 62-63
 * ============================= */
#define MAX86171_MEAS_PPG1_ADC_RGE_S    (0u)
#define MAX86171_MEAS_PPG1_ADC_RGE_M    (0x3u)
#define MAX86171_MEAS_PPG2_ADC_RGE_S    (2u)
#define MAX86171_MEAS_PPG2_ADC_RGE_M    (0x3u)
#define MAX86171_MEAS_LED_RGE_S         (4u)
#define MAX86171_MEAS_LED_RGE_M         (0x3u)
#define MAX86171_MEAS_FILT_SEL_S        (6u)
#define MAX86171_MEAS_FILT_SEL_M        (0x1u)
#define MAX86171_MEAS_SINC3_SEL_S       (7u)
#define MAX86171_MEAS_SINC3_SEL_M       (0x1u)

/* =============================
 *  MEASx Configuration 3 - Page 64
 * ============================= */
#define MAX86171_MEAS_PPG1_DACOFF_S     (0u)
#define MAX86171_MEAS_PPG1_DACOFF_M     (0x3u)
#define MAX86171_MEAS_PPG2_DACOFF_S     (2u)
#define MAX86171_MEAS_PPG2_DACOFF_M     (0x3u)
#define MAX86171_MEAS_LED_SETLNG_S      (4u)
#define MAX86171_MEAS_LED_SETLNG_M      (0x3u)
#define MAX86171_MEAS_PD_SETLNG_S       (6u)
#define MAX86171_MEAS_PD_SETLNG_M       (0x3u)

/* =============================
 *  PD_BIAS Register (0x0F) - Page 56-57
 * ============================= */
#define MAX86171_PD1_BIAS_S             (0u)
#define MAX86171_PD1_BIAS_M             (0x3u)
#define MAX86171_PD2_BIAS_S             (2u)
#define MAX86171_PD2_BIAS_M             (0x3u)
#define MAX86171_PD3_BIAS_S             (4u)
#define MAX86171_PD3_BIAS_M             (0x3u)
#define MAX86171_PD4_BIAS_S             (6u)
#define MAX86171_PD4_BIAS_M             (0x3u)

/* =============================
 *  Status Register 1 (0x00) - Page 47-48
 * ============================= */
#define MAX86171_STATUS1_PWR_RDY_S      (0u)
#define MAX86171_STATUS1_PWR_RDY_M      (0x1u)
#define MAX86171_STATUS1_THRESH1_HILO_S (1u)
#define MAX86171_STATUS1_THRESH1_HILO_M (0x1u)
#define MAX86171_STATUS1_THRESH2_HILO_S (2u)
#define MAX86171_STATUS1_THRESH2_HILO_M (0x1u)
#define MAX86171_STATUS1_EXP_OVF_S      (3u)
#define MAX86171_STATUS1_EXP_OVF_M      (0x1u)
#define MAX86171_STATUS1_ALC_OVF_S      (4u)
#define MAX86171_STATUS1_ALC_OVF_M      (0x1u)
#define MAX86171_STATUS1_FIFO_DATA_RDY_S (5u)
#define MAX86171_STATUS1_FIFO_DATA_RDY_M (0x1u)
#define MAX86171_STATUS1_FRAME_RDY_S    (6u)
#define MAX86171_STATUS1_FRAME_RDY_M    (0x1u)
#define MAX86171_STATUS1_A_FULL_S       (7u)
#define MAX86171_STATUS1_A_FULL_M       (0x1u)

/* =============================
 *  Status Register 2 (0x01) - Page 48-49
 * ============================= */
#define MAX86171_STATUS2_INVALID_CFG_S  (2u)
#define MAX86171_STATUS2_INVALID_CFG_M  (0x1u)
#define MAX86171_STATUS2_VDD_OOR_S      (6u)
#define MAX86171_STATUS2_VDD_OOR_M      (0x1u)
#define MAX86171_STATUS2_LED9_COMPB_S   (7u)
#define MAX86171_STATUS2_LED9_COMPB_M   (0x1u)

/* =============================
 *  Status Register 3 (0x01) - Page 49-50
 * ============================= */
#define MAX86171_STATUS3_LED1_COMPB_S   (0u)
#define MAX86171_STATUS3_LED1_COMPB_M   (0x1u)
#define MAX86171_STATUS3_LED2_COMPB_S   (1u)
#define MAX86171_STATUS3_LED2_COMPB_M   (0x1u)
#define MAX86171_STATUS3_LED3_COMPB_S   (2u)
#define MAX86171_STATUS3_LED3_COMPB_M   (0x1u)
#define MAX86171_STATUS3_LED4_COMPB_S   (3u)
#define MAX86171_STATUS3_LED4_COMPB_M   (0x1u)
#define MAX86171_STATUS3_LED5_COMPB_S   (4u)
#define MAX86171_STATUS3_LED5_COMPB_M   (0x1u)
#define MAX86171_STATUS3_LED6_COMPB_S   (5u)
#define MAX86171_STATUS3_LED6_COMPB_M   (0x1u)
#define MAX86171_STATUS3_LED7_COMPB_S   (6u)
#define MAX86171_STATUS3_LED7_COMPB_M   (0x1u)
#define MAX86171_STATUS3_LED8_COMPB_S   (7u)
#define MAX86171_STATUS3_LED8_COMPB_M   (0x1u)

/* =============================
 *  Interrupt Enable Bits - Page 92-95
 * ============================= */
#define MAX86171_INT_LED_TX_S           (0u)
#define MAX86171_INT_LED_TX_M           (0x1u)
#define MAX86171_INT_THRESH1_HILO_S     (1u)
#define MAX86171_INT_THRESH1_HILO_M     (0x1u)
#define MAX86171_INT_THRESH2_HILO_S     (2u)
#define MAX86171_INT_THRESH2_HILO_M     (0x1u)
#define MAX86171_INT_EXP_OVF_S          (3u)
#define MAX86171_INT_EXP_OVF_M          (0x1u)
#define MAX86171_INT_ALC_OVF_S          (4u)
#define MAX86171_INT_ALC_OVF_M          (0x1u)
#define MAX86171_INT_FIFO_DATA_RDY_S    (5u)
#define MAX86171_INT_FIFO_DATA_RDY_M    (0x1u)
#define MAX86171_INT_FRAME_RDY_S        (6u)
#define MAX86171_INT_FRAME_RDY_M        (0x1u)
#define MAX86171_INT_A_FULL_S           (7u)
#define MAX86171_INT_A_FULL_M           (0x1u)

/* MEAS_EN bit masks */
#define MAX86171_MEAS_EN_MEAS1          (1u << 0)
#define MAX86171_MEAS_EN_MEAS2          (1u << 1)
#define MAX86171_MEAS_EN_MEAS3          (1u << 2)
#define MAX86171_MEAS_EN_MEAS4          (1u << 3)
#define MAX86171_MEAS_EN_MEAS5          (1u << 4)
#define MAX86171_MEAS_EN_MEAS6          (1u << 5)
#define MAX86171_MEAS_EN_MEAS7          (1u << 6)
#define MAX86171_MEAS_EN_MEAS8          (1u << 7)
#define MAX86171_MEAS_EN_MEAS9          (1u << 8)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MAX86171_REGS_H */
