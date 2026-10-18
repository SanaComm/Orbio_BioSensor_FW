#include "em_common.h"
#include "app.h"
#include "em_gpio.h"
#include "em_cmu.h"
#include "pin_config.h"
#include "sli_cli_io.h"
#include "sl_spidrv_instances.h"
#include "sl_spidrv_inst0_config.h"
#include "sl_udelay.h"

#define PRINT_SWEEP_INFO printf
#define PRINT_SWEEP_DEBUG(...)

bool userinput;

#undef MEASURE_TIMING
#ifdef MEASURE_TIMING
#define STORETIME_SIZE (2000)
static uint32_t storetime[STORETIME_SIZE];
static int storetimeindex;
#endif

#ifdef STORE_SWEEPS

#define NUM_STORE_SWEEPS (4)
#define STORESWEEP_SIZE (NUM_STORE_SWEEPS * sizeof (resultArray))
static char storesweep[STORESWEEP_SIZE];
static unsigned int storesweepindex;
static bool storeprintdone;

void initStoreSweep (void)
{
    storeprintdone = false;
    storesweepindex = 0;
}

#endif

#undef USE_PATTERN
#ifdef USE_PATTERN
static int patternindex;
#endif

static void waitForPLLLock (void)
{
    // Wait for PLL lock
    int lock = 0;
    while (lock == 0)
        lock = GPIO_PinInGet (LO_PLL_MUX_PORT, LO_PLL_MUX_PIN);
    PRINT_SWEEP_DEBUG ("LMX2572LP locked\n\r");
}

void doOneSweep (void)
{
#ifdef MEASURE_TIMING
    uint32_t time0 = sl_sleeptimer_get_tick_count();
    storetimeindex = 0;
#endif
#ifdef USE_PATTERN
    patternindex = 0;
#endif
    // Prepare to get ADC samples for a full sweep
    resetResultArray ();
    userinput = false;
    // Set to fast clock rate for PLL
    SPIDRV_SetBitrate (sl_spidrv_inst0_handle, CMU_ClockFreqGet (cmuClock_PCLK) / 2);
    for (int i = START_FREQ_KHZ; i < END_FREQ_KHZ; i += FREQ_STEP_KHZ)
    {
        if (iq_print_enabled == false) { PRINT_SWEEP_INFO ("."); }
#ifdef MEASURE_TIMING
        uint32_t time1 = sl_sleeptimer_get_tick_count();
#endif
        // Set LO frequency: note this goes to LO so should be X2
        setLMX2572LP (i, 1, 0);

#ifdef MEASURE_TIMING
        uint32_t time2 = sl_sleeptimer_get_tick_count();
#endif

        waitForPLLLock ();

#ifdef MEASURE_TIMING
        uint32_t time3 = sl_sleeptimer_get_tick_count();
#endif

        // For timing measurement
        GPIO_PinModeSet(TP1_PORT, TP1_PIN, gpioModePushPull, 1);

#ifdef MEASURE_TIMING
        uint32_t time4 = sl_sleeptimer_get_tick_count();
#endif

#ifdef USE_PATTERN
        for (int k = 0; k < NUM_SAMPLES_PER_FREQ; k++, patternindex++)
        {
            resultArray[patternindex][0] = patternindex * 10 - 1000;
            resultArray[patternindex][1] = patternindex * 5 - 2000;
        }
#else
        // For timing measurement of a particular frequency
        if (((((i - START_FREQ_KHZ) / FREQ_STEP_KHZ) + 1) == trigfreq) || (0 == trigfreq))
        {
            GPIO_PinOutClear(TP1_PORT, TP1_PIN);
            GPIO_PinOutSet(TP1_PORT, TP1_PIN);
        }
        GPIO_PinOutClear(RAMP_DIR_PORT, RAMP_DIR_PIN);
        // Get all ADC samples at a frequency
        doIADC ();
        GPIO_PinOutClear(RAMP_DIR_PORT, RAMP_DIR_PIN);
#endif

#ifdef MEASURE_TIMING
        uint32_t time5 = sl_sleeptimer_get_tick_count();
        // Store as much timing measurement information as possible
        if (storetimeindex < (STORETIME_SIZE - 8))
        {
            storetime[storetimeindex++] = time2 - time1;
            storetime[storetimeindex++] = time3 - time2;
            storetime[storetimeindex++] = time4 - time3;
            storetime[storetimeindex++] = time5 - time4;
        }
#endif

        // User input related
        if (manualsweep == true)
        {
            int c = -1;
            // Wait for spacebar to continue to next frequency
            while (c != ' ')
            {
                c = sli_cli_io_getchar ();
            }
        }
        else if (iq_print_enabled == false)
        {
            int c = sli_cli_io_getchar ();
            if ((c != EOF) && ((char)c != '\0'))
            {
                userinput = true;
                break;
            }
        }
    }
    // Set back to default clock rate for other devices
    SPIDRV_SetBitrate (sl_spidrv_inst0_handle, SL_SPIDRV_INST0_BITRATE);
    PRINT_SWEEP_INFO (" sweep done\n\r");
#ifdef MEASURE_TIMING
    uint32_t time6 = sl_sleeptimer_get_tick_count();
    for (int i = 0; i < storetimeindex; i++)
    {
        PRINT_SWEEP_INFO ("%ld\n\r", storetime[i]);
    }
    PRINT_SWEEP_INFO ("Ticks in one sweep = %ld\n\r", time6 - time0);
#endif

    if (iq_print_enabled == true)
    {
        for (int i = 0; i < RESULT_SIZE; i++)
        {
            PRINT_SWEEP_INFO ("%d,%d\n\r", resultArray[i][0], resultArray[i][1]);
        }
    }

#ifdef STORE_SWEEPS
    // Only store data sent over BLE
    if (getTransferEnabled () == true)
    {
        // Store latest sweep
        if (storesweepindex < (sizeof (storesweep)))
        {
            memcpy (&storesweep[storesweepindex], resultArray, sizeof (resultArray));
            storesweepindex += sizeof(resultArray);
        }
    }
    // Print all stored samples in the end
    if ((storeprintdone == false) && (storesweepindex == sizeof (storesweep)))
    {
        int16_t *storesweepptr = (int16_t *)storesweep;
        for (unsigned int i = 0; i < sizeof (storesweep) / sizeof (uint32_t); i++, storesweepptr += 2)
        {
            PRINT_SWEEP_INFO ("%d,%d\n\r", *storesweepptr, *(storesweepptr + 1));
        }
        storeprintdone = true;
    }
#endif
}
