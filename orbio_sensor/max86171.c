/**
 * @file max86171.c
 * @brief STM32-specific driver implementation for MAX86171 Optical AFE
 *
 * Updated to use correct register addresses from datasheet Rev 4
 *
 * SPI Protocol:
 * - Write: CS low -> [Reg Addr] [Data bytes...] -> CS high
 * - Read:  CS low -> [Reg Addr | 0x80] [dummy/data bytes...] -> CS high
 */

#include "max86171.h"
#include "sl_spidrv_instances.h"

#define PRINT_MAX86171_ERROR printf
#define PRINT_MAX86171_INFO printf

/* =============================
 *        Private Functions
 * ============================= */

// Maximum SPI clock speed of 25MHz

/**
 * @brief Write to MAX86171 register via SPI
 */
static int prv_write(max86171_t *dev, uint8_t reg, const uint8_t *data, size_t len) {
    if (!dev) return MAX86171_E_NULL;
    Ecode_t retVal1, retVal2;

    uint8_t tx_buf[2] = {0}; // 1 byte addr + 1 byte mode

    // Byte 0: Register address
    tx_buf[0] = reg;
    // Byte 1: Write command (W = 0, don't care bits = 0)
    tx_buf[1] = 0;

    // CS low
    max86171_cs_low();
    /* At least tCSS0=20ns from CSB fall to SCLK rise */
    // Send address and command type
    retVal1 = SPIDRV_MTransmitB (sl_spidrv_inst0_handle, tx_buf, 2);
    if (retVal1) { PRINT_MAX86171_ERROR ("prv_write retVal1 = %ld\n\r", retVal1); }
    // Transmit the bytes
    retVal2 = SPIDRV_MTransmitB (sl_spidrv_inst0_handle, data, len);
    if (retVal2) { PRINT_MAX86171_ERROR ("prv_write retVal2 = %ld\n\r", retVal2); }
    /* At least tCSH1=20ns from last SCLK rise to CSB rise */
    // CS high
    max86171_cs_high();
    /* At least tCSF=60ns from last SCLK rise to next CSB falling edge (new transaction) */

    if (retVal1 || retVal2) return MAX86171_E_BUS;
    else return MAX86171_OK;
}

/**
 * @brief Read from MAX86171 register via SPI
 *
 * Protocol: [Address(8-bit)] [Read Command(8-bit)] [Received Data(8-bit)]
 * - Byte 1: Register address (A7-A0)
 * - Byte 2: 0x01 (Read command, R bit = 1)
 * - Byte 3: Received data (DO7-DO0)
 * Total: 24 clock cycles per register read
 *
 * Note: Data is accessed on 16th SCLK rising edge and presented on
 * following SCLK falling edge for capture on 17th rising edge
 */
static int prv_read(max86171_t *dev, uint8_t reg, uint8_t *data, size_t len) {
    if (!dev) return MAX86171_E_NULL;
    Ecode_t retVal;

    // Single-byte read only (per datasheet protocol)
    // Multi-byte reads require multiple 24-clock transactions

    uint8_t tx_buf[3];       // 1 byte addr + 1 byte mode + 1 byte dummy
    uint8_t rx_buf[3] = {0}; // 2 bytes dummy + 1 byte read byte

    // Byte 0: Register address
    tx_buf[0] = reg;
    // Byte 1: Read command (R = 1, don't care bits = 0)
    tx_buf[1] = 0x80;
    // Byte 2: Dummy
    tx_buf[2] = 0x00;

    // CS low
    max86171_cs_low();
    // Transmit and receive 3 bytes
    retVal = SPIDRV_MTransferB(sl_spidrv_inst0_handle, tx_buf, rx_buf, len+2);
    if (retVal) { PRINT_MAX86171_ERROR ("prv_read retVal = %ld\n\r", retVal); }
    // CS high
    max86171_cs_high();

    // Extract received data from byte 3
    data[0] = rx_buf[2];

    if (retVal) return MAX86171_E_BUS;
    else return MAX86171_OK;
}

/**
 * @brief Burst read from MAX86171 FIFO via SPI DMA (non-blocking)
 *
 * @param dev Pointer to MAX86171 device structure
 * @param data Buffer to store FIFO data (must be at least num_bytes size)
 * @param num_bytes Number of bytes to read from FIFO
 * @return MAX86171_OK on success, error code otherwise
 *
 */

static int prv_fifo_burst_read(max86171_t *dev, uint8_t *data, size_t num_bytes) {
    if (!dev) return MAX86171_E_NULL;
    Ecode_t retVal1, retVal2;

    // CS high for safety
    max86171_cs_high();

    uint8_t tx_buf[2]; // 1 byte addr + 1 byte read mode
    tx_buf[0] = MAX86171_REG_FIFO_DATA;
    tx_buf[1] = 0x80;

    // CS low
    max86171_cs_low();
    // Send address and command type
    retVal1 = SPIDRV_MTransmitB (sl_spidrv_inst0_handle, tx_buf, 2);
    if (retVal1) { PRINT_MAX86171_ERROR ("prv_fifo_burst_read retVal1 = %ld\n\r", retVal1); }
    // Read the FIFO data
    retVal2 = SPIDRV_MReceiveB(sl_spidrv_inst0_handle, data, num_bytes);
    if (retVal2) { PRINT_MAX86171_ERROR ("prv_fifo_burst_read retVal2 = %ld\n\r", retVal2); }
    // CS high
    max86171_cs_high();

    if (retVal1 || retVal2) return MAX86171_E_BUS;
    else return MAX86171_OK;
}

#ifdef UNUSED

/**
 * @brief Burst read from MAX86171 FIFO via SPI
 *
 * FIFO burst read protocol:
 * - Keep CS low for entire burst operation
 * - Send initial read command to FIFO register
 * - Continue clocking to read consecutive bytes
 * - FIFO pointer auto-increments with each byte read
 *
 * FIFO data format (3 bytes per sample):
 * - Byte 0: TAG (identifies measurement source)
 * - Byte 1: DATA[17:10] (MSB)
 * - Byte 2: DATA[9:2] (LSB)
 *
 * @param dev Pointer to MAX86171 device structure
 * @param data Buffer to store FIFO data (must be at least num_bytes size)
 * @param num_bytes Number of bytes to read from FIFO
 * @return MAX86171_OK on success, error code otherwise
 */


static int __attribute__((unused)) prv_fifo_burst_read(max86171_t *dev, uint8_t *data, size_t num_bytes) {

//static int prv_fifo_burst_read(max86171_t *dev, uint8_t *data, size_t num_bytes) {
    if (!dev) return MAX86171_E_NULL;
    if (!data || num_bytes == 0) return MAX86171_E_ARG;

    HAL_StatusTypeDef status;

    // Initial command: [FIFO_REG][0x80][dummy]
    uint8_t cmd_buf[2] = {0};

    cmd_buf[0] = MAX86171_REG_FIFO_DATA;  // FIFO register address (0x00)
    cmd_buf[1] = 0x80;                     // Read command

    // CS low - keep it low for entire burst
    max86171_cs_low();

    // Send initial read command (24 clock cycles)
    status = HAL_SPI_TransmitReceive(dev->hspi, cmd_buf, data, num_bytes+2, dev->spi_timeout_ms);
    if (status != HAL_OK) {
        max86171_cs_high();
        return MAX86171_E_BUS;
    }

    // CS high - end of burst
    max86171_cs_high();

    return MAX86171_OK;
}

#endif

/* =============================
 *        Public: Register R/W
 * ============================= */

int max86171_write_reg(max86171_t *dev, uint8_t reg, const uint8_t *data, size_t len) {
    if (!dev || !data) return MAX86171_E_NULL;
    if (len == 0) return MAX86171_E_ARG;
    return prv_write(dev, reg, data, len);
}

int max86171_read_reg(max86171_t *dev, uint8_t reg, uint8_t *data, size_t len) {
    if (!dev || !data) return MAX86171_E_NULL;
    if (len != 1) return MAX86171_E_ARG;
    return prv_read(dev, reg, data, len);
}

int max86171_read_FIFO(max86171_t *dev, uint8_t *data, size_t len) {
    if (!dev || !data) return MAX86171_E_NULL;
    if (len == 0) return MAX86171_E_ARG;
    return prv_fifo_burst_read(dev, data, len);
}

#ifdef UNUSED

int max86171_rmw_bits(max86171_t *dev, uint8_t reg, uint8_t mask, uint8_t value_shifted) {
    if (!dev) return MAX86171_E_NULL;

    uint8_t v = 0;
    int rc = max86171_read_reg(dev, reg, &v, 1);
    if (rc != MAX86171_OK) return rc;

    v = (v & ~mask) | (value_shifted & mask);

    return max86171_write_reg(dev, reg, &v, 1);
}

#endif

/* =============================
 *        Public: Lifecycle
 * ============================= */

int max86171_init(max86171_t *dev) {
    if (!dev) return MAX86171_E_NULL;

    // Small delay for chip to be ready
    HAL_Delay(10);

    // Read Part ID (should be 0x2C for MAX86171)
    uint8_t part_id = 0;
    int rc = max86171_read_reg(dev, MAX86171_REG_PART_ID, &part_id, 1);
    if (rc != MAX86171_OK) return rc;

    PRINT_MAX86171_INFO ("MAX86171 part ID read as 0x%02x, expected 0x2c\n\r", part_id);

    if (part_id == 0x2C)
        return MAX86171_OK;
    else
        return MAX86171_E_BUS;
}

int max86171_soft_reset(max86171_t *dev) {
    if (!dev) return MAX86171_E_NULL;

    uint8_t v = 0;
    int rc = max86171_read_reg(dev, MAX86171_REG_SYS_CFG1, &v, 1);
    if (rc != MAX86171_OK) return rc;

    // Set RESET bit (bit 0)
    MAX86171_F_SET(v, MAX86171_SYS_RESET, 1);
    rc = max86171_write_reg(dev, MAX86171_REG_SYS_CFG1, &v, 1);
    if (rc != MAX86171_OK) return rc;

    // Wait for reset to complete
    HAL_Delay(100);

    return MAX86171_OK;
}

/* =============================
 *        Public: Clock & FPS
 * ============================= */

#ifdef UNUSED

int max86171_set_sync_mode(max86171_t *dev, int mode) {
    if (!dev) return MAX86171_E_NULL;

    uint8_t v = 0;
    int rc = max86171_read_reg(dev, MAX86171_REG_SYS_CFG1, &v, 1);
    if (rc != MAX86171_OK) return rc;

    MAX86171_F_SET(v, MAX86171_SYS_SYNC_MODE, (uint8_t)mode);

    return max86171_write_reg(dev, MAX86171_REG_SYS_CFG1, &v, 1);
}

int max86171_set_frame_rate(max86171_t *dev, uint32_t fps, bool use_32768Hz) {
    if (!dev) return MAX86171_E_NULL;
    if (fps == 0) return MAX86171_E_ARG;

    // Calculate divider from base clock
    uint32_t base = use_32768Hz ? 32768U : 32000U;
    uint32_t div = base / fps;

    // Clamp to maximum divider value (15-bit)
    if (div > 0x7FFF) {
        div = 0x7FFF;
    }
    // Minimum divider is 11
    if (div < 11) {
        div = 11;
    }

    // Read current clock frequency select register
    uint8_t freq_sel = 0;
    int rc = max86171_read_reg(dev, MAX86171_REG_FR_CLK_FREQ_SEL, &freq_sel, 1);
    if (rc != MAX86171_OK) return rc;

    // Set clock source (bit 5)
    MAX86171_F_SET(freq_sel, MAX86171_FR_CLK_SEL, use_32768Hz ? 1 : 0);
    rc = max86171_write_reg(dev, MAX86171_REG_FR_CLK_FREQ_SEL, &freq_sel, 1);
    if (rc != MAX86171_OK) return rc;

    // Write divider MSB (bits [14:8])
    uint8_t div_msb = (uint8_t)((div >> 8) & 0x7F);
    rc = max86171_write_reg(dev, MAX86171_REG_FR_CLK_DIV_MSB, &div_msb, 1);
    if (rc != MAX86171_OK) return rc;

    // Write divider LSB (bits [7:0])
    uint8_t div_lsb = (uint8_t)(div & 0xFF);
    rc = max86171_write_reg(dev, MAX86171_REG_FR_CLK_DIV_LSB, &div_lsb, 1);

    return rc;
}

#endif

/* =============================
 *        Public: FIFO helpers
 * ============================= */

int max86171_fifo_flush(max86171_t *dev) {
    if (!dev) return MAX86171_E_NULL;

    // Use FLUSH_FIFO bit in FIFO Configuration 2 register (0x0A, bit 3)
    uint8_t cfg2 = 0;
    int rc = max86171_read_reg(dev, MAX86171_REG_FIFO_CFG2, &cfg2, 1);
    if (rc != MAX86171_OK) return rc;

    // Set FLUSH_FIFO bit
    MAX86171_F_SET(cfg2, MAX86171_FIFO_FLUSH, 1);
    rc = max86171_write_reg(dev, MAX86171_REG_FIFO_CFG2, &cfg2, 1);

    return rc;
}

#ifdef UNUSED

int max86171_fifo_discard_until_first_tag(max86171_t *dev, uint8_t first_tag) {
    if (!dev) return MAX86171_E_NULL;

    uint8_t sample[3];

    // Safety limit to prevent infinite loop
    for (int i = 0; i < 4096; i++) {
        int rc = max86171_read_reg(dev, MAX86171_REG_FIFO_DATA, sample, 3);
        if (rc != MAX86171_OK) return rc;

        uint8_t tag = (sample[0] >> 4) & 0x0F;

        if (tag == first_tag) {
            return MAX86171_OK;
        }
    }

    return MAX86171_E_TIMEOUT;
}

int max86171_fifo_read_samples(max86171_t *dev,
                               uint16_t max_items,
                               int *out_count,
                               uint8_t *tags,
                               int32_t *ch1,
                               int32_t *ch2) {
    if (!dev || !out_count || !tags || !ch1 || !ch2) {
        return MAX86171_E_NULL;
    }

    // Read FIFO count from FIFO Counter 2 (0x07) and Counter 1 (0x06 bit 7)
    uint8_t cnt_lsb = 0, cnt_msb_reg = 0;
    int rc = max86171_read_reg(dev, MAX86171_REG_FIFO_COUNTER1, &cnt_msb_reg, 1);
    if (rc != MAX86171_OK) return rc;

    rc = max86171_read_reg(dev, MAX86171_REG_FIFO_COUNTER2, &cnt_lsb, 1);
    if (rc != MAX86171_OK) return rc;

    // Bit 7 of Counter1 is MSB of count, bits 0-6 are overflow counter
    uint16_t fifo_count = ((uint16_t)(cnt_msb_reg & 0x80) << 1) | cnt_lsb;

    uint16_t to_read = fifo_count;
    if (to_read > max_items) {
        to_read = max_items;
    }

    // Read samples
    for (uint16_t i = 0; i < to_read; i++) {
        uint8_t sample[3];
        rc = max86171_read_reg(dev, MAX86171_REG_FIFO_DATA, sample, 3);
        if (rc != MAX86171_OK) return rc;

        // Extract tag (upper 4 bits of first byte)
        uint8_t tag = (sample[0] >> 4) & 0x0F;

        // Extract 20-bit value (lower 4 bits of first byte + 2nd and 3rd bytes)
        uint32_t raw20 = ((uint32_t)(sample[0] & 0x0F) << 16) |
                         ((uint32_t)sample[1] << 8) |
                         ((uint32_t)sample[2]);

        // Sign-extend to 32-bit
        int32_t value = max86171_sext20(raw20);

        tags[i] = tag;
        ch1[i] = value;
        ch2[i] = value;  // Both channels get same value; user must separate based on sequence
    }

    *out_count = (int)to_read;
    return MAX86171_OK;
}

/* =============================
 *        Public: Measurement
 * ============================= */

/**
 * @brief Calculate base register address for MEASx_SELECTS
 * @param meas_idx Measurement index (1-9)
 * @return Register address for SELECTS register
 */
static uint8_t meas_selects_addr(int meas_idx) {
    // MEAS1_SELECTS is at 0x18, spacing is 8 bytes between measurements
    return 0x18 + (uint8_t)((meas_idx - 1) * 8);
}

int max86171_configure_measurement(max86171_t *dev, int meas_idx, const max86171_meas_cfg_t *cfg) {
    if (!dev || !cfg) return MAX86171_E_NULL;
    if (meas_idx < 1 || meas_idx > 9) return MAX86171_E_ARG;

    uint8_t base = meas_selects_addr(meas_idx);
    int rc;

    // SELECTS register: LED driver selections and AMB bit
    uint8_t selects = 0;
    MAX86171_F_SET(selects, MAX86171_MEAS_DRVA_SEL, cfg->drvA_sel);
    MAX86171_F_SET(selects, MAX86171_MEAS_DRVB_SEL, cfg->drvB_sel);
    MAX86171_F_SET(selects, MAX86171_MEAS_DRVC_SEL, cfg->drvC_sel);
    MAX86171_F_SET(selects, MAX86171_MEAS_AMB, cfg->ambient_direct ? 1 : 0);
    rc = max86171_write_reg(dev, base, &selects, 1);
    if (rc != MAX86171_OK) return rc;

    // CFG1: PD selections, integration time, averaging
    uint8_t cfg1 = 0;
    MAX86171_F_SET(cfg1, MAX86171_MEAS_AVER, cfg->averages);
    MAX86171_F_SET(cfg1, MAX86171_MEAS_TINT, (uint8_t)cfg->tint);
    MAX86171_F_SET(cfg1, MAX86171_MEAS_PPG1_PDSEL, cfg->ppg1_pdsel & 0x1);
    MAX86171_F_SET(cfg1, MAX86171_MEAS_PPG2_PDSEL, cfg->ppg2_pdsel & 0x1);
    rc = max86171_write_reg(dev, base + 1, &cfg1, 1);
    if (rc != MAX86171_OK) return rc;

    // CFG2: ADC ranges, LED range, filter selection
    uint8_t cfg2 = 0;
    MAX86171_F_SET(cfg2, MAX86171_MEAS_PPG1_ADC_RGE, (uint8_t)cfg->ppg1_adc);
    MAX86171_F_SET(cfg2, MAX86171_MEAS_PPG2_ADC_RGE, (uint8_t)cfg->ppg2_adc);
    MAX86171_F_SET(cfg2, MAX86171_MEAS_LED_RGE, (uint8_t)cfg->led_range);
    MAX86171_F_SET(cfg2, MAX86171_MEAS_FILT_SEL, cfg->use_cdm ? 0 : 1);
    rc = max86171_write_reg(dev, base + 2, &cfg2, 1);
    if (rc != MAX86171_OK) return rc;

    // CFG3: DAC offsets, settling times
    uint8_t cfg3 = 0;
    MAX86171_F_SET(cfg3, MAX86171_MEAS_PPG1_DACOFF, 0);  // Default to 0
    MAX86171_F_SET(cfg3, MAX86171_MEAS_PPG2_DACOFF, 0);  // Default to 0
    MAX86171_F_SET(cfg3, MAX86171_MEAS_LED_SETLNG, 1);   // 12μs default
    MAX86171_F_SET(cfg3, MAX86171_MEAS_PD_SETLNG, 1);    // 12.1μs default
    rc = max86171_write_reg(dev, base + 3, &cfg3, 1);
    if (rc != MAX86171_OK) return rc;

    // Write LED current values
    rc = max86171_write_reg(dev, base + 4, &cfg->drvA_pa, 1);
    if (rc != MAX86171_OK) return rc;

    rc = max86171_write_reg(dev, base + 5, &cfg->drvB_pa, 1);
    if (rc != MAX86171_OK) return rc;

    rc = max86171_write_reg(dev, base + 6, &cfg->drvC_pa, 1);

    return rc;
}

int max86171_enable_measurements(max86171_t *dev, uint16_t meas_bitmask) {
    if (!dev) return MAX86171_E_NULL;

    // Read current System Configuration 1 register
    uint8_t sys_cfg1 = 0;
    int rc = max86171_read_reg(dev, MAX86171_REG_SYS_CFG1, &sys_cfg1, 1);
    if (rc != MAX86171_OK) return rc;

    // Set MEAS9_EN bit (bit 7 of Sys Cfg 1)
    if (meas_bitmask & MAX86171_MEAS_EN_MEAS9) {
        MAX86171_F_SET(sys_cfg1, MAX86171_SYS_MEAS9_EN, 1);
    } else {
        MAX86171_F_SET(sys_cfg1, MAX86171_SYS_MEAS9_EN, 0);
    }
    rc = max86171_write_reg(dev, MAX86171_REG_SYS_CFG1, &sys_cfg1, 1);
    if (rc != MAX86171_OK) return rc;

    // Write lower 8 bits to System Configuration 2 register (MEAS1-8)
    uint8_t sys_cfg2 = (uint8_t)(meas_bitmask & 0xFF);
    rc = max86171_write_reg(dev, MAX86171_REG_SYS_CFG2, &sys_cfg2, 1);

    return rc;
}

/* =============================
 *        Public: PD/LED utils
 * ============================= */

int max86171_set_pd_bias(max86171_t *dev, int pd_idx, max86171_pd_bias_t bias) {
    if (!dev) return MAX86171_E_NULL;
    if (pd_idx < 1 || pd_idx > 4) return MAX86171_E_ARG;

    uint8_t reg = 0;
    int rc = max86171_read_reg(dev, MAX86171_REG_PD_BIAS, &reg, 1);
    if (rc != MAX86171_OK) return rc;

    switch (pd_idx) {
        case 1:
            MAX86171_F_SET(reg, MAX86171_PD1_BIAS, (uint8_t)bias);
            break;
        case 2:
            MAX86171_F_SET(reg, MAX86171_PD2_BIAS, (uint8_t)bias);
            break;
        case 3:
            MAX86171_F_SET(reg, MAX86171_PD3_BIAS, (uint8_t)bias);
            break;
        case 4:
            MAX86171_F_SET(reg, MAX86171_PD4_BIAS, (uint8_t)bias);
            break;
        default:
            return MAX86171_E_ARG;
    }

    return max86171_write_reg(dev, MAX86171_REG_PD_BIAS, &reg, 1);
}

int max86171_set_led_range(max86171_t *dev, max86171_led_range_t range) {
    if (!dev) return MAX86171_E_NULL;

    // Note: LED range is set per-measurement in CFG2, not globally
    // This function would need to iterate through all measurements
    // For now, return OK as range is set in configure_measurement

    return MAX86171_OK;
}

/* =============================
 *        Public: IRQ
 * ============================= */

int max86171_irq_config(max86171_t *dev, uint32_t int1_en_mask, uint32_t int2_en_mask) {
    if (!dev) return MAX86171_E_NULL;

    // Write INT1_EN1 register (primary interrupt enables)
    uint8_t int1_en1 = (uint8_t)(int1_en_mask & 0xFF);
    int rc = max86171_write_reg(dev, MAX86171_REG_INT1_EN1, &int1_en1, 1);
    if (rc != MAX86171_OK) return rc;

    // Write INT2_EN1 register (primary interrupt enables)
    uint8_t int2_en1 = (uint8_t)(int2_en_mask & 0xFF);
    rc = max86171_write_reg(dev, MAX86171_REG_INT2_EN1, &int2_en1, 1);

    return rc;
}

#endif

int max86171_irq_read_clear(max86171_t *dev, max86171_irq_status_t *out) {
    if (!dev || !out) return MAX86171_E_NULL;

    uint8_t s1 = 0, s2 = 0, s3 = 0;

    // Read status registers (reading clears them)
    int rc = max86171_read_reg(dev, MAX86171_REG_STATUS1, &s1, 1);
    if (rc != MAX86171_OK) return rc;

    rc = max86171_read_reg(dev, MAX86171_REG_STATUS2, &s2, 1);
    if (rc != MAX86171_OK) return rc;

    rc = max86171_read_reg(dev, MAX86171_REG_STATUS3, &s3, 1);
    if (rc != MAX86171_OK) return rc;

    PRINT_MAX86171_INFO ("max86171_irq_read_clear: %02x, %02x, %02x\n\r", s1, s2, s3);

    // These are not used
    out->int1 = (s1 != 0);
    out->int2 = (s2 != 0);
    out->status1 = s1;
    out->status2 = s2;
    out->status3 = s3;

    if (MAX86171_F_GET(MAX86171_STATUS2_VDD_OOR, s2) != 0)
    {
        PRINT_MAX86171_ERROR ("max86171_irq_read_clear ERROR: VDD_OOR = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS2_INVALID_CFG, s2) != 0)
    {
        PRINT_MAX86171_ERROR ("max86171_irq_read_clear ERROR: INVALID_CFG = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS2_LED9_COMPB, s2) != 0)
    {
        PRINT_MAX86171_ERROR ("max86171_irq_read_clear ERROR: LED9_COMPB = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS3_LED8_COMPB, s3) != 0)
    {
        PRINT_MAX86171_ERROR ("max86171_irq_read_clear ERROR: LED8_COMPB = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS3_LED7_COMPB, s3) != 0)
    {
        PRINT_MAX86171_ERROR ("max86171_irq_read_clear ERROR: LED7_COMPB = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS3_LED6_COMPB, s3) != 0)
    {
        PRINT_MAX86171_ERROR ("max86171_irq_read_clear ERROR: LED6_COMPB = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS3_LED5_COMPB, s3) != 0)
    {
        PRINT_MAX86171_ERROR ("max86171_irq_read_clear ERROR: LED5_COMPB = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS3_LED4_COMPB, s3) != 0)
    {
        PRINT_MAX86171_ERROR ("max86171_irq_read_clear ERROR: LED4_COMPB = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS3_LED3_COMPB, s3) != 0)
    {
        PRINT_MAX86171_ERROR ("max86171_irq_read_clear ERROR: LED3_COMPB = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS3_LED2_COMPB, s3) != 0)
    {
        PRINT_MAX86171_ERROR ("max86171_irq_read_clear ERROR: LED2_COMPB = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS3_LED1_COMPB, s3) != 0)
    {
        PRINT_MAX86171_ERROR ("max86171_irq_read_clear ERROR: LED1_COMPB = 1\n\r");
    }

    if (MAX86171_F_GET(MAX86171_STATUS1_PWR_RDY, s1) == 0)
    {
        PRINT_MAX86171_ERROR ("max86171_irq_read_clear ERROR: PWR_RDY = 0\n\r");
        return MAX86171_E_STATE;
    }
    return MAX86171_OK;
}

/**
 * @brief Shutdown the device
 *
 * @param dev Pointer to MAX86171 device handle
 * @return MAX86171_OK on success
 */
int max86171_shutdown(max86171_t *dev) {
    if (!dev) return MAX86171_E_NULL;
    int rc;

    // Read current System Configuration 1
    uint8_t sys_cfg1 = 0;
    rc = max86171_read_reg(dev, MAX86171_REG_SYS_CFG1, &sys_cfg1, 1);
    if (rc != MAX86171_OK) {
        return rc;
    }

    // Set shutdown and power-down bits
    // Bit 1: SHDN (0 = active, 1 = shutdown)
    // Bit 2: PPG1_PWRDN (0 = active, 1 = powered down)
    // Bit 3: PPG2_PWRDN (0 = active, 1 = powered down)
    sys_cfg1 |= (0x0E);

    // Write back to device
    rc = max86171_write_reg(dev, MAX86171_REG_SYS_CFG1, &sys_cfg1, 1);
    if (rc != MAX86171_OK) {
        return rc;
    }

    return MAX86171_OK;
}

/**
 * @brief Start the device running (after configuration already loaded)
 *
 * Use this if you've already called max86171_load_config_from_table()
 * and just need to exit shutdown mode.
 *
 * @param dev Pointer to MAX86171 device handle
 * @return MAX86171_OK on success
 */
int max86171_start_running(max86171_t *dev) {
    if (!dev) return MAX86171_E_NULL;
    int rc;

    // Read current System Configuration 1
    uint8_t sys_cfg1 = 0;
    rc = max86171_read_reg(dev, MAX86171_REG_SYS_CFG1, &sys_cfg1, 1);
    if (rc != MAX86171_OK) {
        return rc;
    }

    // Clear shutdown and power-down bits
    // Bit 1: SHDN (0 = active, 1 = shutdown)
    // Bit 2: PPG1_PWRDN (0 = active, 1 = powered down)
    // Bit 3: PPG2_PWRDN (0 = active, 1 = powered down)
    sys_cfg1 &= ~(0x0E);

    // Write back to device
    rc = max86171_write_reg(dev, MAX86171_REG_SYS_CFG1, &sys_cfg1, 1);
    if (rc != MAX86171_OK) {
        return rc;
    }

    // Wait for a while for device to be ready
    int count = 100;
    uint8_t status1 = 0;
    while (count > 0)
    {
        // Wait a little
        HAL_Delay(10);
        // Verify power ready status bit
        rc = max86171_read_reg(dev, MAX86171_REG_STATUS1, &status1, 1);
        if (rc != MAX86171_OK) {
            return rc;
        }
        // Power ready bit is set
        if (status1 & 0x01)
            break;
        count--;
    }

    if (!(status1 & 0x01)) {
        return MAX86171_E_TIMEOUT;  // PWR_RDY not set
    }

    return MAX86171_OK;
}

/**
 * @brief Load configuration from table into MAX86171 device
 *
 * Writes the base register config followed by only the MEAS blocks
 * selected by meas_mask.
 *
 * @param dev       Pointer to MAX86171 device handle
 * @param meas_mask Bitmask of MEAS channels to configure, e.g.:
 *                    MAX86171_MEAS1_EN | MAX86171_MEAS3_EN
 *                  Use MAX86171_MEAS_ALL to enable all nine channels.
 * @return MAX86171_OK on success, error code on failure
 */
int max86171_load_config_from_table(max86171_t *dev, uint16_t meas_mask) {
    if (!dev) return MAX86171_E_NULL;

    int rc;

    /* --- Write base config --- */
    for (size_t i = 0; i < MAX86171_BASE_CONFIG_SIZE; i++) {
        uint8_t value = g_max86171_base_config[i].value;
        rc = max86171_write_reg(dev, g_max86171_base_config[i].address, &value, 1);
        if (rc != MAX86171_OK) {
            return rc;
        }
        HAL_Delay(1);
    }

    /* --- Write selected MEAS blocks --- */
    for (int m = 0; m < MAX86171_NUM_MEAS; m++) {
        if (!(meas_mask & (1U << m))) {
            continue;
        }

        for (int r = 0; r < MAX86171_MEAS_REGS; r++) {
            uint8_t value = g_max86171_meas_config[m][r].value;
            rc = max86171_write_reg(dev, g_max86171_meas_config[m][r].address, &value, 1);
            if (rc != MAX86171_OK) {
                return rc;
            }
            HAL_Delay(1);
        }
    }

    /* --- Write SYS_CFG2: MEAS1-8 enable bits directly from mask --- */
    uint8_t sys_cfg2 = (uint8_t)(meas_mask & 0xFFU);
    rc = max86171_write_reg(dev, MAX86171_REG_SYS_CFG2, &sys_cfg2, 1);
    if (rc != MAX86171_OK) {
        return rc;
    }
    HAL_Delay(1);

    /* --- Write SYS_CFG1: MEAS9 enable + exit shutdown to start acquisition --- */
    uint8_t sys_cfg1 =
        MAX86171_F_MAKE(MAX86171_SYS_RESET,         0) |
        MAX86171_F_MAKE(MAX86171_SYS_SHDN,          0) |  /* exit shutdown = start */
        MAX86171_F_MAKE(MAX86171_SYS_PPG1_PWRDN,    0) |
        MAX86171_F_MAKE(MAX86171_SYS_PPG2_PWRDN,    0) |
        MAX86171_F_MAKE(MAX86171_SYS_SYNC_MODE,     0) |
        MAX86171_F_MAKE(MAX86171_SYS_SW_FORCE_SYNC, 0) |
        MAX86171_F_MAKE(MAX86171_SYS_MEAS9_EN,      (meas_mask & MAX86171_MEAS_EN_MEAS9) ? 1U : 0U);

    rc = max86171_write_reg(dev, MAX86171_REG_SYS_CFG1, &sys_cfg1, 1);
    if (rc != MAX86171_OK) {
        return rc;
    }
    HAL_Delay(1);

    return MAX86171_OK;
}

/**
 * @brief Verify loaded configuration by reading back registers
 *
 * Reads back all R/W registers from the base config and the selected
 * MEAS blocks, comparing them with expected values. Useful for debugging.
 *
 * @param dev             Pointer to MAX86171 device handle
 * @param meas_mask       Bitmask of MEAS channels to verify (same mask used in load)
 * @param mismatches      Array to store addresses of mismatched registers (optional)
 * @param max_mismatches  Maximum number of mismatches to store
 * @param mismatch_count  Pointer to store actual number of mismatches found
 * @return MAX86171_OK if all registers match, MAX86171_E_STATE if mismatches found
 */
int max86171_verify_config(max86171_t *dev,
                           uint16_t meas_mask,
                           uint8_t *mismatches,
                           size_t max_mismatches,
                           size_t *mismatch_count) {
    if (!dev) {
        return MAX86171_E_NULL;
    }

    int rc;
    size_t mismatch_cnt = 0;

    /* Helper lambda-style macro to avoid duplicating the read/compare logic */
#define VERIFY_REG(cfg_addr, cfg_val)                                       \
    do {                                                                     \
        uint8_t actual = 0;                                                  \
        rc = max86171_read_reg(dev, (cfg_addr), &actual, 1);                \
        if (rc != MAX86171_OK) { return rc; }                               \
        if (actual != (cfg_val)) {                                           \
            PRINT_MAX86171_ERROR ("max86171_verify_config mismatch at 0x%02x: 0x%02x, 0x%02x\n\r", cfg_addr, actual, cfg_val); \
            mismatch_cnt++;                                                  \
            if (mismatches && mismatch_cnt <= max_mismatches) {             \
                mismatches[mismatch_cnt - 1] = (cfg_addr);                  \
            }                                                                \
        }                                                                    \
    } while (0)

    /* --- Verify base config --- */
    for (size_t i = 0; i < MAX86171_BASE_CONFIG_SIZE; i++) {
        VERIFY_REG(g_max86171_base_config[i].address,
                   g_max86171_base_config[i].value);
    }

    /* --- Verify selected MEAS blocks --- */
    for (int m = 0; m < MAX86171_NUM_MEAS; m++) {
        if (!(meas_mask & (1U << m))) {
            continue;
        }
        for (int r = 0; r < MAX86171_MEAS_REGS; r++) {
            VERIFY_REG(g_max86171_meas_config[m][r].address,
                       g_max86171_meas_config[m][r].value);
        }
    }

#undef VERIFY_REG

    if (mismatch_count) {
        *mismatch_count = mismatch_cnt;
    }
    PRINT_MAX86171_INFO ("max86171_verify_config mismatch count = %d\n\r", mismatch_cnt);

    return (mismatch_cnt == 0) ? MAX86171_OK : MAX86171_E_STATE;
}

#ifdef UNUSED

/**
 * @brief Set LED driver A current for a given measurement (1-9)
 * @param dev      Device handle
 * @param meas_idx Measurement index 1-9
 * @param pa       Raw PA register value (0x00-0xFF → 0–51 mA in 0.2mA steps)
 */
int max86171_set_led_current(max86171_t *dev, uint8_t meas_idx, uint8_t pa) {
    if (!dev) return MAX86171_E_NULL;
    if (meas_idx < 1 || meas_idx > 9) return MAX86171_E_ARG;

    /* DrvA PA is at base + 4 (same layout as configure_measurement) */
    uint8_t reg = meas_selects_addr(meas_idx) + 4;
    return max86171_write_reg(dev, reg, &pa, 1);
}

/**
 * @brief Set number of averages for a given measurement (1-9)
 * @param dev      Device handle
 * @param meas_idx Measurement index 1-9
 * @param avg      Averages code: 0=1,1=2,2=4,3=8,4=16,5=32,6=64,7=128
 */
int max86171_set_avg(max86171_t *dev, uint8_t meas_idx, uint8_t avg) {
    if (!dev) return MAX86171_E_NULL;
    if (meas_idx < 1 || meas_idx > 9) return MAX86171_E_ARG;

    uint8_t reg = meas_selects_addr(meas_idx) + 1;

    uint8_t v = 0;
    int rc = max86171_read_reg(dev, reg, &v, 1);
    if (rc != MAX86171_OK) return rc;

    /* Clear AVER field then set new value */
    MAX86171_F_SET(v, MAX86171_MEAS_AVER, avg);

    return max86171_write_reg(dev, reg, &v, 1);
}

#endif

/**
 * @brief Get total number of register entries across base config and all MEAS blocks
 */
size_t max86171_get_config_size(void) {
    return MAX86171_BASE_CONFIG_SIZE + (MAX86171_NUM_MEAS * MAX86171_MEAS_REGS);
}

/**
 * @brief Get register configuration entry by flat index
 *
 * Indices 0..(BASE_CONFIG_SIZE-1) map to base config.
 * Indices BASE_CONFIG_SIZE.. map sequentially through MEAS1..MEAS9 blocks.
 *
 * @param index Flat index into the combined config table
 * @param addr  Pointer to store register address
 * @param value Pointer to store register value
 * @return MAX86171_OK on success, MAX86171_E_ARG if index out of bounds
 */
int max86171_get_config_entry(size_t index, uint8_t *addr, uint8_t *value) {
    if (index >= max86171_get_config_size()) {
        return MAX86171_E_ARG;
    }

    const max86171_reg_config_t *entry;

    if (index < MAX86171_BASE_CONFIG_SIZE) {
        entry = &g_max86171_base_config[index];
    } else {
        size_t meas_index = index - MAX86171_BASE_CONFIG_SIZE;
        int m = (int)(meas_index / MAX86171_MEAS_REGS);
        int r = (int)(meas_index % MAX86171_MEAS_REGS);
        entry = &g_max86171_meas_config[m][r];
    }

    if (addr)  *addr  = entry->address;
    if (value) *value = entry->value;

    return MAX86171_OK;
}

/* End of file */
