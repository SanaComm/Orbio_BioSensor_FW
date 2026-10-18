#ifndef I2CLED_H
#define I2CLED_H

#include "sl_i2cspm.h"
#include <stdio.h>
#include <stdint.h>

#define REG_ID_VAL ((0x5 << 5) | 0x06) // Kinetic, KTD2052A

enum
{
    LED_1 = 0, // only one RGB LED in hardware
};

enum
{
    RED_MASK = (1 << 2),
    GREEN_MASK = (1 << 1),
    BLUE_MASK = (1 << 0)
};

enum
{
    COLOUR_WHITE = RED_MASK | GREEN_MASK | BLUE_MASK,
    COLOUR_BLUE = 0 | 0 | BLUE_MASK,
    COLOUR_CYAN = 0 | GREEN_MASK | BLUE_MASK,
    COLOUR_RED = RED_MASK | 0 | 0,
    COLOUR_GREEN = 0 | GREEN_MASK | 0,
    COLOUR_YELLOW = RED_MASK | GREEN_MASK | 0,
    COLOUR_MAGENTA = RED_MASK | 0 | BLUE_MASK,
    COLOUR_NONE = 0
};

void setLed (int rgbsetno, int colourmask);
void setLedShutdown (void);
void setLedNormal (void);
void setLedFade (uint8_t fade);
uint8_t getLedMonitor (void);
uint8_t getLedID (void);

#endif
