#include "em_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sl_spidrv_instances.h"
#include "pin_config.h"

// local includes
#include "accel.h"

#define PRINT_ACCEL_ERROR printf

#define TXRXDATABUFLEN (2)
#define FIFODATABUFLEN (7)

// Maximum SPI clock speed of 10MHz
static int doSPI(int rw, int len, uint8_t *txdata, uint8_t *rxdata)
{
    GPIO_PinOutClear(CS_ACCEL_PORT, CS_ACCEL_PIN);
    // Add in the read/write bit in the MSB of the register address
    txdata[0] |= rw;
    Ecode_t retVal = SPIDRV_MTransferB (sl_spidrv_inst0_handle, txdata, rxdata, len);
    if (retVal) { PRINT_ACCEL_ERROR ("prv_write retVal = %ld\n\r", retVal); }
    GPIO_PinOutSet(CS_ACCEL_PORT, CS_ACCEL_PIN);
    return 0;
}

static uint8_t accelRegRead(uint8_t reg)
{
    uint8_t packet[TXRXDATABUFLEN];
    uint8_t output[TXRXDATABUFLEN];

    // prepare operation
    packet[0] = (reg);
    // This is dummy for read
    packet[1] = 0;
    // perform read
    doSPI(ACCEL_REG_READBIT, sizeof(packet), packet, output);
    // return result
    return output[1];
}

static void accelRegReadContinuous(uint8_t reg, uint8_t *output)
{
    uint8_t packet[FIFODATABUFLEN];
    uint8_t rxbuf[FIFODATABUFLEN];

    // prepare operation
    packet[0] = (reg);
    // Dummy bytes
    for (int i = 1; i < FIFODATABUFLEN; i++)
        packet[i] = 0;
    // perform read
    doSPI(ACCEL_REG_READBIT | ACCEL_REG_AUTOINC, sizeof(packet), packet, rxbuf);
    // rxbuf[0] is garbage (clocked out during the address byte), copy the rest
    memcpy(output, &rxbuf[1], FIFODATABUFLEN - 1);
    return;
}

static void accelRegWrite(uint8_t reg, uint8_t value)
{
    uint8_t packet[TXRXDATABUFLEN];
    uint8_t output[TXRXDATABUFLEN];

    // prepare operation
    packet[0] = (reg);
    packet[1] = value;
    // perform write
    doSPI(0, sizeof(packet), packet, output);
}

// functions
int accelInit(void)
{
    uint8_t id = 0x00;

    // check ID
    id = accelRegRead(ACCEL_REG_WHOAMI);
    printf ("Accel WHOAMI 0x%02x, expected 0x%02x\n\r", id, ACCEL_ID);

    if(id == ACCEL_ID)
    {
        // Disable ADC and temperature sensor
        accelRegWrite(ACCEL_REG_TEMP_CFG_REG, 0x00);
        // Disable pull-up on SDO/SA0 to reduce current consumption
        accelRegWrite(ACCEL_REG_CTRL_REG0, 0x90);
        // Set sampling frequency and enable axes
        accelPowerOn();
        // Normal mode, HP filter enabled cutoff = ~2Hz
        // HPF bypassed for output registers and FIFO
        // HPF disabled for Click function & AOI function on interrupt 2
        // HPF enabled for AOI function on interrupt 1
        accelRegWrite(ACCEL_REG_CTRL_REG2, 0x01);
        // Intertial interrupt IA1 on INT1
        accelRegWrite(ACCEL_REG_CTRL_REG3, 0x40);
        // No block data, little endian byte reads, +/-2g,Low Res(HR=0), no self test, SPI 4-wire
        accelRegWrite(ACCEL_REG_CTRL_REG4, 0x00);
        // Fifo enabled, INT1 & INT2 src not latched
        accelRegWrite(ACCEL_REG_CTRL_REG5, 0x40);
        // Int active low - nothing on Int 2
        accelRegWrite(ACCEL_REG_CTRL_REG6, 0x02);
        // Set to 0 for now
        accelRegWrite(ACCEL_REG_REFERENCE, 0x00);
        // Fifo stream mode, trigger to int2, watermark threshold 8 (N+1)
        accelRegWrite(ACCEL_REG_FIFO_CTRL_REG, 0xA7);
        // X-Y-Z Movement detection (one OR more)
        accelRegWrite(ACCEL_REG_INT1_CFG, 0x2A);
        // Int 1 threshold 0-127 0x08 = 8*16=128mG
        accelRegWrite(ACCEL_REG_INT1_THS, 0x08);
        // Int 1 duration (using time scale in CTRL_REG1 0-127)
        accelRegWrite(ACCEL_REG_INT1_DURATION, 0x00);
        // Int 2 all disabled
        accelRegWrite(ACCEL_REG_INT2_CFG, 0x00);
        // Int 2 threshold 0-127 0x20 = 32*16=512mG
        accelRegWrite(ACCEL_REG_INT2_THS, 0x20);
        // Int 2 duration (using time scale in CTRL_REG1 0-127)
        accelRegWrite(ACCEL_REG_INT2_DURATION, 0x00);
        // Disable click
        accelRegWrite(ACCEL_REG_CLICK_CFG, 0x00);
        // Normal mode, HP filter enabled cutoff = ~2Hz
        // THIS READ IS NOT A BUG
        accelRegRead(ACCEL_REG_HP_FILTER_RESET);
        return 0;
    }

    return 1;
}

// Get how many accelerometer X/Y/Z data are available in FIFO
int accelGetNumData(void)
{
    int num;
    // Find out how many samples are available to read (FSS)
    num = accelRegRead(ACCEL_REG_FIFO_SRC_REG) & ((1L << 5) - 1);
    return num;
}

// Get accelerometer X/Y/Z data
void accelGetData(uint8_t *output)
{
    // Read one sample (6 bytes per sample)
    accelRegReadContinuous(ACCEL_REG_OUT_X_L, output);
}

// Get source of Int1
uint8_t accelGetSrc1(void)
{
    // This will clear the latched interrupt sources
    uint8_t val = accelRegRead(ACCEL_REG_INT1_SOURCE);
    return val;
}

// Get source of Int2
uint8_t accelGetSrc2(void)
{
    // This will clear the latched interrupt sources
    uint8_t val = accelRegRead(ACCEL_REG_INT2_SOURCE);
    return val;
}

// Put into power-on mode
void accelPowerOn(void)
{
#ifdef USE_25Hz
    // 25Hz all axes enabled, normal mode (Lpen=0) 10 bits per sample
    accelRegWrite(ACCEL_REG_CTRL_REG1, 0x37);
#elif defined(USE_10Hz)
    // 10Hz all axes enabled, normal mode (Lpen=0) 10 bits per sample
    accelRegWrite(ACCEL_REG_CTRL_REG1, 0x27);
#elif defined(USE_1Hz)
    // 1Hz all axes enabled, normal mode (Lpen=0) 10 bits per sample
    accelRegWrite(ACCEL_REG_CTRL_REG1, 0x17);
#else
#error Accel frequency not chosen
#endif
}

// Put into power-off mode
void accelPowerOff(void)
{
    // Set ODR bits to 0000
    accelRegWrite(ACCEL_REG_CTRL_REG1, 0x00);
}

// Clear FIFO
void accelClearFifo(void)
{
    // FIFO bypass mode
    accelRegWrite(ACCEL_REG_FIFO_CTRL_REG, 0x00);
    // FIFO stream mode, trigger to int2, watermark threshold 8
    accelRegWrite(ACCEL_REG_FIFO_CTRL_REG, 0xA7);
}

/* End of file */
