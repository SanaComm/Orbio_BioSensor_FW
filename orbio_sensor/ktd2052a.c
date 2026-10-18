#include "ktd2052a.h"

#define LED_I2C_DEVICE                I2C0
#define LED_OK                        0x00  /**< No errors                   */
#define LED_ERROR_I2C_TRANSFER_NACK   0x01  /**< No acknowledgement received */
#define LED_ERROR_I2C_TRANSFER_FAILED 0x02  /**< I2C transaction failed      */
#define LED_ERROR_TIMEOUT             0x04  /**< Timeout occurred            */

#define LED_I2C_ADDR 0xE8 // Including left shift

#define RED_CURRENT (3000) // In uA
#define GREEN_CURRENT (2000) // In uA
#define BLUE_CURRENT (3000) // In uA
#define CURRENT_LEAST_COUNT (125) // In uA
#define RED_CURRENT_SETTING (RED_CURRENT / CURRENT_LEAST_COUNT)
#define GREEN_CURRENT_SETTING (GREEN_CURRENT / CURRENT_LEAST_COUNT)
#define BLUE_CURRENT_SETTING (BLUE_CURRENT / CURRENT_LEAST_COUNT)

#define PRINT_ERROR printf

enum
{
    REG_ID = 0,
    REG_MONITOR,
    REG_CONTROL,
    REG_IRED1,
    REG_IGRN1,
    REG_IBLU1,
    REG_IRED2,
    REG_IGRN2,
    REG_IBLU2,
    REG_IRED3,
    REG_IGRN3,
    REG_IBLU3,
    REG_IRED4,
    REG_IGRN4,
    REG_IBLU4,
    REG_PG_CNTL,
    REG_PG_FADE,
    REG_PG_RGB1,
    REG_PG_RGB2,
    REG_PG_RGB3,
    REG_PG_RGB4,
    REG_PG_WD
};

static uint8_t writeI2C (uint8_t addr, uint8_t data)
{
  I2C_TransferSeq_TypeDef seq;
  I2C_TransferReturn_TypeDef ret;
  uint8_t cmd[2];

  seq.addr = LED_I2C_ADDR;
  seq.buf[0].data = cmd;
  seq.buf[0].len = sizeof (cmd);
  seq.flags = I2C_FLAG_WRITE;
  cmd[0] = addr;
  cmd[1] = data;

  ret = I2CSPM_Transfer(LED_I2C_DEVICE, &seq);

  if ( ret == i2cTransferNack ) {
    PRINT_ERROR ("LED I2C write nack\n\r");
    return LED_ERROR_I2C_TRANSFER_NACK;
  } else if ( ret != i2cTransferDone ) {
    PRINT_ERROR ("LED I2C write fail\n\r");
    return LED_ERROR_I2C_TRANSFER_FAILED;
  }

  return LED_OK;
}

static uint8_t readI2C (uint8_t addr, uint8_t *data)
{
  I2C_TransferSeq_TypeDef seq;
  I2C_TransferReturn_TypeDef ret;
  uint8_t cmd[1];

  seq.addr = LED_I2C_ADDR;
  seq.buf[0].data = cmd;
  seq.buf[0].len = sizeof (cmd);
  seq.buf[1].data = data;
  seq.buf[1].len = 1;
  seq.flags = I2C_FLAG_WRITE_READ;
  cmd[0] = addr;

  ret = I2CSPM_Transfer(LED_I2C_DEVICE, &seq);

  if ( ret == i2cTransferNack ) {
    PRINT_ERROR ("LED I2C read nack\n\r");
    return LED_ERROR_I2C_TRANSFER_NACK;
  } else if ( ret != i2cTransferDone ) {
    PRINT_ERROR ("LED I2C read fail\n\r");
    return LED_ERROR_I2C_TRANSFER_FAILED;
  }

  return LED_OK;
}

void setLed (int rgbsetno, int colourmask)
{
    // Check for valid RGB LEDs
    if (rgbsetno != LED_1) return;
    // Check for valid colour mask which for now is a simple on/off for each of the LEDs in this set
    if (colourmask > 7) return;
    // Set Red LED of this RGB set
    int current = 0;
    if (colourmask & RED_MASK) current = RED_CURRENT_SETTING;
    writeI2C (rgbsetno * 3 + REG_IRED1, current);
    // Set Green LED of this RGB set
    current = 0;
    if (colourmask & GREEN_MASK) current = GREEN_CURRENT_SETTING;
    writeI2C (rgbsetno * 3 + REG_IGRN1, current);
    // Set Blue LED of this RGB set
    current = 0;
    if (colourmask & BLUE_MASK) current = BLUE_CURRENT_SETTING;
    writeI2C (rgbsetno * 3 + REG_IBLU1, current);
}

void setLedShutdown (void)
{
    writeI2C (REG_CONTROL, (0 << 6));
    return;
}

void setLedNormal (void)
{
    writeI2C (REG_CONTROL, (2 << 6));
    return;
}

void setLedFade (uint8_t fade)
{
    uint8_t val = 0;
    readI2C (REG_CONTROL, &val);
    // Extract mode (shutdown/normal)
    val = val & 0xC0;
    // Insert fade
    val = val | (fade & 0x3F);
    writeI2C (REG_CONTROL, val);
}

uint8_t getLedMonitor (void)
{
    uint8_t val = 0;
    readI2C (REG_MONITOR, &val);
    return val;
}

uint8_t getLedID (void)
{
    uint8_t val = 0;
    readI2C (REG_ID, &val);
    return val;
}
