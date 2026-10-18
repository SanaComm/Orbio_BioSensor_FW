#include "em_common.h"
#include "app.h"
#include "pin_config.h"

/***************************************************************************//**
 * @file main_scan_interrupt_dvl_4.c
 * @brief Use the ADC to take repeated non-blocking measurements on
 * multiple inputs.
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 *******************************************************************************
 * # Evaluation Quality
 * This code has been minimally tested to ensure that it builds and is suitable 
 * as a demonstration for evaluation purposes only. This code will be maintained
 * at the sole discretion of Silicon Labs.
 ******************************************************************************/

#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "em_iadc.h"
#include "em_gpio.h"
#include "em_ldma.h"
#include "sl_sleeptimer.h"
#include "dmadrv.h"

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

// Option to use ADC's internal timer for exact sampling frequency
#define USE_ADC_TIMER
// Option to use LDMA to transfer ADC samples
#define USE_ADC_LDMA
// For testing FIFO timing
#define TOGGLE_RAMP_DIR

// Choose one of these three
#undef PRECISION_20BIT
#define PRECISION_15BIT
#undef PRECISION_12BIT

// Set CLK_ADC to 10MHz
#define CLK_SRC_ADC_FREQ      20000000  // CLK_SRC_ADC
#define CLK_ADC_FREQ          10000000  // CLK_ADC - 10 MHz max in normal mode

// Sampling frequency desired
#define SAMPLING_FREQ_HZ (40000)

// IADC voltage reference in mV
#define VREF_MV (3300L)

/*
 * Specify the IADC input using the IADC_PosInput_t typedef.
 * As these are specialised Analog Input pins, no GPIO mapping
 * to the bus is needed.
 */
#define IADC_POS_INPUT_0_PORT_PIN     iadcPosInputPadAna0;
#define IADC_POS_INPUT_1_PORT_PIN     iadcPosInputPadAna2;
#define IADC_NEG_INPUT_0_PORT_PIN     iadcNegInputPadAna1;
#define IADC_NEG_INPUT_1_PORT_PIN     iadcNegInputPadAna3;

#if 0
#define IADC_POS_INPUT_0_BUS          BBUSALLOC
#define IADC_POS_INPUT_0_BUSALLOC     GPIO_BBUSALLOC_BEVEN0_ADC0
#define IADC_NEG_INPUT_0_BUS          BBUSALLOC
#define IADC_NEG_INPUT_0_BUSALLOC     GPIO_BBUSALLOC_BODD0_ADC0
#define IADC_POS_INPUT_1_BUS          BBUSALLOC
#define IADC_POS_INPUT_1_BUSALLOC     GPIO_BBUSALLOC_BEVEN0_ADC0
#define IADC_NEG_INPUT_1_BUS          BBUSALLOC
#define IADC_NEG_INPUT_1_BUSALLOC     GPIO_BBUSALLOC_BODD0_ADC0
#endif

/*******************************************************************************
 ***************************   GLOBAL VARIABLES   ******************************
 ******************************************************************************/

static bool adc_init_done = false;
static volatile bool conversiondone;
int16_t resultArray[RESULT_SIZE][NUM_INPUTS];
static volatile int resultArrayIndex;
// TODO: Why is this a uint16_t * and not int16_t *? Was there a specific reason?
// Change after testing if needed for consistency.
static uint16_t *resultArrayPtr;
#ifndef USE_ADC_LDMA
static volatile int resultArrayIndexScan;
#endif

/**************************************************************************//**
 * @brief  IADC initialization
 *****************************************************************************/
void initIADC (void)
{
  // Declare initialization structures
  IADC_Init_t init = IADC_INIT_DEFAULT;
  IADC_AllConfigs_t initAllConfigs = IADC_ALLCONFIGS_DEFAULT;
  IADC_InitScan_t initScan = IADC_INITSCAN_DEFAULT;
  IADC_ScanTable_t initScanTable = IADC_SCANTABLE_DEFAULT;

  CMU_ClockEnable(cmuClock_IADC0, true);
  CMU_ClockEnable(cmuClock_GPIO, true);
   
  // Select clock for IADC as FSRCO which is 20MHz
  // Use the FSRC0 as the IADC clock so it can run in EM2
  CMU_ClockSelectSet(cmuClock_IADCCLK, cmuSelect_FSRCO);

  // Remain powered-up between conversions to avoid warmup time before each conversion
  init.warmup = iadcWarmupKeepWarm;

  // Set the HFSCLK prescale value here
  init.srcClkPrescale = IADC_calcSrcClkPrescale(IADC0, CLK_SRC_ADC_FREQ, 0);

#ifdef USE_ADC_TIMER
  /*
   * The IADC local timer runs at CLK_SRC_ADC_FREQ, which is at least
   * 2x CLK_ADC_FREQ. CLK_SRC_ADC_FREQ in this example is equal to the
   * HFXO frequency. Dividing the frequency of the HFXO by 1000 will give
   * the tick count for 1 ms trigger rate.
   * For example - if HFXO freq = 38.4 MHz,
   *
   * ticks for 1 ms trigger = 38400000 / 1000
   * ticks =  38400
   */
  init.timerCycles = CMU_ClockFreqGet(cmuClock_IADCCLK) / SAMPLING_FREQ_HZ;
#endif

  /*
   * Configuration 0 is used by both scan and single conversions by
   * default.  Use internal bandgap as the reference and specify the
   * reference voltage in mV.
   *
   * Resolution is not configurable directly but is based on the
   * selected oversampling ratio (osrHighSpeed), which defaults to
   * 2x and generates 12-bit results.
   */
  initAllConfigs.configs[0].reference = iadcCfgReferenceVddx;
  initAllConfigs.configs[0].analogGain = iadcCfgAnalogGain0P5x;
  initAllConfigs.configs[0].vRef = VREF_MV;
#ifdef PRECISION_20BIT
  initAllConfigs.configs[0].osrHighSpeed = iadcCfgOsrHighSpeed16x;
  initAllConfigs.configs[0].digAvg = iadcDigitalAverage4;
#endif
  #ifdef PRECISION_15BIT
  initAllConfigs.configs[0].osrHighSpeed = iadcCfgOsrHighSpeed8x;
  initAllConfigs.configs[0].digAvg = iadcDigitalAverage2;
#endif
#ifdef PRECISION_12BIT
  initAllConfigs.configs[0].osrHighSpeed = iadcCfgOsrHighSpeed2x;
  initAllConfigs.configs[0].digAvg = iadcDigitalAverage1;
#endif

  /*
   * CLK_SRC_ADC is prescaled to derive the intended CLK_ADC frequency.
   *
   * Based on the default 2x oversampling rate (OSRHS)...
   *
   * conversion time = ((4 * OSRHS) + 2) / fCLK_ADC
   *
   * ...which, results in a maximum sampling rate of 833 ksps with the
   * 2-clock input multiplexer switching time is included.
   * OSRHS = 2, so 4 * OSRHS + 2 = 10, add 2 cycles to switch GPIO
   * so total 12 cycles per conversion, at 10 MHz
   *
   */
  initAllConfigs.configs[0].adcClkPrescale = IADC_calcAdcClkPrescale(IADC0,
                                             CLK_ADC_FREQ,
                                             0,
                                             iadcCfgModeNormal,
                                             init.srcClkPrescale);

  /*
   * Set the SCANFIFODVL flag when there are 2 entries in the scan
   * FIFO.  Note that in this example, the interrupt associated with
   * the SCANFIFODVL flag in the IADC_IF register is not used.
   *
   * Similarly, the fifoDmaWakeup member of the initScan structure
   * is left at its default setting of false, so LDMA service is not
   * requested when the FIFO holds the specified number of samples.
   */
  initScan.dataValidLevel = iadcFifoCfgDvl2;
#ifdef PRECISION_20BIT
  initScan.alignment = iadcAlignRight20;
#endif
#ifdef PRECISION_15BIT
  initScan.alignment = iadcAlignRight16;
#endif
#ifdef PRECISION_12BIT
  initScan.alignment = iadcAlignRight12;
#endif

#ifdef USE_ADC_TIMER
  // The IADC local timer triggers conversions.
  initScan.triggerSelect = iadcTriggerSelTimer;
#endif
  // Tag FIFO entry with scan table entry id.
  initScan.showId = true;

#ifdef USE_ADC_LDMA
  /*
   * Trigger continuously once scan is started.  Note that
   * initScan.start = false by default, so conversions must be started
   * manually with IADC_command(IADC0, iadcCmdStartScan).
   *
   * Set the SCANFIFODVL flag when scan FIFO holds 2 entries.  In this
   * example, the interrupt associated with the SCANFIFODVL flag in
   * the IADC_IF register is not used.
   *
   * Enable DMA wake-up to save the results when the specified FIFO
   * level is hit.
   */
  initScan.triggerAction = iadcTriggerActionOnce;
  initScan.fifoDmaWakeup = true;
#endif

  /*
   * Configure two differential entries in scan table.
   */
  initScanTable.entries[0].posInput = IADC_POS_INPUT_0_PORT_PIN;
  initScanTable.entries[0].negInput = IADC_NEG_INPUT_0_PORT_PIN;
  initScanTable.entries[0].includeInScan = true;

  initScanTable.entries[1].posInput = IADC_POS_INPUT_1_PORT_PIN;
  initScanTable.entries[1].negInput = IADC_NEG_INPUT_1_PORT_PIN;
  initScanTable.entries[1].includeInScan = true;

  // Initialize IADC
  IADC_init(IADC0, &init, &initAllConfigs);

  // Initialize scan
  IADC_initScan(IADC0, &initScan, &initScanTable);

#if 0
  // Allocate the analog bus for ADC0 inputs
  GPIO->IADC_POS_INPUT_0_BUS |= IADC_POS_INPUT_0_BUSALLOC;
  GPIO->IADC_POS_INPUT_1_BUS |= IADC_POS_INPUT_1_BUSALLOC;
  GPIO->IADC_NEG_INPUT_0_BUS |= IADC_NEG_INPUT_0_BUSALLOC;
  GPIO->IADC_NEG_INPUT_1_BUS |= IADC_NEG_INPUT_1_BUSALLOC;
#endif

#if !defined(USE_ADC_LDMA) || defined (TOGGLE_RAMP_DIR)
  // Clear any previous interrupt flags
  IADC_clearInt(IADC0, _IADC_IF_MASK);

  // Enable Scan interrupts
  IADC_enableInt(IADC0, IADC_IEN_SCANTABLEDONE);

  // Enable ADC interrupts
  NVIC_ClearPendingIRQ(IADC_IRQn);
  NVIC_EnableIRQ(IADC_IRQn);
#endif
  adc_init_done = true;
}

#ifndef USE_ADC_LDMA

/**************************************************************************//**
 * @brief  IADC interrupt handler
 *****************************************************************************/
void IADC_IRQHandler(void)
{
  IADC_Result_t result = {0, 0};

  // While the FIFO count is non-zero...
  while (IADC_getScanFifoCnt(IADC0))
  {
    // Pull a scan result from the FIFO
    result = IADC_pullScanFifoResult(IADC0);

    /*
     * Calculate the voltage converted as follows:
     *
     * For single-ended conversions, the result can range from 0 to
     * +Vref, i.e., for Vref = VBGR = 1.21V, and with analog gain = 0.5
     * Convert ADC reading to mV based on the above.
     */
    if (((resultArrayIndex & 1) == result.id) && (resultArrayIndexScan < (NUM_SAMPLES_PER_FREQ * NUM_INPUTS)))
    {
        resultArrayPtr[resultArrayIndex++] = result.data;
        resultArrayIndexScan++;
    }
  }

  /*
   * Clear the scan table complete interrupt.  Reading FIFO results
   * does not do this automatically.
   */
  IADC_clearInt(IADC0, IADC_IF_SCANTABLEDONE);

#ifdef USE_ADC_TIMER
  if (resultArrayIndexScan >= (NUM_SAMPLES_PER_FREQ * NUM_INPUTS))
  {
      IADC_command(IADC0, iadcCmdStopScan);
      conversiondone = true;
  }
#else
  conversiondone = true;
#endif
}

void doIADC (void)
{
    resultArrayIndexScan = 0;
#ifndef USE_ADC_TIMER
    for (int i = 0; i < NUM_SAMPLES_PER_FREQ; i++)
    {
#endif
        // Start scan
        conversiondone = false;
        IADC_command(IADC0, iadcCmdStartScan);
        // Wait for scan to complete
        while (conversiondone == false);
#ifndef USE_ADC_TIMER
    }
#endif
}

#else

#ifdef TOGGLE_RAMP_DIR

/**************************************************************************//**
 * @brief  IADC interrupt handler to toggle RAMP_DIR test pin
 *****************************************************************************/

void IADC_IRQHandler(void)
{
  /*
   * Clear the scan table complete interrupt.  Reading FIFO results
   * does not do this automatically.
   */
  IADC_clearInt(IADC0, IADC_IF_SCANTABLEDONE);
  GPIO_PinOutToggle(RAMP_DIR_PORT, RAMP_DIR_PIN);
}

#endif

#endif

void resetResultArray (void)
{
    resultArrayIndex = 0;
    resultArrayPtr = (uint16_t *)&resultArray[0][0];
}

#ifdef USE_ADC_LDMA

/**************************************************************************//**
 * @brief  LDMA Handler callback
 *****************************************************************************/
static void LDMA_IRQHandler_cb(void)
{
    IADC_command(IADC0, iadcCmdStopScan);
    conversiondone = true;
}

// TODO: The following are done for every frequency step
// Is it possible to not do the Init/AllocateChannel/FreeChannel
// each time, to optimise? However, this is working code from a previous
// project, so don't change it until changes can be made to test it.
static unsigned int dmachannel;
static int initLDMADRV (uint32_t *buffer, uint32_t size)
{
  Ecode_t ecode;

  // Init the LDMA
  ecode = DMADRV_Init();
  if (ecode != ECODE_OK && ecode != ECODE_EMDRV_DMADRV_ALREADY_INITIALIZED) {
    return SL_STATUS_INITIALIZATION;
  }

  // Allocate the LDMA channel
  ecode = DMADRV_AllocateChannel((unsigned int *)&dmachannel, NULL);
  if (ecode != ECODE_OK) {
    return SL_STATUS_INITIALIZATION;
  }

  // Start the (L)DMA to handle IADC transfer
  ecode = DMADRV_PeripheralMemory(dmachannel,
                                  ldmaPeripheralSignal_IADC0_IADC_SCAN,
                                  (void *)buffer,
                                  (void *)&(IADC0->SCANFIFODATA),
                                  true,
                                  size,
                                  dmadrvDataSize4,
                                  (DMADRV_Callback_t)LDMA_IRQHandler_cb,
                                  NULL);
  if (ecode != ECODE_OK) {
    return SL_STATUS_INITIALIZATION;
  }

  return SL_STATUS_OK;
}

static int freeLDMADRV (void)
{
  Ecode_t ecode;
  // Free the allocated LDMA channel
  ecode = DMADRV_FreeChannel(dmachannel);
  if (ecode != ECODE_OK) {
    return SL_STATUS_INITIALIZATION;
  }

  return SL_STATUS_OK;
}

// Buffer to store IADC samples
#define DUMMY_SAMPLES (5)
uint32_t scanBuffer[NUM_INPUTS * (NUM_SAMPLES_PER_FREQ + DUMMY_SAMPLES)];

void doIADC (void)
{
    conversiondone = false;
    initLDMADRV(scanBuffer, NUM_INPUTS * (NUM_SAMPLES_PER_FREQ + DUMMY_SAMPLES));
    // Start scan
#ifdef USE_ADC_TIMER
    // Enable the IADC timer (must be after the IADC is initialized)
    IADC_command(IADC0, iadcCmdEnableTimer);
#endif
    IADC_command(IADC0, iadcCmdStartScan);
    while (conversiondone == false);
#ifdef USE_ADC_TIMER
    // Disable the IADC timer once a scan is done
    IADC_command(IADC0, iadcCmdDisableTimer);
#endif
    freeLDMADRV();
    uint32_t *ptr = &scanBuffer[NUM_INPUTS * DUMMY_SAMPLES];
    for (int i = 0; i < NUM_INPUTS * NUM_SAMPLES_PER_FREQ; i++)
        resultArrayPtr[resultArrayIndex++] = *ptr++;
}

#endif

// Copied from em_iadc.c as it is static there
void disableIADC (void)
{
    // Will get stuck if not enabled before, so check
    if (adc_init_done == false) return;
#if defined(IADC_STATUS_SYNCBUSY)
    while ((IADC0->STATUS & IADC_STATUS_SYNCBUSY) != 0U) {
      // Wait for synchronization to finish before disable
    }
#endif
    IADC0->EN_CLR = IADC_EN_EN;
#if defined(_IADC_EN_DISABLING_MASK)
    while (IADC0->EN & _IADC_EN_DISABLING_MASK) {
    }
#endif
}

/* End of file */
