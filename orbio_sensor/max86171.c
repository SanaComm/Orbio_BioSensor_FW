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
#define PRINT_MAX86171_IGNORE(...)

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

int max86171_status_read_clear(max86171_t *dev) {
    if (!dev) return MAX86171_E_NULL;

    uint8_t s1 = 0, s2 = 0, s3 = 0;

    // Read status registers (reading clears them)
    int rc = max86171_read_reg(dev, MAX86171_REG_STATUS1, &s1, 1);
    if (rc != MAX86171_OK) return rc;

    rc = max86171_read_reg(dev, MAX86171_REG_STATUS2, &s2, 1);
    if (rc != MAX86171_OK) return rc;

    rc = max86171_read_reg(dev, MAX86171_REG_STATUS3, &s3, 1);
    if (rc != MAX86171_OK) return rc;

    PRINT_MAX86171_IGNORE ("max86171_status_read_clear: %02x, %02x, %02x\n\r", s1, s2, s3);

    if (MAX86171_F_GET(MAX86171_STATUS1_A_FULL, s1) != 0)
    {
        PRINT_MAX86171_IGNORE ("max86171_status_read_clear: A_FULL = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS1_FRAME_RDY, s1) != 0)
    {
        PRINT_MAX86171_IGNORE ("max86171_status_read_clear: FRAME_RDY = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS1_FIFO_DATA_RDY, s1) != 0)
    {
        PRINT_MAX86171_IGNORE ("max86171_status_read_clear: FIFO_DATA_RDY = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS1_ALC_OVF, s1) != 0)
    {
        PRINT_MAX86171_INFO ("max86171_status_read_clear: ALC_OVF = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS1_EXP_OVF, s1) != 0)
    {
        PRINT_MAX86171_INFO ("max86171_status_read_clear: EXP_OVF = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS2_VDD_OOR, s2) != 0)
    {
        PRINT_MAX86171_INFO ("max86171_status_read_clear ERROR: VDD_OOR = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS2_INVALID_CFG, s2) != 0)
    {
        PRINT_MAX86171_INFO ("max86171_status_read_clear ERROR: INVALID_CFG = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS2_LED9_COMPB, s2) != 0)
    {
        PRINT_MAX86171_INFO ("max86171_status_read_clear ERROR: LED9_COMPB = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS3_LED8_COMPB, s3) != 0)
    {
        PRINT_MAX86171_INFO ("max86171_status_read_clear ERROR: LED8_COMPB = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS3_LED7_COMPB, s3) != 0)
    {
        PRINT_MAX86171_INFO ("max86171_status_read_clear ERROR: LED7_COMPB = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS3_LED6_COMPB, s3) != 0)
    {
        PRINT_MAX86171_INFO ("max86171_status_read_clear ERROR: LED6_COMPB = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS3_LED5_COMPB, s3) != 0)
    {
        PRINT_MAX86171_INFO ("max86171_status_read_clear ERROR: LED5_COMPB = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS3_LED4_COMPB, s3) != 0)
    {
        PRINT_MAX86171_INFO ("max86171_status_read_clear ERROR: LED4_COMPB = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS3_LED3_COMPB, s3) != 0)
    {
        PRINT_MAX86171_INFO ("max86171_status_read_clear ERROR: LED3_COMPB = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS3_LED2_COMPB, s3) != 0)
    {
        PRINT_MAX86171_INFO ("max86171_status_read_clear ERROR: LED2_COMPB = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS3_LED1_COMPB, s3) != 0)
    {
        PRINT_MAX86171_INFO ("max86171_status_read_clear ERROR: LED1_COMPB = 1\n\r");
    }
    if (MAX86171_F_GET(MAX86171_STATUS1_PWR_RDY, s1) != 0)
    {
        PRINT_MAX86171_ERROR ("max86171_status_read_clear ERROR: PWR_RDY = 1\n\r");
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

int max86171_set_config_current(size_t index, uint8_t value) {
    if (index > (MAX86171_NUM_MEAS - 1)) {
        return MAX86171_E_ARG;
    }
   
    max86171_reg_config_t *entry = &g_max86171_meas_config[index][MAX86171_MEAS_REGS - 3];
    // Iterate over DRVA, DRVB, DRVC
    for (int i = 0; i < 3; i++)
    {
        // Set new current only if no-zero already
        if (entry->value) entry->value = value;
        entry++;
    }
    return MAX86171_OK;
}

/* End of file */
