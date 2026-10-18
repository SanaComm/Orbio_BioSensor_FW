#include "app.h"
#include "sl_spidrv_instances.h"
#include "em_gpio.h"
#include "pin_config.h"

#define PRINT_CMX994_ERROR printf

// TBD: Look at tCDS, tCDH
static void sendCMX994 (uint8_t *cmd, int cmdlen)
{
    GPIO_PinOutClear(CS_CMX994_PORT, CS_CMX994_PIN);
    /* MSB first */
    /* 5MHz maximum clock speed as tCK=200ns */
    /* At least tCSE=100ns from CSN enable to clock */
    for (int i = 0; i < cmdlen; i++)
    {
        /* At least tNXT=200ns from command byte to data byte, so send one-by-one */
        Ecode_t retVal = SPIDRV_MTransmitB (sl_spidrv_inst0_handle, &cmd[i], 1);
        (void)retVal;
        if (retVal) { PRINT_CMX994_ERROR ("sendCMX994 retVal = %ld\n\r", retVal); }
    }
    /* At least tCSH=100ns from clock to CSN disable */
    /* Times are 100ns, so let normal code execution at tens of MHz take care of these minimum delays */
    GPIO_PinOutSet(CS_CMX994_PORT, CS_CMX994_PIN);
}

void resetCMX994 (void)
{
    uint8_t cmd = 0x10;
    sendCMX994 (&cmd, sizeof (cmd));
}

void enableCMX994 (void)
{
    uint8_t cmd[2];
    // Enable BIAS (b7), set highest frequency range (b6,b5=3)
    // Disable low power mode (b4), VCO (b3), PLL (b2)
    // Enable RX (b1), TX (b0)
    cmd[0] = 0x11;
    // (1 << 7) | (3 << 5) | (0 << 4) | (0 << 3) | (0 << 2) | (1 << 1) | (1 << 0)
    cmd[1] = 0xE3;
    sendCMX994 (cmd, sizeof (cmd));

    // tCSOFF=1usec from one command to the next i.e. between CS high to CS low

    // To set 50Ohm LNA output impedance, rest of gain settings at defaults
    cmd[0] = 0x16;
    // (0 << 6) | (0 << 4) | (1 << 3) | (0 << 0)
    cmd[1] = 0x08;
    sendCMX994 (cmd, sizeof (cmd));

    // tCSOFF=1usec from one command to the next i.e. between CS high to CS low

    // To enable LO Input, TXLO=LO/2 and disable VCO related blocks.
    cmd[0] = 0x25;
    // (0 << 7) | (0 << 5) | (1 << 4) | (0 << 3) | (0 << 2) | (0 << 1) | (0 << 0)
    cmd[1] = 0x10;
    sendCMX994 (cmd, sizeof (cmd));
}

void setCMX994Offset (int ioffset, int qoffset)
{
    uint8_t cmd[3];
    // Validate input offsets
    if (ioffset > CMX994_MAX_OFFSET) return;
    if (ioffset < -CMX994_MAX_OFFSET) return;
    if (qoffset > CMX994_MAX_OFFSET) return;
    if (qoffset < -CMX994_MAX_OFFSET) return;
    // To set the I/Q offsets
    cmd[0] = 0x17;
    // Find index in steps of 25mV
    cmd[1] = abs (qoffset) / 25;
    cmd[2] = abs (ioffset) / 25;
    // Add sign
    if (qoffset < 0) cmd[1] |= (1 << 5);
    if (ioffset < 0) cmd[2] |= (1 << 5);
    sendCMX994 (cmd, sizeof (cmd));
}

/* End of file */
