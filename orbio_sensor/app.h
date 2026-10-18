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
#define CMX994_MAX_OFFSET (775)
void resetCMX994 (void);
void enableCMX994 (void);
void setCMX994Offset (int ioffset, int qoffset);

/**************************************************************************//**
 * LMX2572LP PLL related
 *****************************************************************************/
#define LMX2572LP_MAX_GAIN (18)
void powerdownLMX2572LP (void);
int setLMX2572LP (int freq_khz, int rxtx, int offset_khz);
extern uint8_t pll_gain;

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

void doOneSweep (void);
void doSetFreq (void);

/**************************************************************************//**
 * Sweep related globals
 *****************************************************************************/

extern uint8_t trigfreq;
extern int16_t ioffset, qoffset;
extern uint8_t sync_mode;
extern bool iq_print_enabled;
extern bool userinput;
extern uint32_t frequency_to_set_khz;

/**************************************************************************//**
 * ADC related
 *****************************************************************************/
void initIADC (void);
void disableIADC (void);
void doIADC (void);
void resetResultArray (void);
// Number of ADC scan channels (One I/Q pair)
#define NUM_INPUTS 2
extern int16_t resultArray[RESULT_SIZE][NUM_INPUTS];

/**************************************************************************//**
 * MAX86171 related, needed to map to SiLabs with minimal code change
 *****************************************************************************/

#define HAL_Delay sl_sleeptimer_delay_millisecond

#endif // APP_H
