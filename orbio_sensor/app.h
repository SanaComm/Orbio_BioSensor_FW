/***************************************************************************//**
 * @file
 * @brief Application interface provided to main().
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
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
 ******************************************************************************/

#ifndef APP_H
#define APP_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "sli_cli_io.h"
#include <sl_sleeptimer.h>
#include "sl_iostream_usart_inst0_config.h"

#define USE_BLE
#define USE_PPG
#define BLE_KEEP_ALIVE
#if (SL_IOSTREAM_USART_INST0_RESTRICT_ENERGY_MODE_TO_ALLOW_RECEPTION == 1)
#define CLI_KEEP_ALIVE
#endif
#undef USE_ATOMIC

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
void app_init(void);

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
void app_process_action(void);

/**************************************************************************//**
 * Custom command table
 *****************************************************************************/
void cli_custom_cmd_init(void);

/**************************************************************************//**
 * CMX994 downconvertor related
 *****************************************************************************/
#define CMX994_MAX_OFFSET (31)
#define CMX994_MAX_LNA_GAIN (3)
#define CMX994_MAX_VGA_GAIN (7)
void resetCMX994 (void);
void enableCMX994 (void);
void setCMX994_Offset (int8_t ioffset, int8_t qoffset);
void setCMX994_LNA_VGA_Gain (uint8_t lnagain, uint8_t vgagain);
extern uint8_t lnagain, vgagain;

/**************************************************************************//**
 * LMX2572LP PLL related
 *****************************************************************************/
#define LMX2572LP_MAX_GAIN (18)
void powerdownLMX2572LP (void);
int setLMX2572LP (int freq_khz, int rxtx, int offset_khz);

/**************************************************************************//**
 * Sweep related
 *****************************************************************************/
// Number of inputs collected for one frequency
#define NUM_SAMPLES_PER_FREQ (32)
// Sweep range
#define START_FREQ_KHZ (700000)
#define END_FREQ_KHZ (1090000)
#define FREQ_STEP_KHZ (10000)
#define VCO_SWITCH_FREQ_KHZ (800000)
#define NUM_FREQS ((END_FREQ_KHZ - START_FREQ_KHZ) / FREQ_STEP_KHZ)
#define RESULT_SIZE (NUM_FREQS * NUM_SAMPLES_PER_FREQ)
#define NUM_CAL_SWEEPS (100)

void doOneSweep (int sweepcount);
void doSetFreq (void);

/**************************************************************************//**
 * Sweep related globals
 *****************************************************************************/

extern uint8_t pll_gain;
extern uint8_t trigfreq;
extern int8_t ioffset, qoffset;
extern uint8_t sync_mode;
extern bool samples_print_enabled;
extern bool userinput;
extern uint32_t frequency_to_set_khz;
extern int i_avg_mV[NUM_FREQS], q_avg_mV[NUM_FREQS];
extern int i_avg_sum[NUM_FREQS][NUM_CAL_SWEEPS], q_avg_sum[NUM_FREQS][NUM_CAL_SWEEPS];
extern bool run_cal_offsets;

/**************************************************************************//**
 * ADC related
 *****************************************************************************/
// Choose one of these three
#define PRECISION_20BIT
#undef PRECISION_15BIT
#undef PRECISION_12BIT
#ifdef PRECISION_20BIT
#define RESULT_TYPE int32_t
#define IQPRECISION (20)
#else
#define RESULT_TYPE int16_t
#define IQPRECISION (15)
#endif

void initIADC (void);
void disableIADC (void);
void doIADC (void);
void resetResultArray (void);
// Number of ADC scan channels (One I/Q pair)
#define NUM_INPUTS 2
// IADC voltage reference in mV
#define VREF_MV (3300L)
extern RESULT_TYPE resultArray[RESULT_SIZE][NUM_INPUTS];

/**************************************************************************//**
 * 2-Means algorithm related
 *****************************************************************************/

void two_means(int *points, int n, int *cent0, int *cent1);
void two_means_2D(int *Xpoints, int *Ypoints, int n, int *Xcent0, int *Ycent0, int *Xcent1, int *Ycent1);

/**************************************************************************//**
 * MAX86171 related, needed to map to SiLabs with minimal code change
 *****************************************************************************/

#define HAL_Delay sl_sleeptimer_delay_millisecond

#endif // APP_H
