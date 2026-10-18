#include "app.h"
#include "sl_spidrv_instances.h"
#include "em_gpio.h"
#include "em_usart.h"
#include "pin_config.h"
#include <stdlib.h>
#include <math.h>

#define FOSC_MHZ (39)

#define PRINT_LMX2572LP_REG(...)
#define PRINT_LMX2572LP_INFO(...)
#define PRINT_LMX2572LP_ERROR printf

#undef CALCULATE_VCO_PARAMS
#undef USE_TEST_CODE

// Three options for SPI transfers
#undef SPIDRV_3BYTES
#undef SPIDRV_1BYTE
#define USART_1BYTE

// LMX2572LP SPI can run at upto 75MHz
static void writeLMX2572LP (uint32_t fulldata)
{
    PRINT_LMX2572LP_REG ("writeLMX2572LP %04lx\n\r", fulldata);
    uint8_t bytedata[3];
    bytedata[0] = (fulldata >> 16) & 0xFF;
    bytedata[1] = (fulldata >> 8) & 0xFF;
    bytedata[2] = (fulldata >> 0) & 0xFF;
    GPIO_PinOutClear(CS_LO_PORT, CS_LO_PIN);
#if defined(SPIDRV_3BYTES)
    Ecode_t retVal = SPIDRV_MTransmitB (sl_spidrv_inst0_handle, bytedata, 3);
    if (retVal) { PRINT_LMX2572LP_ERROR ("writeLMX2572LP retVal = %ld\n\r", retVal); }
#elif defined(SPIDRV_1BYTE)
    Ecode_t retVal = SPIDRV_MTransmitB (sl_spidrv_inst0_handle, &bytedata[0], 1);
    if (retVal) { PRINT_LMX2572LP_ERROR ("writeLMX2572LP retVal1 = %ld\n\r", retVal); }
    retVal = SPIDRV_MTransmitB (sl_spidrv_inst0_handle, &bytedata[1], 1);
    if (retVal) { PRINT_LMX2572LP_ERROR ("writeLMX2572LP retVal2 = %ld\n\r", retVal); }
    retVal = SPIDRV_MTransmitB (sl_spidrv_inst0_handle, &bytedata[2], 1);
    if (retVal) { PRINT_LMX2572LP_ERROR ("writeLMX2572LP retVal3 = %ld\n\r", retVal); }
#else
    USART_Tx (USART1, bytedata[0]);
    USART_Tx (USART1, bytedata[1]);
    USART_SpiTransfer (USART1, bytedata[2]);
#endif
    GPIO_PinOutSet(CS_LO_PORT, CS_LO_PIN);
}

#ifdef CALCULATE_VCO_PARAMS

enum
{
    VCO1 = 0,
    VCO2,
    VCO3,
    VCO4,
    VCO5,
    VCO6,
    VCO_MAX,
};

#define NUM_VCO_PARAMS (5)

// Parameters taken from LMX2572LP datasheet Table 135
static int vcoSelection[(VCO_MAX + 1) * NUM_VCO_PARAMS] = 
{
    // VCO CORE, fMin (MHz),  fMax (MHz), CMin, CMax, AMin, AMax
    3200, 131, 19, 138, 137,
    3650, 143, 25, 162, 142,
    4200, 135, 34, 126, 114,
    4650, 136, 25, 195, 172,
    5200, 133, 20, 190, 163,
    5750, 151, 27, 256, 204,
    6400, 0, 0, 0, 0
};

#endif

// Register values taken from datasheet & TICSPro
// Datasheet says to start with R0 RESET=1 and then start from R125 till R0. FCAL_EN will be 1 even when doing RESET=1 as FCAL_EN=0 is prohibited.
static const uint32_t lmx2572lpregistervalues [] = 
{
#if (FOSC_MHZ == 100)
0x00211E, // RAMP_EN = 0, VCO_PHASE_SYNC_EN = 0, ADD_HOLD = 0, OUT_MUTE = 0, FCAL_HPFD_ADJ = 2, FCAL_LPFD_ADJ = 0, FCAL_EN = 1, MUXOUT_LD_SEL = Lock Detect, RESET = 1, POWERDOWN = 0
          // RAMP_EN = 0, VCO_PHASE_SYNC_EN = 1, ADD_HOLD = 0, OUT_MUTE = 0, FCAL_HPFD_ADJ = 1, FCAL_LPFD_ADJ = 0, FCAL_EN = 1, MUXOUT_LD_SEL = Lock Detect, RESET = 1, POWERDOWN = 0
#elif (FOSC_MHZ == 39)
0x00209E, // RAMP_EN = 0, VCO_PHASE_SYNC_EN = 0, ADD_HOLD = 0, OUT_MUTE = 0, FCAL_HPFD_ADJ = 1, FCAL_LPFD_ADJ = 0, FCAL_EN = 1, MUXOUT_LD_SEL = Lock Detect, RESET = 1, POWERDOWN = 0
          // RAMP_EN = 0, VCO_PHASE_SYNC_EN = 1, ADD_HOLD = 0, OUT_MUTE = 0, FCAL_HPFD_ADJ = 1, FCAL_LPFD_ADJ = 0, FCAL_EN = 1, MUXOUT_LD_SEL = Lock Detect, RESET = 1, POWERDOWN = 0
#endif
// R125 Use reset value
// R124 Use reset value
// R123 Use reset value
// R122 Use reset value
// R121 Use reset value
// R120 Use reset value
// R119 Use reset value
// R118 Use reset value
// R117 Use reset value
// R116 Use reset value
// R115 Use reset value
// R114 Use reset value
// R113 Use reset value
// R112 Use reset value
// R111 Use reset value
// R110 Use reset value
// R109 Use reset value
// R108 Use reset value
// R107 Use reset value
// R106 Use reset value
// R105 Use reset value
// R104 Use reset value
// R103 Use reset value
// R102 Use reset value
// R101 Use reset value
// R100 Use reset value
// R99 Use reset value
// R98 Use reset value
// R97 Use reset value
// R96 Use reset value
// R95 Use reset value
// R94 Use reset value
// R93 Use reset value
// R92 Use reset value
// R91 Use reset value
// R90 Use reset value
// R89 Use reset value
// R88 Use reset value
// R87 Use reset value
// R86 Use reset value
// R85 Use reset value
// R84 Use reset value
// R83 Use reset value
// R82 Use reset value
// R81 Use reset value
// R80 Use reset value
// R79 Use reset value
0x4E0065, // QUICK_RECAL_EN = 0 (Assisted or unassisted cal), VCO_CAPCTRL_STRT can be set using Equation 5 for Assisted Cal but TI advised to do Full Cal.
// R77 Use reset value
// R76 Use reset value
0x4B0840, // CHDIV = 1 (Divide by 4)
// R74 Use reset value
// R73 Use reset value
// R72 Use reset value
// R71 Use reset value
0x4661A8, // R70 MASH_RST_COUNT (LOW) use value of 25000 to speedup PLL lock time when SYNC is ON (default = 50000)
// R69 MASH_RST_COUNT (HI) use reset value which is the advised value when SYNC is used as well
// R68 Use reset value
// R67 Use reset value
// R66 Use reset value
// R65 Use reset value
// R64 Use reset value
// R63 Use reset value
// R62 DBL_BUF_EN Use reset value
// R61 Use reset value
// R60 Use reset value. LD_DLY = 1000 (delay in fPD/4 cycles before VCOCal lock detect is asserted high)
// R59 Use reset value. LD_TYPE = 1 (Vtune and VCOCal)
0x3A0001, // R58 INPIN_IGNORE = 0 (Use SYNC, SysReqRef pins), INPIN_HYST = 0 (Disables High Hysterisis on these pins), INPIN_LVL = 0 (Default), INPIN_FMT = 0 (Default)
          // R58 INPIN_IGNORE = 1 (Ignore SYNC, SysReqRef pins), INPIN_HYST = 0 (Disables High Hysterisis on these pins), INPIN_LVL = 0 (Default), INPIN_FMT = 0 (Default)
// R57 Use reset value
// R56 Use reset value
// R55 Use reset value
// R54 Use reset value
// R53 Use reset value
// R52 Use reset value
// R51 Use reset value
// R50 Use reset value
// R49 Use reset value
// R48 Use reset value
// R47 Use reset value
// R46 Use reset value
// R45 Use reset value. OUTB_PWR = 8
0x2C08A3, // OUTA_PWR = 8, OUTB_PD = 1 (Power Down), OUTA_PD = 0 (Normal operation), MASH_RESET_N = 1 (Normal operation), MASH_ORDER = 3rd order.
0x2BBB80, // PLL_NUM lower (CALCULATE)
0x2A0000, // PLL_NUM upper (CALCULATE)
// R41 Use reset value. MASH_SEED = 0 (lower)
// R40 Use reset value. MASH_SEED = 0 (upper)
0x274240, // PLL_DEN lower set to 1000000
0x26000F, // PLL_DEN upper set to 1000000
// R37 Use reset value. MASH_SEED_EN = 0, PFD_DLY_SEL = 2 as per Table 3.
0x240028, // PLL_N in bottom 16 bits (CALCULATE)
// R35 Use reset value
0x220010, // PLL_N top 3 bits (CALCULATE)
// R33 Use reset value
// R32 Use reset value
// R31 Use reset value
// R30 Use reset value
// R29 Use reset value
// R28 Use reset value
// R27 Use reset value
// R26 Use reset value
// R25 Use reset value
// R24 Use reset value
// R23 Use reset value
// R22 Use reset value
// R21 Use reset value
// R20 Use reset value. User specified start VCO_SEL for calibration based on Tables 134/135, VCO_SEL_FORCE.
// R19 VCO_CAP_CTRL, needed if FORCE=1, so most likely we will not set it.
// R18 Use reset value
// R17 Use reset value. VCO_DACISET_STRT can be set using Equation 6 for Assisted Cal.
// R16 VCO_DACISET, needed for full-assisted VCO calibration, so most likely we will not set it.
// R15 Use reset value
0x0E1820, // CGP = Charge Pump Gain = 4 (2500uA) taken from TICS. Default is CGP = 8.
// R13 Use reset value
// R12 Use reset value. PLL_R_PRE = 1
#if (FOSC_MHZ == 100)
0x0BB028, // R11 PLL_R = 2 to compensate for extra divide by 2 in fVCO path
#else
// R11 Use reset value. PLL_R = 1
#endif
// R10 Use reset value. MULT = 1
// R9 Use reset value. MULT_HI = 0, OSC_2X = 0, to get fPD = fOSCin * OSC_2X * MULT_HI.
// R8 Use reset value. VCO_DACISET_FORCE = 0, VCO_CAPCTRL_FORCE = 0. Don't force values during calibration.
// R7 Use reset value. OUT_FORCE = Mute during VCO calibration depends on OUT_MUTE (0)
// R6 Use reset value. TI clarified that optimal LDO_DLY values in Table 141 are needed only if settling time after wakeup from powerdown is very crucial
// R5 Use reset value. IPBUF_TYPE = Single ended (1), IPBUF_TERM = Normal operation (0).
// R4 Use reset value
// R3 Use reset value
// R2 Use reset value
// R1 Use reset value. CAL_CLK_DIV=0 as fOSCin <= 200MHz
#if (FOSC_MHZ == 100)
0x00211C, // RAMP_EN = 0, VCO_PHASE_SYNC_EN = 0, ADD_HOLD = 0, OUT_MUTE = 0, FCAL_HPFD_ADJ = 2, FCAL_LPFD_ADJ = 0, FCAL_EN = 1, MUXOUT_LD_SEL = Lock Detect, RESET = 0, POWERDOWN = 0
          // RAMP_EN = 0, VCO_PHASE_SYNC_EN = 1, ADD_HOLD = 0, OUT_MUTE = 0, FCAL_HPFD_ADJ = 1, FCAL_LPFD_ADJ = 0, FCAL_EN = 1, MUXOUT_LD_SEL = Lock Detect, RESET = 0, POWERDOWN = 0
#elif (FOSC_MHZ == 39)
0x00209C, // RAMP_EN = 0, VCO_PHASE_SYNC_EN = 0, ADD_HOLD = 0, OUT_MUTE = 0, FCAL_HPFD_ADJ = 1, FCAL_LPFD_ADJ = 0, FCAL_EN = 1, MUXOUT_LD_SEL = Lock Detect, RESET = 0, POWERDOWN = 0
          // RAMP_EN = 0, VCO_PHASE_SYNC_EN = 1, ADD_HOLD = 0, OUT_MUTE = 0, FCAL_HPFD_ADJ = 1, FCAL_LPFD_ADJ = 0, FCAL_EN = 1, MUXOUT_LD_SEL = Lock Detect, RESET = 0, POWERDOWN = 0
#endif
0x4E0265, // QUICK_RECAL_EN = 1 (Assisted or unassisted cal), VCO_CAPCTRL_STRT can be set using Equation 5 for Assisted Cal but TI advised to do Full Cal.
#if (FOSC_MHZ == 100)
0x00211C, // RAMP_EN = 0, VCO_PHASE_SYNC_EN = 0, ADD_HOLD = 0, OUT_MUTE = 0, FCAL_HPFD_ADJ = 2, FCAL_LPFD_ADJ = 0, FCAL_EN = 1, MUXOUT_LD_SEL = Lock Detect, RESET = 0, POWERDOWN = 0
          // RAMP_EN = 0, VCO_PHASE_SYNC_EN = 1, ADD_HOLD = 0, OUT_MUTE = 0, FCAL_HPFD_ADJ = 1, FCAL_LPFD_ADJ = 0, FCAL_EN = 1, MUXOUT_LD_SEL = Lock Detect, RESET = 0, POWERDOWN = 0
#elif (FOSC_MHZ == 39)
0x00209C, // RAMP_EN = 0, VCO_PHASE_SYNC_EN = 0, ADD_HOLD = 0, OUT_MUTE = 0, FCAL_HPFD_ADJ = 1, FCAL_LPFD_ADJ = 0, FCAL_EN = 1, MUXOUT_LD_SEL = Lock Detect, RESET = 0, POWERDOWN = 0
          // RAMP_EN = 0, VCO_PHASE_SYNC_EN = 1, ADD_HOLD = 0, OUT_MUTE = 0, FCAL_HPFD_ADJ = 1, FCAL_LPFD_ADJ = 0, FCAL_EN = 1, MUXOUT_LD_SEL = Lock Detect, RESET = 0, POWERDOWN = 0
#endif
};

int setLMX2572LP (int freq_khz, int rxtx, int offset_khz)
{
    int vcofreq_khz;
#ifdef CALCULATE_VCO_PARAMS
    int vcofreq, vco_sel = 0, vco_capctrl_strt = 0, vco_daciset_strt = 0;
#endif
    int chdiv = 4, FOSCin_khz = FOSC_MHZ * 1000, osc_2x = 1, pll_r = 1;
    int pll_n, pll_num, Fpd_khz, pll_den;
    Fpd_khz = (FOSCin_khz * osc_2x) / pll_r;
    pll_den = 1000000;
#if (FOSC_MHZ == 100)
    vcofreq_khz = (freq_khz + offset_khz) * chdiv;
#elif (FOSC_MHZ == 39)
    int error = 1;
    while (error)
    {
        PRINT_LMX2572LP_INFO ("chdiv = %d\n", chdiv);
        if (sync_mode == 0)
        {
            vcofreq_khz = (freq_khz + offset_khz) * chdiv;
            if (vcofreq_khz >= 3200000)
            {
                PRINT_LMX2572LP_INFO ("vcofreq_khz = %d\n", vcofreq_khz);
                error = 0;
            }
            else
            {
                chdiv *= 2;
                PRINT_LMX2572LP_INFO ("vcofreq_khz = %d, chdiv now = %d\n", vcofreq_khz, chdiv);
            }
        }
        else
        {
            // VCO_PHASE_SYNC = 1 adds an extra divide by 2 in fVCO feedback path
            vcofreq_khz = ((freq_khz + offset_khz) * chdiv) / 2;
            if (vcofreq_khz >= 1600000)
            {
                PRINT_LMX2572LP_INFO ("vcofreq_khz = %d\n", vcofreq_khz);
                error = 0;
            }
            else
            {
                chdiv *= 2;
                PRINT_LMX2572LP_INFO ("vcofreq_khz = %d, chdiv now = %d\n", vcofreq_khz, chdiv);
            }
        }
    }
#else
#error FOSC_MHZ must be either 100 MHz or 39 MHz
#endif
    PRINT_LMX2572LP_INFO ("Fvco_khz = %d\n\r", vcofreq_khz);
    if (sync_mode == 0)
    {
        if ((vcofreq_khz < 3200000) || (vcofreq_khz > 6400000))
        {
            PRINT_LMX2572LP_ERROR ("ERROR: VCO %d out of range!\n", vcofreq_khz);
            return 1;
        }
    }
    else
    {
        if ((vcofreq_khz < 1600000) || (vcofreq_khz > 3200000))
        {
            PRINT_LMX2572LP_ERROR ("ERROR: VCO %d out of range!\n", vcofreq_khz);
            return 1;
        }
    }
    // RX operates at double the frequency due to divide by 2 in downconvertor
    if (rxtx == 1)
        chdiv /= 2;
    pll_n = vcofreq_khz / Fpd_khz;
    PRINT_LMX2572LP_INFO ("PLL_N = %d\n\r", pll_n);
    if (pll_n < 32)
    {
        PRINT_LMX2572LP_ERROR ("ERROR: For MASH_ORDER = 3, N should be at least 32: %d\n\r", pll_n);
        return 1;
    }
    pll_num = (((uint64_t)vcofreq_khz * pll_den) / Fpd_khz) - (pll_n * pll_den);
    uint64_t temp1 = (pll_num + pll_n * pll_den) * FOSC_MHZ, temp2 = 3200 * pll_den;
#if (FOSC_MHZ == 39)
    if (sync_mode == 1) temp2 = 1600 * pll_den;
#endif
    // Make sure Fvco is not less than 3.2 GHz / 1.6 GHz
    int count = 0;
    while (temp1 < temp2)
    {
        pll_num++;
        temp1 = (pll_num + pll_n * pll_den) * FOSC_MHZ;
        // For safety, usually should happen max once
        if (++count == 5) break;
    }
    PRINT_LMX2572LP_INFO ("PLL_NUM = %d\n\r", pll_num);
#ifdef CALCULATE_VCO_PARAMS
    vcofreq = vcofreq_khz / 1000;
    // Check if fVCO is below the range
    if (vcofreq < vcoSelection[VCO1 * NUM_VCO_PARAMS])
    {
        PRINT_LMX2572LP_ERROR ("ERROR: Below minimum VCO frequency: %d\n\r", vcofreq);
        return 1;
    }
    // Check if fVCO is above the range
    if (vcofreq > vcoSelection[VCO6 * NUM_VCO_PARAMS])
    {
        PRINT_LMX2572LP_ERROR ("ERROR: Above maximum VCO frequency: %d\n\r", vcofreq);
        return 1;
    }
    for (int i = VCO1; i <= VCO6; i++)
    {
        if (vcofreq > vcoSelection[(i + 1) * NUM_VCO_PARAMS]) continue;
        // Find the values
        int cmin = vcoSelection[i * NUM_VCO_PARAMS + 1];
        int cmax = vcoSelection[i * NUM_VCO_PARAMS + 2];
        int amin = vcoSelection[i * NUM_VCO_PARAMS + 3];
        int amax = vcoSelection[i * NUM_VCO_PARAMS + 4];
        int fmin = vcoSelection[i * NUM_VCO_PARAMS];
        int fmax = vcoSelection[(i + 1) * NUM_VCO_PARAMS];
        vco_sel = i + 1;
        // Equations taken from LMX2572LP datasheet Equations 5 & 6
        vco_capctrl_strt = round (cmin - ((cmin - cmax) * (vcofreq - fmin)) / (fmax - fmin));
        vco_daciset_strt = round (amin - ((amin - amax) * (vcofreq - fmin)) / (fmax - fmin));
        PRINT_LMX2572LP_INFO ("VCO = %d, VCO_CAPCTRL_STRT = %d, VCO_DACISET_START = %d\n\r", vco_sel, vco_capctrl_strt, vco_daciset_strt);
        break;
    }
#endif
    int endcount = sizeof (lmx2572lpregistervalues) / sizeof (lmx2572lpregistervalues[0]);
    for (int i = 0; i < endcount; i++)
    {
        uint32_t reg = lmx2572lpregistervalues[i];
        int send = 1;
        // After setting first frequency no need to send most of the registers
        // The exception is 800 MHz where a VCO change occurs, so should send
        if ((freq_khz != START_FREQ_KHZ) && (freq_khz != VCO_SWITCH_FREQ_KHZ)) send = 0;
        PRINT_LMX2572LP_INFO ("%06lx\n\r", reg);
        if ((reg & 0xFF0000) == 0x240000)
        {
            PRINT_LMX2572LP_INFO ("Register 0x24, insert PLL_N = %d\n\r", pll_n);
            // Insert into bottom 16 bits
            reg = (reg & 0xFF0000) | pll_n;
            // Always send PLL_N
            send = 1;
        }
        else if ((reg & 0xFF0000) == 0x2C0000)
        {
            PRINT_LMX2572LP_INFO ("Register 0x2C, insert OUTA_PWR for PLL = %d\n\r", pll_gain);
            // Insert into position
            reg = (reg & 0xFFC0FF) | ((uint16_t)pll_gain << 8);
        }
        else if ((reg & 0xFF0000) == 0x2B0000)
        {
            PRINT_LMX2572LP_INFO ("Register 0x2B, insert PLL_NUM lower = %d\n\r", pll_num & 0xFFFF);
            // Insert into bottom 16 bits
            reg = (reg & 0xFF0000) | (pll_num & 0xFFFF);
            // Always send PLL_NUM
            send = 1;
        }
        else if ((reg & 0xFF0000) == 0x2A0000)
        {
            PRINT_LMX2572LP_INFO ("Register 0x2A, insert PLL_NUM upper = %d\n\r", pll_num >> 16);
            // Insert into bottom 16 bits
            reg = (reg & 0xFF0000) | (pll_num >> 16);
            // Always send PLL_NUM
            send = 1;
        }
#ifdef CALCULATE_VCO_PARAMS
        else if ((reg & 0xFF0000) == 0x110000)
        {
            PRINT_LMX2572LP_INFO ("Register 0x11, insert VCO_DACISET_STRT = %d\n\r", vco_daciset_strt);
            // Insert 9-bit field
            reg = (reg & 0xFFFE00) | (vco_daciset_strt << 0);
        }
        else if ((reg & 0xFF0000) == 0x4E0000)
        {
            PRINT_LMX2572LP_INFO ("Register 0x4E, insert VCO_CAPCTRL_STRT = %d\n\r", vco_capctrl_strt);
            // Insert 8-bit field
            reg = (reg & 0xFFFE01) | (vco_capctrl_strt << 1);
        }
        else if ((reg & 0xFF0000) == 0x140000)
        {
            PRINT_LMX2572LP_INFO ("Register 0x14, insert VCO_SEL = %d\n\r", vco_sel);
            // Insert 3-bit field
            reg = (reg & 0xFFC7FF) | (vco_sel << 11);
        }
#else
        else if ((reg & 0xFF0000) == 0x4E0000)
        {
            // Always send QUICK_RECAL_EN
            send = 1;
        }
#endif
        else if ((reg & 0xFF0000) == 0x4B0000)
        {
#if 1
            // RX operates at double the frequency due to divide by 2 in downconvertor
            int chdivfield = 0;
            // Only these three settings are relevant for us in our frequency range
            if (chdiv == 2) chdivfield = 0;
            else if (chdiv == 4) chdivfield = 1;
            else if (chdiv == 8) chdivfield = 3;
#else
            // RX at same frequency as TX for Les's phase testing
            int chdivfield = 1;
#endif
            PRINT_LMX2572LP_INFO ("Register 0x4B, insert CHDIV = %d\n\r", chdivfield);
            // Insert 5-bit field
            reg = (reg & 0xFFF83F) | (chdivfield << 6);
        }
        else if ((reg == 0x3A0001) && (sync_mode == 0))
        {
            // Only send INPIN_IGNORE = 0 if SYNC is set
            send = 0;
        }
#if (FOSC_MHZ == 100)
        else if ((reg == 0x0BB028) && (sync_mode == 0))
        {
            // Only send PLL_R = 2 if SYNC is set
            send = 0;
        }
#endif
#if (FOSC_MHZ == 100)
        else if (reg == 0x00211E)
#elif (FOSC_MHZ == 39)
        else if (reg == 0x00209E)
#endif
        {
            // Set VCO_PHASE_SYNC if SYNC is set
            // RESET will be done only for the first frequency in a sweep
            if (sync_mode == 1)
                reg |= (1 << 14);
        }
#if (FOSC_MHZ == 100)
        else if (reg == 0x00211C)
#elif (FOSC_MHZ == 39)
        else if (reg == 0x00209C)
#endif
        {
            // Set VCO_PHASE_SYNC if SYNC is set
            if (sync_mode == 1)
                reg |= (1 << 14);
            // Always send final two FCAL_EN=1
            send = 1;
        }
        PRINT_LMX2572LP_INFO ("%06lx\n\r", reg);
        // Insert logic to not send CALCULATE values after starting frequency of the sweep
        if (send)
        {
            writeLMX2572LP (reg);
        }
    }

    return 0;
}

void powerdownLMX2572LP (void)
{
    // Sets powerdown pin in Reg0
#if (FOSC_MHZ == 100)
    writeLMX2572LP (0x00211D);
#elif (FOSC_MHZ == 39)
    writeLMX2572LP (0x00209D);
#endif
}

/* End of file */
