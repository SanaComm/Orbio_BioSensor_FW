#include "em_common.h"
#include "app.h"
#include "em_gpio.h"
#include "em_cmu.h"
#include "pin_config.h"
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

#undef USE_PATTERN
#ifdef USE_PATTERN
static int patternindex;
#endif

#ifdef USE_ATOMIC
// To keep the LED timer from firing during PLL+IADC operation
// which we need to be as identical in time as possible
static CORE_irqState_t myIRQState;
static void enter_atomic (void)
{
    myIRQState = CORE_EnterAtomic ();
}

void exit_atomic (void)
{
    CORE_ExitAtomic(myIRQState);
}
#endif

static void waitForPLLLock (void)
{
    // Wait for PLL lock
    int lock = 0;
    while (lock == 0)
        lock = GPIO_PinInGet (LO_PLL_MUX_PORT, LO_PLL_MUX_PIN);
    PRINT_SWEEP_DEBUG ("LMX2572LP locked\n\r");
}

#define CONVERTFREQTOINDEX(x) ((x - START_FREQ_KHZ) / FREQ_STEP_KHZ)

void doOneSweep (int sweepcount)
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
        if (samples_print_enabled == false) { PRINT_SWEEP_INFO ("."); }
#ifdef MEASURE_TIMING
        uint32_t time1 = sl_sleeptimer_get_tick_count();
#endif
        // Set RX gains
        setCMX994_LNA_VGA_Gain (lnagain, vgagain);
        // Set IQ offsets based on frequency
        setCMX994_Offset (i_avg_mV[CONVERTFREQTOINDEX(i)], q_avg_mV[CONVERTFREQTOINDEX(i)]);
        // Set LO frequency
        setLMX2572LP (i, 1, 0);

#ifdef MEASURE_TIMING
        uint32_t time2 = sl_sleeptimer_get_tick_count();
#endif

#ifdef USE_ATOMIC
        enter_atomic ();
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

        if (samples_print_enabled == false)
        {
            int c = sli_cli_io_getchar ();
            if ((c != EOF) && ((char)c != '\0'))
            {
                userinput = true;
                break;
            }
        }
    }

    if (run_cal_offsets == true)
    {
        int cntr = 0;
        PRINT_SWEEP_DEBUG ("\n\r");
        for (int i = 0; i < NUM_FREQS; i++)
        {
            int i_sum = 0, q_sum = 0;
            for (int k = 0; k < NUM_SAMPLES_PER_FREQ; k++)
            {
                i_sum += resultArray[cntr][0];
                q_sum += resultArray[cntr][1];
                cntr++;
            }
            i_avg_sum[i][sweepcount] = (i_sum / NUM_SAMPLES_PER_FREQ);
            q_avg_sum[i][sweepcount] = (q_sum / NUM_SAMPLES_PER_FREQ);
            PRINT_SWEEP_DEBUG ("At %d, I:%d, Q:%d\n\r", i, i_sum / NUM_SAMPLES_PER_FREQ, q_sum / NUM_SAMPLES_PER_FREQ);
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

    if (samples_print_enabled == true)
    {
        for (int i = 0; i < RESULT_SIZE; i++)
        {
            PRINT_SWEEP_INFO ("%ld,%ld\n\r", (int32_t)resultArray[i][0], (int32_t)resultArray[i][1]);
        }
    }
}

void doSetFreq (void)
{
    // Set RX gains
    setCMX994_LNA_VGA_Gain (lnagain, vgagain);
    // Set IQ offsets based on frequency
    setCMX994_Offset (i_avg_mV[CONVERTFREQTOINDEX(frequency_to_set_khz)], q_avg_mV[CONVERTFREQTOINDEX(frequency_to_set_khz)]);
    // Set to fast clock rate for PLL
    SPIDRV_SetBitrate (sl_spidrv_inst0_handle, CMU_ClockFreqGet (cmuClock_PCLK) / 2);
    // First set the base frequency so that PLL code will reset and do full calibration
    if (frequency_to_set_khz < VCO_SWITCH_FREQ_KHZ)
        setLMX2572LP (START_FREQ_KHZ, 1, 0);
    else
        setLMX2572LP (VCO_SWITCH_FREQ_KHZ, 1, 0);
    waitForPLLLock ();
    if ((frequency_to_set_khz != START_FREQ_KHZ) && (frequency_to_set_khz != VCO_SWITCH_FREQ_KHZ))
    {
        // Set LO frequency
        setLMX2572LP (frequency_to_set_khz, 1, 0);
        waitForPLLLock ();
    }
    // Set back to default clock rate for other devices
    SPIDRV_SetBitrate (sl_spidrv_inst0_handle, SL_SPIDRV_INST0_BITRATE);
    PRINT_SWEEP_INFO ("Frequency %ld MHz set\n\r", frequency_to_set_khz / 1000);
}

/* End of file */
