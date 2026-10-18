/***************************************************************************//**
 * @file
 * @brief Core application logic.
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
#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"
#include "em_gpio.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "sl_cli.h"
#include "app.h"
#include "gpiointerrupt.h"
#include "ktd2052a.h"
#include "max86171.h"
#include "ppg_agc.h"
#include "accel.h"
#include "sl_cli_handles.h"
#include "math.h"

#define VERSION (26)

// Types of prints
#define PRINT_INIT printf
#define PRINT_INFO printf
#define PRINT_DEBUG printf
#define PRINT_ERROR printf
#define PRINT_IGNORE(...)

/**************************************************************************************************
    Active module Defines and Variables.
**************************************************************************************************/

typedef enum
{
    ACTIVE_MODULE_NONE = 0,
    ACTIVE_MODULE_SWEEP = 1 << 1,
    ACTIVE_MODULE_PPG = 1 << 2,
    ACTIVE_MODULE_ACCEL = 1 << 3,
    ACTIVE_MODULE_BLE = 1 << 4,
    ACTIVE_MODULE_CLI = 1 << 5,
} app_module_t;

static app_module_t active_module;
// Index them from bit shifted version of active_module using __builtin_ctz
static char *moduleNames[] = {"None", "Sweep", "PPG", "Accel", "BLE", "CLI"};
static uint8_t moduleLedColours[] = {COLOUR_NONE, COLOUR_BLUE, COLOUR_GREEN, COLOUR_YELLOW, COLOUR_MAGENTA, COLOUR_NONE};

static const char *getModuleName(app_module_t module)
{
    if (module == ACTIVE_MODULE_NONE)
        return "None";
    return moduleNames[__builtin_ctz(module)];
}

// TODO: What if a module is stopped from CLI or BLE, and there is a timer/interrupt that just triggers
// TODO: Current handleNewPacket does not work if two or more packet types have to be interspersed
// TODO: VBAT_ADC periodic read and battery status check
// TODO: Wake/sleep cycle for PPG & Accel modes

/**************************************************************************************************
    Sleep/Wake States Defines and Variables.
**************************************************************************************************/

typedef enum
{
    SLEEP_STATE_SLEEP = 1,
    SLEEP_STATE_WAKE = 2
} sleep_state_t;
static volatile sleep_state_t sleepState;

/**************************************************************************************************
    Variables set/reported via command line / BLE interface
**************************************************************************************************/

uint8_t trigfreq;
uint8_t pll_gain;
uint8_t sync_mode;
uint32_t frequency_to_set_khz;
bool samples_print_enabled;
bool run_cal_offsets;
uint8_t lnagain, vgagain;
#define SWEEP_INTERVAL_DEFAULT_SEC (10 * 60)
#define ACTIVE_TIME_DEFAULT_SEC (0)
static uint16_t sweep_interval_sec;
static uint16_t active_time_sec;
static char reportsettings[60];
// Device name to advertise
static uint8_t devicename[] = {'O', 'r', 'b', 'i', 'o', '-', 0, 0, 0, 0, 0, 0, 0};

/**************************************************************************************************
    MAX86171 Variables.
**************************************************************************************************/

static max86171_t max86171_dev;

/**************************************************************************************************
    Sweep States Defines and Variables.
**************************************************************************************************/

#define SWEEP_START_DELAY_MSEC (50)
typedef enum
{
    SWEEP_STATE_INIT = 1,
    SWEEP_STATE_SWEEP,
    SWEEP_STATE_PENDING,
    SWEEP_STATE_SETFREQ,
    SWEEP_STATE_END,
    SWEEP_STATE_NONE,
} sweep_state_t;
static sweep_state_t sweepState;

// To keep track of when the sweeps started, to timeout after 30seconds
static uint32_t blestarttime, starttime;
static int sweepcount;

/**************************************************************************//**
 * Periodic activity
 *****************************************************************************/

typedef enum
{
    SIGNAL_SWEEP = 1 << 0,
    SIGNAL_PPG = 1 << 1,
    SIGNAL_ACCEL = 1 << 2,
    SIGNAL_WAKE = 1 << 3,
} app_signal_t;

static sl_sleeptimer_timer_handle_t timer_startsweep;
static sl_sleeptimer_timer_handle_t timer_readaccel;

static void triggerSweep (void);
static void triggerReadAccel (void);

// This timer will fire when sleep is over
// Note: this is called from a timer tick ISR context so needs to be compact
static void timer_cb_startsweep (sl_sleeptimer_timer_handle_t *handle, void *data)
{
    (void)data;
    (void)handle;
    triggerSweep ();
}

// Sends a signal to the event handler to start a sweep
// Called 1) When timer callback is called when waking from sleep or CLI/BLE command to start a sweep
//        2) When handleNewPacket either send a sweep in a single packet or connection is not open
//        3) When app_process_action calls sendNextChunk and the whole sweep has been sent or too many errors
static void triggerSweep (void)
{
    // Trigger sweep activity, making sure to retry if the signal did not work due to resource issue
    if (sl_bt_external_signal (SIGNAL_SWEEP) != SL_STATUS_OK)
    {
        // Start timer to trigger sweeps after a delay
        sl_sleeptimer_stop_timer (&timer_startsweep);
        sl_sleeptimer_start_timer_ms (&timer_startsweep, 10, timer_cb_startsweep, NULL, 0, 0);
    }
}

// This timer will fire when sleep is over
// Note: this is called from a timer tick ISR context so needs to be compact
static void timer_cb_readaccel (sl_sleeptimer_timer_handle_t *handle, void *data)
{
    (void)data;
    (void)handle;
    triggerReadAccel ();
}

static void triggerReadAccel (void)
{
    // Trigger periodic Accel read, making sure to retry if the signal did not work due to resource issue
    if (sl_bt_external_signal (SIGNAL_ACCEL) != SL_STATUS_OK)
    {
        // Start timer to trigger sweeps after a delay
        sl_sleeptimer_stop_timer (&timer_readaccel);
        sl_sleeptimer_start_timer_ms (&timer_readaccel, 10, timer_cb_readaccel, NULL, 0, 0);
    }
}

/**************************************************************************//**
 * GPIO Interrupts
 *****************************************************************************/

// Unused as of now
static void AccelInterrupt (uint8_t pin)
{
    (void)pin;
    if (sleepState == SLEEP_STATE_SLEEP)
    {
        if (sl_bt_external_signal (SIGNAL_WAKE) != SL_STATUS_OK)
        {
            // Print failure but do not retry in case this gets into a loop
            PRINT_ERROR ("ERROR: Signal from Accel interrupt failed!\n\r");
        }
        else
        {
            PRINT_DEBUG ("Accel interrupt wake!\n\r");
            sleepState = SLEEP_STATE_WAKE;
        }
    }
    return;
}

#ifdef USE_PPG

// this interrupt handler is used to trigger read_ppg_fifo() via a signal
static void PPGInterrupt (uint8_t pin)
{
    (void)pin;
    if (sl_bt_external_signal (SIGNAL_PPG) != SL_STATUS_OK)
    {
        // Print failure but do not retry in case this gets into a loop
        PRINT_ERROR ("ERROR: Signal from PPG interrupt failed!\n\r");
    }
    return;
}

#endif

#ifdef USE_BLE

/**************************************************************************************************
    BLE data transfer variables.
**************************************************************************************************/

// Note: right now we only use Notifications so the following two flags are set/cleared at the same
// times. However they are retained just in case Indications are added later, in which case
// transferEnabled will be the common flag and sendNotification will say whether Not or Ind is used.
static bool transferEnabled, sendNotification;
static int bytesRemaining, sendTotalDataLen;
static uint8_t *sendData;
#define MAX_PACKET_LEN (240)

#ifdef STORE_SWEEPS
bool getTransferEnabled (void)
{
    return transferEnabled;
}
#endif

/**************************************************************************************************
    BLE Variables.
**************************************************************************************************/

#define ADVERTISING_INTERVAL_MS 500
// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle;
// To save connection handle
static int OpenedConnectionHandle;

/**************************************************************************//**
 * Utility function for creating unique advertising name
 *****************************************************************************/

static uint8_t nibble_to_hex (uint8_t x)
{
    if (x <= 9)
        x += '0';
    else if ((x >= 10) && (x <= 15))
        x += ('A' - 10);
    else
        x = '?';
    return x;
}

/**************************************************************************************************
    Functions to set parameters for BLE advertising
**************************************************************************************************/

// Add 6 characters of unique ID to advertised name (from 3 bytes)
#define UNIQUE_NAME_LEN (6)
#define UNIQUE_NAME_LEN_OFFSET (6)
#define NIBBLE_LEN (4)

static void setupDeviceName (void)
{
    sl_status_t sc;
    uint32_t unit_id;
    bd_addr address;
    uint8_t address_type;
    // Extract unique ID from BT Address.
    sc = sl_bt_system_get_identity_address(&address, &address_type);
    app_assert_status(sc);
    // Form unique System ID.
    unit_id = (address.addr[2] << 16) | (address.addr[1] << 8) | address.addr[0];
    for (int i = 0; i < UNIQUE_NAME_LEN; i++)
    {
        uint8_t x = (unit_id >> (NIBBLE_LEN * (UNIQUE_NAME_LEN - 1 - i))) & 0xF;
        devicename[UNIQUE_NAME_LEN_OFFSET + i] = nibble_to_hex (x);
    }
    sc = sl_bt_gatt_server_write_attribute_value(gattdb_device_name, UNIQUE_NAME_LEN_OFFSET, UNIQUE_NAME_LEN, &devicename[UNIQUE_NAME_LEN_OFFSET]);
    app_assert_status(sc);
    PRINT_INFO ("Device ID %s\n\r", devicename);
}

static void setupAdvertising (void)
{
    sl_status_t sc;

    // Set TX power to 0dBm
    sl_bt_system_set_tx_power(0, 0, NULL, NULL);

    // Create an advertising set.
    sc = sl_bt_advertiser_create_set(&advertising_set_handle);
    app_assert_status(sc);

    // Generate data for advertising
    sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle, sl_bt_advertiser_general_discoverable);
    app_assert_status(sc);

    // Set advertising interval
    sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        ADVERTISING_INTERVAL_MS * 1.6, // min. adv. interval (milliseconds * 1.6)
        ADVERTISING_INTERVAL_MS * 1.6, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
    app_assert_status(sc);

    // Minimum & Maximum connection interval = 7.5msec (6 * 1.25)
    // Slave latency (how many connection intervals the slave can skip if it has no data to send) = 0 (default)
    // Supervision timeout (time that the connection is maintained although the devices can't communicate at the currently configured connection intervals) = 1000msec (100 * 10)
    sl_bt_connection_set_default_parameters (6, 6, 0, 100, 0, 0xffff);
    // Set maximum possible MTU & PDU sizes for best throughput
    uint16_t val16;
    sl_bt_gatt_server_set_max_mtu (250, &val16);
    sl_bt_connection_set_default_data_length (251);
}

static void startAdvertising (void)
{
    sl_status_t sc;

    // Start advertising and enable connections.
    sc = sl_bt_legacy_advertiser_start(advertising_set_handle, sl_bt_legacy_advertiser_connectable);
    app_assert_status(sc);
    PRINT_INFO ("startAdvertising\n\r");
}

#ifndef BLE_KEEP_ALIVE

// Called when connection is closed as the stack automatically
// restarts advertising. This is only if BLE doesn't need
// to be kept alive.
// If the connection was closed by the client or due to
// bad link, advertising will remain off till the next wake.
static void stopAdvertising (void)
{
    sl_status_t sc;

    // Stop advertising. Returns SL_STATUS_OK if already closed as well
    sc = sl_bt_advertiser_stop(advertising_set_handle);
    app_assert_status(sc);
    PRINT_INFO ("stopAdvertising\n\r");
}

#endif

// Called from setLowPowerMode and from sl_bt_evt_connection_closed_id
// if still in WAKE, as there is no point doing the sample collection
// if there is no open BLE connection to send the samples through
static void closeConnection (void)
{
#ifdef BLE_KEEP_ALIVE
    // Don't close the connection in keep alive case
    return;
#else
    // Close connection only if open
    if (OpenedConnectionHandle != SL_BT_INVALID_CONNECTION_HANDLE)
    {
        sl_bt_connection_close(OpenedConnectionHandle);
        PRINT_INFO ("closeConnection\n\r");
    }
    // else close it after closed event happens
    else
        stopAdvertising ();
#endif
}

/**************************************************************************//**
 * Checks if there is data sending to be initiated
 *****************************************************************************/
static bool notificationTransferInProgress (void)
{
  bool ret = false;
  if(transferEnabled && sendNotification && bytesRemaining)
      ret = true;
  return ret;
}

/**************************************************************************//**
 * Checks if it is ok to sleep now
 *****************************************************************************/
bool app_is_ok_to_sleep(void)
{
  bool ret = true;
  if(notificationTransferInProgress() == true)
  {
      ret = false;
  }
  return ret;
}

/**************************************************************************//**
 * Routine for power manager handler
 *****************************************************************************/
sl_power_manager_on_isr_exit_t app_sleep_on_isr_exit(void)
{
  sl_power_manager_on_isr_exit_t ret = SL_POWER_MANAGER_IGNORE;
  if(notificationTransferInProgress() == true)
  {
      ret = SL_POWER_MANAGER_WAKEUP;
  }
  return ret;
}

/**************************************************************************//**
 * Routines to send BLE packets in chunks
 *****************************************************************************/
#define DATAPRINTF(...)
#undef PRINT_PACKET_TIMES
#ifdef PRINT_PACKET_TIMES
static uint64_t time_now = 0;
#endif

#define MAX_RETRY_COUNT (40)
#define RETRY_BACKOFF_TIME_MSEC (5)
static int sendchunkretrycount;
static uint32_t errtime;

// Send a chunk of data and update remaining bytes to send if successful
// Return false if partial packet i.e. chunk was successfully sent or packet could not be sent
// Return true if packet was fully sent in a single chunk, or reached maximum unsuccessful retries
// This overload of a bool variable is knowingly done and considered okay for now.
static bool sendChunk (void)
{
    bool ret = false;
    sl_status_t status;
    uint32_t nowtime = sl_sleeptimer_tick_to_ms (sl_sleeptimer_get_tick_count());
    // No point trying immediately after resources error, wait a while
    if ((errtime) && ((nowtime - errtime) < RETRY_BACKOFF_TIME_MSEC))
            return false;
    // Check packet size for safety
    int pktlen = MAX_PACKET_LEN;
    if (bytesRemaining < pktlen) pktlen = bytesRemaining;
    // Should not happen
    if (pktlen == 0) return true;
    // Send upto 240 bytes in a packet as per BLE spec
    status = sl_bt_gatt_server_send_notification(OpenedConnectionHandle, gattdb_sweepdata, pktlen, &sendData[sendTotalDataLen - bytesRemaining]);
    if (status != SL_STATUS_OK)
    {
        // Keep a count of retries so that we are not stuck in this state forever, which prevents MCU from sleeping and stalls
        // data transfer till the next time handleNewPacket is called. So just drop the current data and continue.
        // Distinguish between different types of error codes: SL_STATUS_NO_MORE_RESOURCE, INVALID_HANDLE, NOT_CONNECTED, ...
        // No point retrying for anything but the first error type.
        if (status == SL_STATUS_NO_MORE_RESOURCE)
        {
            if (++sendchunkretrycount >= MAX_RETRY_COUNT)
            {
                sendchunkretrycount = 0;
                bytesRemaining = 0;
                ret = true;
                errtime = 0;
                PRINT_ERROR ("ERROR: sendChunk retry count for error 0x%lx limit reached, drop chunk and continue!\n\r", status);
            }
            errtime = sl_sleeptimer_tick_to_ms (sl_sleeptimer_get_tick_count());
        }
        else
        {
            sendchunkretrycount = 0;
            bytesRemaining = 0;
            ret = true;
            errtime = 0;
            PRINT_ERROR ("ERROR: sendChunk error 0x%lx, no need to retry, drop chunk and continue!\n\r", status);
        }
#ifdef PRINT_PACKET_TIMES
        uint64_t new_time_now;
        sl_sleeptimer_tick64_to_ms (sl_sleeptimer_get_tick_count64 (), &new_time_now);
        PRINT_ERROR ("ERR:%ld\n\r", (uint32_t)(new_time_now - time_now));
        time_now = new_time_now;
#endif
    }
    else
    {
        DATAPRINTF ("Success in sendChunk size %d\n\r", pktlen);
        bytesRemaining -= pktlen;
        // Reset retry count on success
        sendchunkretrycount = 0;
        errtime = 0;
#ifdef PRINT_PACKET_TIMES
        uint64_t new_time_now;
        sl_sleeptimer_tick64_to_ms (sl_sleeptimer_get_tick_count64 (), &new_time_now);
        PRINT_INFO ("OK:%ld\n\r", (uint32_t)(new_time_now - time_now));
        time_now = new_time_now;
#endif
        // All data sent, trigger next sweep
        if (bytesRemaining == 0)
        {
#ifdef PRINT_PACKET_TIMES
            uint64_t new_time_now;
            sl_sleeptimer_tick64_to_ms (sl_sleeptimer_get_tick_count64 (), &new_time_now);
            PRINT_INFO ("PKT:%ld\n\r", (uint32_t)(new_time_now - time_now));
            time_now = new_time_now;
#endif
            PRINT_INFO ("A");
            ret = true;
        }
    }
    return ret;
}

// Will be called in app_process_action to send the next pending chunk
// and also acts as a retry mechanism
static void sendNextChunk (void)
{
    // To keep transferring pending packets via notifications
    if(notificationTransferInProgress() == true)
    {
        bool ret = sendChunk ();
        // Trigger next sweep as data was fully sent or failure
        // was retried enough times
        if ((ret == true) && (active_module == ACTIVE_MODULE_SWEEP))
            triggerSweep ();
    }
}

#define HEADER_SIZE (10) // 3 byte syncword, 1 byte packet type, 4 bytes timestamp, 2 byte byte count
// Sweep data is the largest out of Sweep/PPG/Accel so set array size based on it
static uint8_t resultArraySend[HEADER_SIZE + sizeof(RESULT_TYPE) * RESULT_SIZE * NUM_INPUTS];

// Call this when a new packet is ready to send e.g. when one sweep of data is
// collected, or PPG has collected one frame of measurements.
static bool handleNewPacket (app_module_t source, uint8_t *dataSource, uint16_t dataSourceLen)
{
    // True if packet was fully sent or connection not opened or previous data not fully sent
    // False if packet partially sent or send failure which needs retry i.e. send is ongoing
    // Note: The bool return value is overloaded and bytesRemaining is used to distinguish
    // some cases
    bool ret = true;
    if (!dataSource || (dataSourceLen > ((int)sizeof(resultArraySend) - HEADER_SIZE)))
    {
        PRINT_ERROR ("ERROR: handleNewPacket source address %p or source length %d\n\r", dataSource, dataSourceLen);
        bytesRemaining = 0;
    }
    // Send data if transfer is enabled
    else if ((transferEnabled == true) && (sendNotification == true))
    {
        if (bytesRemaining == 0)
        {
            // At the start of a sweep send a syncword, packet type and timestamp
            resultArraySend[0] = 0xAA;
            resultArraySend[1] = 0xBB;
            resultArraySend[2] = 0xCC;
            // Insert packet type
            resultArraySend[3] = __builtin_ctz(source);
            // Insert timestamp
            uint32_t nowtime = sl_sleeptimer_tick_to_ms (sl_sleeptimer_get_tick_count());
            for (size_t k = 0; k < sizeof(uint32_t); k++)
                resultArraySend[4 + k] = (nowtime >> (24 - k * 8)) & 0xFF;
            // Insert packet byte count (not including fixed header size)
            resultArraySend[8] = (dataSourceLen >> 8) & 0xFF;
            resultArraySend[9] = dataSourceLen & 0xFF;
            // As dataSource will come into use when adding a new packet, make a local copy for TX
            memcpy (resultArraySend + HEADER_SIZE, dataSource, dataSourceLen);
            sendData = resultArraySend;
            sendTotalDataLen = HEADER_SIZE + dataSourceLen;
            bytesRemaining = sendTotalDataLen;
            sendchunkretrycount = 0;
            errtime = 0;
            ret = sendChunk ();
        }
        else
        {
            // Should not form the new data in resultArraySend, instead wait till the previous
            // data is fully sent.
        }
    }
    else
    {
        // Can't send so reset
        bytesRemaining = 0;
    }
    return ret;
}

#endif

/**************************************************************************//**
 * Low Power Mode related
 *****************************************************************************/

static void shutoffRFICs (void)
{
    // Make sure the downconvertor is disabled
    resetCMX994 ();
    // Make sure the PLLs is disabled
    powerdownLMX2572LP ();
}

// Helper function to enable UART pins
static void enable_UART_pins (void)
{
#ifndef CLI_KEEP_ALIVE
    // Re-enable UART pins
    GPIO_PinModeSet(USART0_TX_PORT, USART0_TX_PIN, gpioModePushPull, 1);
    GPIO_PinModeSet(USART0_RX_PORT, USART0_RX_PIN, gpioModeInputPull, 1);
#endif
}

// Helper function to disable UART pins before sleep
static void disable_UART_pins (void)
{
#ifndef CLI_KEEP_ALIVE
    // Disable UART pins
    GPIO_PinModeSet(USART0_TX_PORT, USART0_TX_PIN, gpioModeDisabled, 0);
    GPIO_PinModeSet(USART0_RX_PORT, USART0_RX_PIN, gpioModeDisabled, 0);
#endif
}

// Helper function to enable all SPI & CS pins before the first SPI operations
static void enable_SPI_pins (void)
{
    // Re-enable SPI pins
    // Note that there is no SPI activity in interrupts, and the signal
    // event handlers run to completion, so even if multiple modes are active
    // at the same time, there is no SPI bus clash. At the moment, only a
    // single mode is kept active.
    // Note also that LIS2DH12, CMX994, LMX2572LP all use Big Endian mode
    GPIO_PinModeSet(USART1_CLK_PORT, USART1_CLK_PIN, gpioModePushPull, 0);
    GPIO_PinModeSet(USART1_RX_PORT, USART1_RX_PIN, gpioModeInput, 0);
    GPIO_PinModeSet(USART1_TX_PORT, USART1_TX_PIN, gpioModePushPull, 0);
    // Enable all CS pins
    GPIO_PinModeSet(CS_CMX994_PORT, CS_CMX994_PIN, gpioModePushPull, 1);
    GPIO_PinModeSet(CS_LO_PORT, CS_LO_PIN, gpioModePushPull, 1);
    GPIO_PinModeSet(CS_ACCEL_PORT, CS_ACCEL_PIN, gpioModePushPull, 1);
#ifdef USE_PPG
    GPIO_PinModeSet(CS_OPT_PORT, CS_OPT_PIN, gpioModePushPull, 1);
#endif
}

// Helper function to disable all SPI & CS pins before sleep
static void disable_SPI_pins (void)
{
    // Disable SPI pins
    GPIO_PinModeSet(USART1_CLK_PORT, USART1_CLK_PIN, gpioModeDisabled, 0);
    GPIO_PinModeSet(USART1_TX_PORT, USART1_TX_PIN, gpioModeDisabled, 0);
    GPIO_PinModeSet(USART1_RX_PORT, USART1_RX_PIN, gpioModeDisabled, 0);
    // Disable all CS pins
    GPIO_PinModeSet(CS_CMX994_PORT, CS_CMX994_PIN, gpioModeDisabled, 0);
    GPIO_PinModeSet(CS_LO_PORT, CS_LO_PIN, gpioModeDisabled, 0);
    GPIO_PinModeSet(CS_ACCEL_PORT, CS_ACCEL_PIN, gpioModeDisabled, 0);
#ifdef USE_PPG
    GPIO_PinModeSet(CS_OPT_PORT, CS_OPT_PIN, gpioModeDisabled, 0);
#endif
}

// Board_init / BOARD_uninit enable/disable all GPIO & Port pins
static void BOARD_pins_init(void)
{
    // Re-enable SPI & UART pins
    enable_UART_pins ();
    enable_SPI_pins ();
    // Enable MUXOUT input
    GPIO_PinModeSet(LO_PLL_MUX_PORT, LO_PLL_MUX_PIN, gpioModeInput, 0);
    // For timing measurement
    GPIO_PinModeSet(TP1_PORT, TP1_PIN, gpioModePushPull, 1);
    GPIO_PinModeSet(RAMP_DIR_PORT, RAMP_DIR_PIN, gpioModePushPull, 0);
}

static void BOARD_pins_uninit (void)
{
    // Disable SPI pins (UART will be done in sleep hook)
    disable_SPI_pins ();
    // Disable MUXOUT input
    GPIO_PinModeSet(LO_PLL_MUX_PORT, LO_PLL_MUX_PIN, gpioModeDisabled, 0);
    // For timing measurement
    GPIO_PinModeSet(TP1_PORT, TP1_PIN, gpioModeDisabled, 0);
    GPIO_PinModeSet(RAMP_DIR_PORT, RAMP_DIR_PIN, gpioModeDisabled, 0);
}

// Used in case of opening BLE connection when in sleep state when I2C pins are disabled
static void enableI2CLED (void)
{
    // Re-enable I2C pins
    GPIO_PinModeSet(I2C0_SCL_PORT, I2C0_SCL_PIN, gpioModeWiredAndPullUp, 1);
    GPIO_PinModeSet(I2C0_SDA_PORT, I2C0_SDA_PIN, gpioModeWiredAndPullUp, 1);
    // Set LED IC to operational mode and switch LEDs off
    setLedNormal ();
    setLed (LED_1, COLOUR_NONE);
}

// Used in case of closing BLE connection when in sleep state when I2C pins are disabled
// and normal low power mode
static void disableI2CLED (void)
{
    // Switch off LEDs
    setLed (LED_1, COLOUR_NONE);
    setLedShutdown ();
    // Disable I2C pins
    GPIO_PinModeSet(I2C0_SCL_PORT, I2C0_SCL_PIN, gpioModeDisabled, 0);
    GPIO_PinModeSet(I2C0_SDA_PORT, I2C0_SDA_PIN, gpioModeDisabled, 0);
}

/**************************************************************************************************
    EN_POWER/HFXO related utility functions to switch on/off
**************************************************************************************************/

// This is called from sl_platform_init before sl_clock_manager_init as HFXO will be initialised there
// and needs EN_POWER to be powered up.
// This is done so that all the drivers will be set based on PCLK which is based on SYSCLK = HFXO
// Setting drivers based on HFRCODPLL will need them to be re-initialised later each time in wake
void my_en_power_init (void)
{
    // Make sure GPIOs are enabled
    CMU_ClockEnable(cmuClock_GPIO, true);
    // Powerup PLLs, Downconvertor, Attenuator
    GPIO_PinModeSet(EN_POWER_PORT, EN_POWER_PIN, gpioModePushPull, 1);
}

void EMU_EFPEM23PresleepHook (void)
{
    // Disable pins before EM2
    disable_UART_pins ();
    // Remove power from TCXO and RF section
    GPIO_PinOutClear(EN_POWER_PORT, EN_POWER_PIN);
}

void EMU_EFPEM23PostsleepHook (void)
{
    // Enable power to TCXO and RF section
    GPIO_PinOutSet(EN_POWER_PORT, EN_POWER_PIN);
}

/**************************************************************************************************
    Low Power Mode enter/exit functions (pins, BLE connection, BLE advertising)
**************************************************************************************************/

// Called when sweep is started
static void unsetLowPowerMode (void)
{
    sleepState = SLEEP_STATE_WAKE;
    // Enable required pins
    BOARD_pins_init ();
#ifdef USE_BLE
    // Connection was already open
    if (OpenedConnectionHandle != SL_BT_INVALID_CONNECTION_HANDLE)
    {
        PRINT_INFO ("BLE connection already open!\n\r");
    }
#ifndef BLE_KEEP_ALIVE
    startAdvertising ();
#endif
#endif
    PRINT_INFO ("Exit LPM\n\r");
}

// Called when sweep is ended and when BLE connection is closed if not already in sleep
// Accel is not powered off in case it is needed for wakeup
static void setLowPowerMode (void)
{
    sweepState = SWEEP_STATE_INIT;
    sleepState = SLEEP_STATE_SLEEP;
    PRINT_INFO ("Enter LPM\n\r");
#ifdef USE_BLE
    // Close the BLE connection if open when sweep is done
    closeConnection ();
#endif
    // Disable PPG1/PPG2 and Shutdown MAX86171
    max86171_shutdown(&max86171_dev);
    // Shut off RF ICs
    shutoffRFICs ();
    // Disable IADC
    disableIADC ();
    // Disable used pins
    BOARD_pins_uninit ();
}

// Setup Accel and PPG interrupts
static void setup_interrupts (void)
{
    // GPIO input used as Accelerometer interrupt
    GPIO_PinModeSet(INT_ACCEL_PORT, INT_ACCEL_PIN, gpioModeInput, 0);
    // Setup the interrupt handlers
    GPIOINT_CallbackRegister(INT_ACCEL_PIN, (GPIOINT_IrqCallbackPtr_t)AccelInterrupt);
    // Setup the interrupt on falling edge
    GPIO_ExtIntConfig(INT_ACCEL_PORT, INT_ACCEL_PIN, INT_ACCEL_PIN, false, true, true);

#ifdef USE_PPG
    // GPIO input used as PPG interrupt
    GPIO_PinModeSet(INT_OPT_PORT, INT_OPT_PIN, gpioModeInput, 0);
    // Setup the interrupt handlers
    GPIOINT_CallbackRegister(INT_OPT_PIN, (GPIOINT_IrqCallbackPtr_t)PPGInterrupt);
    // Setup the interrupt on falling edge
    GPIO_ExtIntConfig(INT_OPT_PORT, INT_OPT_PIN, INT_OPT_PIN, false, true, true);
#endif
    GPIOINT_Init();
}

/**************************************************************************//**
 * Display LED related
 * Centralised here instead of spread over each module / BLE.
 * Delinked from low power mode functions.
 *****************************************************************************/

static uint8_t led_toggle;
static sl_sleeptimer_timer_handle_t timer_led;
#define LED_BLIP_TIME_MSEC (100)

// This timer will fire when LED needs to change
// The LED will be on for a shorter period of time to indicate which module
// (including BLE if connection is open) is active and off for the remainder of 1 second.
// The I2C is enabled/disable here and is kept partitioned from the modes as they
// do not need I2C for any other purpose.
// Note: this is called from a timer tick ISR context so needs to be compact
static void timer_cb_led (sl_sleeptimer_timer_handle_t *handle, void *data)
{
    (void)data;
    (void)handle;
    sl_sleeptimer_stop_timer (&timer_led);
    // Cycle through LED states: active module, off, BLE (if open), off, active module, ...
    if (led_toggle == 0)
    {
        // In case the timer fires when no module is active
        if (active_module)
            setLed (LED_1, moduleLedColours[__builtin_ctz(active_module)]);
        else
            setLed (LED_1, COLOUR_NONE);
        sl_sleeptimer_start_timer_ms (&timer_led, LED_BLIP_TIME_MSEC, timer_cb_led, NULL, 0, 0);
        led_toggle = 1;
    }
    else if (led_toggle == 1)
    {
        setLed (LED_1, COLOUR_NONE);
        sl_sleeptimer_start_timer_ms (&timer_led, 1000 - LED_BLIP_TIME_MSEC, timer_cb_led, NULL, 0, 0);
        led_toggle = 0;
#ifdef USE_BLE
        if (OpenedConnectionHandle != SL_BT_INVALID_CONNECTION_HANDLE)
            led_toggle = 2;
#endif
    }
    else if (led_toggle == 2)
    {
        setLed (LED_1, moduleLedColours[__builtin_ctz(ACTIVE_MODULE_BLE)]);
        sl_sleeptimer_start_timer_ms (&timer_led, LED_BLIP_TIME_MSEC, timer_cb_led, NULL, 0, 0);
        led_toggle = 3;
    }
    else if (led_toggle == 3)
    {
        setLed (LED_1, COLOUR_NONE);
        sl_sleeptimer_start_timer_ms (&timer_led, 1000 - LED_BLIP_TIME_MSEC, timer_cb_led, NULL, 0, 0);
        led_toggle = 0;
    }
}

// Start LED if not already started
static void startLedTimer (void)
{
    bool is_running = false;
    sl_sleeptimer_is_timer_running (&timer_led, &is_running);
    if (is_running == false)
    {
        // Enable display LEDs
        enableI2CLED ();
        // Start a timer to blip the module LED briefly
        sl_sleeptimer_stop_timer (&timer_led);
        sl_sleeptimer_start_timer_ms (&timer_led, LED_BLIP_TIME_MSEC, timer_cb_led, NULL, 0, 0);
        led_toggle = 0;
    }
    return;
}

// Stop LED if not needed
static void stopLedTimer (void)
{
#ifdef USE_BLE
    if ((OpenedConnectionHandle == SL_BT_INVALID_CONNECTION_HANDLE) && (active_module == 0))
#else
    if (active_module == 0)
#endif
    {
        // Stop the timer
        sl_sleeptimer_stop_timer (&timer_led);
        // Disable display LEDs
        disableI2CLED ();
    }
    return;
}

/**************************************************************************//**
 * Common function to stop an active module
 *****************************************************************************/

static void stop_module (void)
{
    active_module = 0;
    // Stop LED timer for Accel status
    stopLedTimer ();
    // Go to sleep
    setLowPowerMode ();
}

#ifdef USE_PPG

/**************************************************************************//**
 * PPG functions
 *****************************************************************************/

static bool ppg_init_done, ppg_agc_on;
static uint8_t fifo_buf[MAX_FIFO_SAMPLES * 3];
static uint16_t samplecount;
static bool tag1found;
static uint8_t predicted_tag;
static ppg_agc_t g_agc;
static void dostopppg (void);

// Called when PPG is started
static void start_ppg (void)
{
    int ret;
    // Soft reset
    ret = max86171_soft_reset(&max86171_dev);
    if (ret)
    {
        PRINT_ERROR ("ERROR: max86171_soft_reset failed at startup\n\r");
        dostopppg ();
        return;
    }
    // Write the base config registers
    // Write the registers for each MEASn enabled
    // Write SYS_CFG2 and SYS_CFG1
    ret = max86171_load_config_from_table(&max86171_dev, MAX86171_MEAS_ALL);
    if (ret)
    {
        PRINT_ERROR ("ERROR: max86171_load_config_from_table failed at startup\n\r");
        dostopppg ();
        return;
    }
    // Clear the FIFO to start afresh
    ret = max86171_fifo_flush(&max86171_dev);
    if (ret)
    {
        PRINT_ERROR ("ERROR: max86171_fifo_flush failed at startup\n\r");
        dostopppg ();
        return;
    }
    // tag is unknown to begin with
    tag1found = false;
    predicted_tag = 0;

    // Verify MAX86171 power ready before proceeding
    // TODO: Due to the retries this could take almost 0.5 sec which can
    // bring down BLE connection as this is called from processblesetting.
    int count = 10;
    while (1)
    {
        // Read and clear all status bits
        ret = max86171_status_read_clear(&max86171_dev);
        if (ret)
        {
            count--;
            if (count == 0)
            {
                PRINT_ERROR ("ERROR: max86171 PWR_RDY failed at startup, stop PPG\n\r");
                max86171_verify_config(&max86171_dev, MAX86171_MEAS_ALL, NULL, 0, NULL);
                dostopppg ();
                return;
            }
            else
            {
                PRINT_ERROR ("ERROR: max86171 PWR_RDY failed at startup, wait and retry\n\r");
                sl_sleeptimer_delay_millisecond (50);
            }
        }
        else
        {
            PRINT_INFO ("max86171 PWR_RDY is good\n\r");
            break;
        }
    }
    ppg_agc_init(&g_agc);
    ppg_init_done = true;
}

// Called when PPG is to be stopped
static void stop_ppg (void)
{
    // Disable PPG1/PPG2 and Shutdown MAX86171
    max86171_shutdown(&max86171_dev);
    ppg_init_done = false;
    stop_module ();
}

// To be called when PPG interrupt is triggered periodically
// As we have kept the FIFO interrupt level as MAX_FIFO_SAMPLES
// which is 180 i.e. 10 frames, this interrupt should happen
// every 100 msec.
static bool read_ppg_fifo (void)
{
    int ret1, ret2;

    ret1 = max86171_status_read_clear(&max86171_dev);
    if (ret1)
    {
        // TODO: For now just return. If there was a read error the status
        // will not clear and this could lead to an interrupt blockage.
        return false;
    }

    uint8_t cnt1, cnt2;
    ret1 = max86171_read_reg(&max86171_dev, MAX86171_REG_FIFO_COUNTER1, &cnt1, 1);
    ret2 = max86171_read_reg(&max86171_dev, MAX86171_REG_FIFO_COUNTER2, &cnt2, 1);
    if (ret1 || ret2)
    {
        PRINT_ERROR ("ERROR: max86171 counter: 0x%02x, 2: 0x%02x\n\r", ret1, ret2);
        return false;
    }
    // Check for FIFO overflow
    if (cnt1 & 0x7F)
    {
        PRINT_DEBUG ("max86171 FIFO overflow count: 0x%02x\n\r", cnt1 & 0x7F);
        // Clear the FIFO to clear overflow
        int ret = max86171_fifo_flush(&max86171_dev);
        if (ret)
        {
            PRINT_ERROR ("ERROR: max86171 FIFO flush failed after overflow\n\r");
        }
        // Last tag is unknown to begin with
        tag1found = false;
        predicted_tag = 0;
        return false;
    }

    // Append MSB of upper byte to the lower byte
    // Bugfix: the formation of the samplecount was corrected here
    // max86171_fifo_read_samples had the correct method
    // entry was created after interrupt was triggered
    samplecount = (uint16_t)((cnt1 & 0x80u) << 1) | cnt2;
    PRINT_IGNORE ("samplecount = %d\n\r", samplecount);

    // Safety clamp to max size of the FIFO as per A_FULL interrupt size
    if (samplecount > MAX_FIFO_SAMPLES) samplecount = MAX_FIFO_SAMPLES;
    // Ensure an even number of samples (PPG1 + PPG2 pair)
    samplecount = (samplecount / 2) * 2;
    // Call with the samplecount * 3 bytes per sample
    // Bugfix: +2 for the SPI command + type is added inside so it's not done here
    // A FIFO entry is one sample (PPG1 or PPG2)
    ret1 = max86171_read_FIFO(&max86171_dev, fifo_buf, (size_t)samplecount * 3u);
    if (ret1)
    {
        PRINT_ERROR ("ERROR: max86171 FIFO read: %0x\n\r", ret1);
        return false;
    }
    return true;
}

// PPG will have 3 bytes per measurement, 8 measurements, 10 frames
// collected at a time, which we can send as a BLE packet of 240 Bytes.
// As frame rate is set as 100 Hz, the required BLE transfer speed
// is 24 Bytes every 10 msec i.e. 2400 Bytes per second or 19.2 kbps
static uint8_t resultArrayPPG[(MAX86171_NUM_MEAS - 1) * 3 * MAX86171_NUM_FRAMES];

static uint8_t PPG_Average_Type[MAX86171_NUM_MEAS - 1];

// To be called after PPG FIFO data has been read
static void parse_ppg_fifo (void)
{
    /* ================================================================
     * Step 7 — inside while(1), after the MAX86171_INT_f block
     * Parse FIFO.
     * Frame layout:  tag 1,1, 2,2, 3,3, 4,4, 5,5, 6,6, 7,7, 8,8, 9,9
     *                (PD1 then PD2 for each MEAS)
     * We average PD1 & PD2 of MEAS1-8, discard MEAS9 (Ambient).
     * ================================================================ */
    /* ── Step 7: parse FIFO → BLE buffer + AGC ─────────────────────── */
    /* Each pair = PD1(3 bytes) + PD2(3 bytes) = 6 bytes = 2 FIFO samples */
    uint16_t num_pairs = samplecount / 2u;
    int resultIndex = 0;
    for (uint16_t i = 0; i < num_pairs; i++)
    {
        uint16_t base = i * 6u;

        uint8_t tag  =  fifo_buf[base] >> 4;
        uint8_t p1b0 =  fifo_buf[base] & 0x0Fu;
        uint8_t p1b1 =  fifo_buf[base + 1];
        uint8_t p1b2 =  fifo_buf[base + 2];

        /* --- PD2 (next 3 bytes, same tag expected) --- */
        uint8_t tag2 =  fifo_buf[base + 3] >> 4;
        uint8_t p2b0 =  fifo_buf[base + 3] & 0x0Fu;
        uint8_t p2b1 =  fifo_buf[base + 4];
        uint8_t p2b2 =  fifo_buf[base + 5];

        // Handle complex PPG tag errors e.g. not a full frame, tags out of sequence:
        // Doesn't seem to be required. We can just handle the tags as they come and not
        // expect any sequencing. The AGC anyway acts on different measurements independently.

        // Keep updating the predicted tag as we get samples
        if (tag1found == true)
        {
            predicted_tag++;
            if (predicted_tag == (MAX86171_NUM_MEAS + 1)) predicted_tag = 1;
        }
        bool ovf_error = false;
        // Handle basic PPG tag errors
        if (tag < 1u || tag2 != tag || tag > MAX86171_NUM_MEAS)
        {
            PRINT_IGNORE ("ERROR: in sample %d, tag1 %d, tag2 %d\n\r", i, tag, tag2);
            // These errors indicate the current is too high, so reduce sample target range
            if (((tag == MAX86171_TAG_ALC_OVF) || (tag == MAX86171_TAG_EXP_OVF) || (tag2 == MAX86171_TAG_ALC_OVF) || (tag2 == MAX86171_TAG_EXP_OVF)) && predicted_tag)
            {
                tag = predicted_tag;
                PRINT_IGNORE ("OVF ERROR in predicted tag %d\n\r", tag);
                ovf_error = true;
            }
            else
            {
                continue;
            }
        }
        // MAX86171 datasheet says to ignore samples till tag = 1 after a restart.
        else if ((tag1found == false) && (tag != 1))
        {
            continue;
        }
        else if ((tag1found == false) && (tag == 1))
        {
            // tag = 1 has been found so no need to check for it in the future
            tag1found = true;
            // This is to help find the measurement number in case the next one had OVF errors in tag
            predicted_tag = tag;
        }

        // If we have a good tag, check if we're in sync with predicted tag
        if (predicted_tag != tag)
        {
            // Not in sync so wait for tag = 1 again
            tag1found = false;
            predicted_tag = 0;
            continue;
        }

        // Skip the last (ambient) measurement
        if (tag == MAX86171_NUM_MEAS)
        {
            if (samples_print_enabled == true)
            {
                PRINT_INFO ("\n\r");
            }
            continue;
        }

        /* Bugfix: average the two 20-bit values after sign-extension */
        uint32_t pd1 = ((uint32_t)p1b0 << 16) | ((uint32_t)p1b1 << 8) | p1b2;
        uint32_t pd2 = ((uint32_t)p2b0 << 16) | ((uint32_t)p2b1 << 8) | p2b2;
        int32_t avg, temp = (max86171_sext20 (pd1) + max86171_sext20 (pd2)) / 2;
        if (PPG_Average_Type[tag - 1] == 1)
            avg = max86171_sext20 (pd1);
        else if (PPG_Average_Type[tag - 1] == 2)
            avg = max86171_sext20 (pd2);
        else
            avg = temp;
        if (samples_print_enabled == true)
        {
            PRINT_INFO ("%ld,", avg);
        }

        if (ppg_agc_on == true)
        {
            // Feed samples to the AGC algorithm for each PPG1/PPG2 pair
            ppg_agc_feed(&g_agc, &max86171_dev, tag - 1u, avg, ovf_error);
        }

        // Shouldn't happen due to limiting of samplecount, but add anyway
        // for local safety.
        if (resultIndex + 3 > (int)sizeof(resultArrayPPG)) break;
        // Add back the tag before sending over BLE
        uint32_t sample_for_ble = (tag << 20) | (avg & ((1L << 20) - 1));
        resultArrayPPG[resultIndex++] = (sample_for_ble >> 16) & 0xFF;
        resultArrayPPG[resultIndex++] = (sample_for_ble >> 8) & 0xFF;
        resultArrayPPG[resultIndex++] = (sample_for_ble >> 0) & 0xFF;
    }
#ifdef USE_BLE
    handleNewPacket (ACTIVE_MODULE_PPG, (uint8_t *)resultArrayPPG, resultIndex);
#endif
}

#endif

/**************************************************************************//**
 * Accel functions
 *****************************************************************************/

#define NUM_BYTES_PER_ACCEL_SAMPLE (6)
#ifdef USE_25Hz
#define NUM_ACCEL_SAMPLES_PER_SEC (25)
#elif defined(USE_10Hz)
#define NUM_ACCEL_SAMPLES_PER_SEC (10)
#elif defined(USE_1Hz)
#define NUM_ACCEL_SAMPLES_PER_SEC (1)
#endif

// Have a little space for extra samples in case of timing jitter
#define NUM_ACCEL_SAMPLES_TO_READ (NUM_ACCEL_SAMPLES_PER_SEC + 2)
static uint8_t resultArrayAccel[NUM_BYTES_PER_ACCEL_SAMPLE * NUM_ACCEL_SAMPLES_TO_READ];

// Read Accel samples from FIFO
static int read_accel_fifo (void)
{
    // Get number of available Accel X+Y+Z samples
    int num = accelGetNumData ();
    // Limit to available storage
    if (num > NUM_ACCEL_SAMPLES_TO_READ) num = NUM_ACCEL_SAMPLES_TO_READ;
    // Read Accel FIFO
    uint8_t *ptr = resultArrayAccel;
    for (int k = 0; k < num; k++)
    {
        // Get one sample of Accel data (X+Y+Z) into BLE buffer
        accelGetData (ptr);
#define MAKE_INT16(x,y) ((int16_t)(((uint16_t)y << 8) | x))
        if (samples_print_enabled == true)
        {
            PRINT_INFO ("%d,%d,%d\n\r", MAKE_INT16(*ptr, *(ptr + 1)), MAKE_INT16(*(ptr + 2), *(ptr + 3)), MAKE_INT16(*(ptr + 4), *(ptr + 5)));
        }
        ptr += NUM_BYTES_PER_ACCEL_SAMPLE;
    }
    return num;
}

/**************************************************************************//**
 * Sweep related
 *****************************************************************************/

#ifdef PRECISION_20BIT
/*
 * pack_into_20 - pack an array of int32_t values that each hold a
 * right-justified 20-bit value (bits 19:0, upper 12 bits assumed 0)
 * down into the same array, with no gaps, 20 bits per element.
 *
 * number_of_data_words MUST be a multiple of 32.
 *
 * Why 32 at a time: 32 elements * 20 bits = 640 bits = 20 words * 32 bits,
 * exactly. So the array packs down into fixed, independent blocks of
 * 32 input words -> 20 output words, with no leftover/carry bits between
 * blocks. That lets the whole thing be fully unrolled per block (no
 * per-bit shifting loop, no branches) and done safely in place with a
 * simple two-pointer walk: output offset (20*block) is always <= input
 * offset (32*block), and every input word is read before its memory
 * could ever be overwritten by that same block's output (proven by
 * construction: C evaluates the right-hand side of an assignment, i.e.
 * reads, before performing the store for that statement, and statements
 * execute in increasing output-address order, so a later store can never
 * clobber a value an earlier statement still needed).
 *
 * No sign extension is applied on the way in/out; if your 20-bit values
 * are meant to be treated as signed on unpack, sign-extend after
 * unpacking (not needed for packing itself).
 */

/* Packs 32 input words (only the low 20 bits of each are used) into
 * 20 output words. in and out may overlap PROVIDED out <= in (i.e.
 * out points at the same address or earlier than in), which is the
 * case for every block when called from pack_into_20 below. */
static inline void pack_block_32to20(const uint32_t *in, uint32_t *out)
{
    out[0]  = ((in[0]  >>  0) & 0xFFFFFu) <<  0 | ((in[1]  >>  0) & 0xFFFu)   << 20;
    out[1]  = ((in[1]  >> 12) & 0xFFu)    <<  0 | ((in[2]  >>  0) & 0xFFFFFu) <<  8 | ((in[3]  >> 0) & 0xFu) << 28;
    out[2]  = ((in[3]  >>  4) & 0xFFFFu)  <<  0 | ((in[4]  >>  0) & 0xFFFFu)  << 16;
    out[3]  = ((in[4]  >> 16) & 0xFu)     <<  0 | ((in[5]  >>  0) & 0xFFFFFu) <<  4 | ((in[6]  >> 0) & 0xFFu) << 24;
    out[4]  = ((in[6]  >>  8) & 0xFFFu)   <<  0 | ((in[7]  >>  0) & 0xFFFFFu) << 12;
    out[5]  = ((in[8]  >>  0) & 0xFFFFFu) <<  0 | ((in[9]  >>  0) & 0xFFFu)   << 20;
    out[6]  = ((in[9]  >> 12) & 0xFFu)    <<  0 | ((in[10] >>  0) & 0xFFFFFu) <<  8 | ((in[11] >> 0) & 0xFu) << 28;
    out[7]  = ((in[11] >>  4) & 0xFFFFu)  <<  0 | ((in[12] >>  0) & 0xFFFFu)  << 16;
    out[8]  = ((in[12] >> 16) & 0xFu)     <<  0 | ((in[13] >>  0) & 0xFFFFFu) <<  4 | ((in[14] >> 0) & 0xFFu) << 24;
    out[9]  = ((in[14] >>  8) & 0xFFFu)   <<  0 | ((in[15] >>  0) & 0xFFFFFu) << 12;
    out[10] = ((in[16] >>  0) & 0xFFFFFu) <<  0 | ((in[17] >>  0) & 0xFFFu)   << 20;
    out[11] = ((in[17] >> 12) & 0xFFu)    <<  0 | ((in[18] >>  0) & 0xFFFFFu) <<  8 | ((in[19] >> 0) & 0xFu) << 28;
    out[12] = ((in[19] >>  4) & 0xFFFFu)  <<  0 | ((in[20] >>  0) & 0xFFFFu)  << 16;
    out[13] = ((in[20] >> 16) & 0xFu)     <<  0 | ((in[21] >>  0) & 0xFFFFFu) <<  4 | ((in[22] >> 0) & 0xFFu) << 24;
    out[14] = ((in[22] >>  8) & 0xFFFu)   <<  0 | ((in[23] >>  0) & 0xFFFFFu) << 12;
    out[15] = ((in[24] >>  0) & 0xFFFFFu) <<  0 | ((in[25] >>  0) & 0xFFFu)   << 20;
    out[16] = ((in[25] >> 12) & 0xFFu)    <<  0 | ((in[26] >>  0) & 0xFFFFFu) <<  8 | ((in[27] >> 0) & 0xFu) << 28;
    out[17] = ((in[27] >>  4) & 0xFFFFu)  <<  0 | ((in[28] >>  0) & 0xFFFFu)  << 16;
    out[18] = ((in[28] >> 16) & 0xFu)     <<  0 | ((in[29] >>  0) & 0xFFFFFu) <<  4 | ((in[30] >> 0) & 0xFFu) << 24;
    out[19] = ((in[30] >>  8) & 0xFFFu)   <<  0 | ((in[31] >>  0) & 0xFFFFFu) << 12;
}

/*
 * array           : in/out. On entry, number_of_data_words elements,
 *                   each holding a right-justified 20-bit value.
 *                   On return, the packed bitstream occupies the first
 *                   (number_of_data_words * 20) / 32 elements; the rest
 *                   of the array is untouched (stale) - ignore it.
 * number_of_data_words : MUST be a multiple of 32.
 */
#if (NUM_SAMPLES_PER_FREQ != 32)
#error NUM_SAMPLES_PER_FREQ should be 32 for this code to work
#endif
#if (IQPRECISION != 20)
#error IQPRECISION should be 20 for this code to work
#endif
void pack_into_20(int32_t *array, int number_of_data_words)
{
    uint32_t *a = (uint32_t *)array;
    int nblocks = number_of_data_words / NUM_SAMPLES_PER_FREQ;

    for (int b = 0; b < nblocks; b++) {
        pack_block_32to20(a + (size_t)b * NUM_SAMPLES_PER_FREQ, a + (size_t)b * IQPRECISION);
    }
}

#endif

static uint32_t last_set_frequency;

int i_avg_mV[NUM_FREQS], q_avg_mV[NUM_FREQS];
int i_avg_sum[NUM_FREQS][NUM_CAL_SWEEPS], q_avg_sum[NUM_FREQS][NUM_CAL_SWEEPS];

void clearIQOffsets (void)
{
    memset (i_avg_mV, 0, sizeof (i_avg_mV));
    memset (q_avg_mV, 0, sizeof (q_avg_mV));
}

static void run_sweep_statemachine (void)
{
    int sweep_data_size = 0;
    switch (sweepState)
    {
        case SWEEP_STATE_INIT:
        {
        // To keep track of when the sweeps started, to timeout after 30seconds
        starttime = sl_sleeptimer_tick_to_ms (sl_sleeptimer_get_tick_count());
        blestarttime = starttime;
        sweepcount = 0;
        // Needed here for when sweep wakes up from sleep, or BLE / CLI command
        active_module = ACTIVE_MODULE_SWEEP;
        // Start LED timer to display Sweep status
        startLedTimer ();
        // Normal sweep
        if (frequency_to_set_khz == 0)
        {
            if (active_time_sec)
            {
                PRINT_INFO ("Sweep for %d sec ... press any key to break\n\r", active_time_sec);
            }
            else
            {
                PRINT_INFO ("Sweep forever ... press any key to break\n\r");
            }
            sweepState = SWEEP_STATE_SWEEP;
        }
        // Set specific frequency
        else
        {
            PRINT_INFO ("Set frequency %ld MHz ... send stop_sweep to break\n\r", frequency_to_set_khz / 1000);
            PRINT_INFO ("Use > key to move forward in frequency and < key to move backward\n\r");
            sweepState = SWEEP_STATE_SETFREQ;
            last_set_frequency = 0;
        }
        // Enable downconvertor and IADC
        resetCMX994 ();
        enableCMX994 ();
        initIADC();
        // Continue with sweep
        if (sl_bt_external_signal (SIGNAL_SWEEP) != SL_STATUS_OK)
        {
            PRINT_ERROR ("ERROR: Could not trigger sl_bt_external_signal INIT\n\r");
        }
#ifdef STORE_SWEEPS
        initStoreSweep ();
#endif
        if (run_cal_offsets == true)
        {
            // Clear before running cal sweeps
            clearIQOffsets ();
        }
        }
        break;

        case SWEEP_STATE_SWEEP:
        {
        uint32_t endtime = sl_sleeptimer_tick_to_ms (sl_sleeptimer_get_tick_count());
        // Cater for infinite sweep time
        uint32_t temp = active_time_sec * 1000;
        if (active_time_sec == 0) temp = INT32_MAX;
        if ((endtime - starttime) < temp)
        {
            doOneSweep (sweepcount);
            sweepcount++;
            if ((userinput == true) || (active_module == 0) || ((run_cal_offsets == true) && (sweepcount == NUM_CAL_SWEEPS)))
            {
                PRINT_INFO ("Break from sweep\n\r");
                sweepState = SWEEP_STATE_END;
                // Finish up sweep
                triggerSweep ();
                break;
            }
#ifdef USE_BLE
            sweep_data_size = sizeof(resultArray);
#ifdef PRECISION_20BIT
            // Add logic here to compress the 20-bit precision samples
            int number_of_data_words = sweep_data_size / sizeof(RESULT_TYPE);
            pack_into_20((RESULT_TYPE *)resultArray, number_of_data_words);
            // Update size of BLE data after packing into bytes
            sweep_data_size = (number_of_data_words * IQPRECISION) / 8;
#endif
            bool ret = handleNewPacket (ACTIVE_MODULE_SWEEP, (uint8_t *)resultArray, sweep_data_size);
            // Trigger next sweep as data was fully sent in a single packet, or connection is not open
            if (ret == true && bytesRemaining == 0)
            {
                // Continue sweep
                triggerSweep ();
            }
            // Previous bytes not sent yet so wait in a new state
            else if (ret == true && bytesRemaining != 0)
            {
                sweepState = SWEEP_STATE_PENDING;
                // Trigger to check if all bytes sent
                triggerSweep ();
            }
            else
            {
                // Do nothing as the sweep will be triggered once the last chunk is sent
            }
#else
            // Continue sweep
            triggerSweep ();
#endif
        }
        else
        {
            sweepState = SWEEP_STATE_END;
            // Finish up sweep
            triggerSweep ();
        }
        }
        break;

        // This state is transitory and so the sweep time and user
        // input checks to break from active sweep are not added here.
        case SWEEP_STATE_PENDING:
        {
            bool ret = handleNewPacket (ACTIVE_MODULE_SWEEP, (uint8_t *)resultArray, sweep_data_size);
            // Trigger next sweep as data was fully sent in a single packet, or connection is not open
            if (ret == true && bytesRemaining == 0)
            {
                sweepState = SWEEP_STATE_SWEEP;
                triggerSweep ();
            }
            // Previous bytes not sent yet so wait in this state
            else if (ret == true && bytesRemaining != 0)
            {
                // Check after a while if still pending
                triggerSweep ();
            }
            else
            {
                // Do nothing as the sweep will be triggered once the last chunk is sent
            }
        }
        break;

        case SWEEP_STATE_SETFREQ:
        {
        uint32_t endtime = sl_sleeptimer_tick_to_ms (sl_sleeptimer_get_tick_count());
        // Cater for infinite sweep time
        uint32_t temp = active_time_sec * 1000;
        if (active_time_sec == 0) temp = INT32_MAX;
        if ((endtime - starttime) < temp)
        {
            if (last_set_frequency != frequency_to_set_khz)
                doSetFreq ();
            last_set_frequency = frequency_to_set_khz;
            int c = 0;
            int len = sl_cli_handles[0]->input_len;
            int pos = sl_cli_handles[0]->input_pos;
            if (len)
            {
                c = sl_cli_handles[0]->input_buffer[pos - 1];
                // Only erase these keys so that normal commands are not ignored
                if ((c == '<') || (c == '>'))
                {
                    sl_cli_handles[0]->input_buffer[pos - 1] = '\0';
                    sl_cli_handles[0]->input_len--;
                    sl_cli_handles[0]->input_pos--;
                    userinput = true;
                }
            }
            if ((userinput == true) && (c == '>'))
            {
                frequency_to_set_khz += FREQ_STEP_KHZ;
                // Wrap around frequency
                if (frequency_to_set_khz == END_FREQ_KHZ)
                    frequency_to_set_khz = START_FREQ_KHZ;
                last_set_frequency = 0;
            }
            else if ((userinput == true) && (c == '<'))
            {
                frequency_to_set_khz -= FREQ_STEP_KHZ; 
                // Wrap around frequency
                if (frequency_to_set_khz < START_FREQ_KHZ)
                    frequency_to_set_khz = END_FREQ_KHZ - FREQ_STEP_KHZ;
                last_set_frequency = 0;
            }
            if (active_module == 0)
            {
                // Finish up sweep
                sweepState = SWEEP_STATE_END;
                // Set this only when commanded to stop and not when going to sleep
                frequency_to_set_khz = 0;
                last_set_frequency = 0;
            }
            // Continue
            triggerSweep ();
        }
        else
        {
            sweepState = SWEEP_STATE_END;
            last_set_frequency = 0;
            // frequency_to_set_khz is not reset here so that when it wakes it will continue at the last set frequency
            // Finish up sweep
            triggerSweep ();
        }
        }
        break;

        case SWEEP_STATE_END:
        {
        uint32_t bleendtime = sl_sleeptimer_tick_to_ms (sl_sleeptimer_get_tick_count());
        int bletime = (bleendtime - blestarttime) / 1000;
        (void)bletime;
        PRINT_INFO ("Finished %d sweeps in %d sec with BLE on\n\r", sweepcount, bletime);
        sl_sleeptimer_stop_timer (&timer_startsweep);
#ifdef USE_WAKEUP
        // Check if Sweep should continue
        if (active_module == ACTIVE_MODULE_SWEEP)
        {
            // Start timer to sleep till next sweep
            sl_sleeptimer_start_timer_ms (&timer_startsweep, sweep_interval_sec * 1000L, timer_cb_startsweep, NULL, 0, 0);
            PRINT_INFO ("Sleep and set wakeup after %d sec\n\r", sweep_interval_sec);
        }
        else
#endif
        {
            PRINT_INFO ("Sweep has been stopped, so sleep\n\r");
        }
        if (run_cal_offsets == true)
        {
            // Calculate clusters after running all cal sweeps
            for (int i = 0; i < NUM_FREQS; i++)
            {
                int icent0, icent1, qcent0, qcent1;
                two_means_2D(&i_avg_sum[i][0], &q_avg_sum[i][0], NUM_CAL_SWEEPS, &icent0, &qcent0, &icent1, &qcent1);
                PRINT_INFO ("Freq index %d, <IQ1:%d, %d>, <IQ2:%d, %d>\n\r", i, icent0, qcent0, icent1, qcent1);
            }
            run_cal_offsets = false;
        }
        stop_module ();
        }
        break;

        default:
        break;
    }
}

/**************************************************************************//**
 * Common functions for setting parameters from CLI / BLE
 *****************************************************************************/

static void dosettrigfreq (uint8_t temp)
{
    // 0 for all frequencies, 1 to NUM_FREQS for individual selection
    if (temp <= NUM_FREQS)
    {
        PRINT_INFO ("Set TP1 trigger frequency to %d\n\r", temp);
        trigfreq = temp;
    }
    else
    {
        PRINT_ERROR ("ERROR: invalid argument to set_trigfreq: %d\n\r", temp);
    }
}

static void dosetiqoffsets (int8_t temp1, int8_t temp2)
{
    if ((abs(temp1) <= CMX994_MAX_OFFSET) && (abs(temp2) <= CMX994_MAX_OFFSET))
    {
        PRINT_INFO ("Set Ioffset = %d, Qoffset = %d\n\r", temp1, temp2);
        PRINT_INFO ("Step size = %.2f mV\n\r", 25 * pow (10.0, (-6.0 / 20) * vgagain));
        setCMX994_Offset (temp1, temp2);
    }
    else
    {
        PRINT_ERROR ("ERROR: invalid arguments to set_iq_offsets: %d, %d\n\r", temp1, temp2);
    }
}

static void dosetlnavgagain (uint8_t temp1, uint8_t temp2)
{
    if ((temp1 <= CMX994_MAX_LNA_GAIN) && (temp2 <= CMX994_MAX_VGA_GAIN))
    {
        lnagain = temp1;
        vgagain = temp2;
        PRINT_INFO ("Set LNA gain = %d dB, VGA gain = %d dB\n\r", -6 * (int)temp1, -6 * (int)temp2);
        PRINT_INFO ("IQ offsets step size = %.2f mV\n\r", 25 * pow (10.0, (-6.0 / 20) * vgagain));
        setCMX994_LNA_VGA_Gain (lnagain, vgagain);
    }
    else
    {
        PRINT_ERROR ("ERROR: invalid arguments to set_lna_vga_gain: %d, %d\n\r", temp1, temp2);
    }
}

static void dosetsweeptiming (int temp1, int temp2)
{
    if (((temp1 == 0) || ((temp1 >= 1) && (temp1 <= 6000))) && (temp2 >= 10) && (temp2 <= 6000))
    {
        if (temp1)
        {
            PRINT_INFO ("Set sweep wake time to %d sec, sleep time to %d sec\n\r", temp1, temp2);
        }
        else
        {
            PRINT_INFO ("Set sweep wake time to always awake, sleep time to %d sec\n\r", temp2);
        }
        active_time_sec = temp1;
        sweep_interval_sec = temp2;
        // If already in sleep, then update timer to sleep till next sweep
        // Else, the old setting will still be active and device will not wake as per new setting
        if (sleepState == SLEEP_STATE_SLEEP)
        {
            if (temp1)
            {
#ifdef USE_WAKEUP
                sl_sleeptimer_restart_timer_ms (&timer_startsweep, sweep_interval_sec * 1000L, timer_cb_startsweep, NULL, 0, 0);
                PRINT_INFO ("Update to wakeup after %d sec\n\r", sweep_interval_sec);
#endif
            }
            else
            {
                sl_sleeptimer_restart_timer_ms (&timer_startsweep, SWEEP_START_DELAY_MSEC, timer_cb_startsweep, NULL, 0, 0);
                PRINT_INFO ("Wakeup and stay always awake\n\r");
            }
        }
    }
    else
    {
        PRINT_ERROR ("ERROR: invalid arguments to set_sweep_timing: %d, %d\n\r", temp1, temp2);
    }
}

static void dosweep (void)
{
    if (active_module)
    {
        PRINT_ERROR ("Module %s already active\n\r", getModuleName(active_module));
        return;
    }
    sweepState = SWEEP_STATE_INIT;
    // Start timer to run sweeps after a delay
    sl_sleeptimer_stop_timer (&timer_startsweep);
    sl_sleeptimer_start_timer_ms (&timer_startsweep, SWEEP_START_DELAY_MSEC, timer_cb_startsweep, NULL, 0, 0);
}

static void dosetfreq (uint16_t temp)
{
    // Check range and multiple of step frequency
    if ((temp < START_FREQ_KHZ / 1000) || (temp >= END_FREQ_KHZ / 1000) || ((temp % (FREQ_STEP_KHZ / 1000)) != 0))
    {
        PRINT_ERROR("set_freq: incorrect argument %d\n\r", temp);
    }
    else
    {
        frequency_to_set_khz = temp * 1000;
        dosweep ();
    }
}

// Actual stopping will be done inside the Sweep state machine if sweeping.
// If in sleep state, it is enough to stop the wake timer.
static void dostopsweep (void)
{
    bool is_running = false;
    sl_sleeptimer_is_timer_running (&timer_startsweep, &is_running);
    if ((active_module != ACTIVE_MODULE_SWEEP) && (is_running == false))
    {
        PRINT_ERROR ("Module %s not active\n\r", getModuleName(ACTIVE_MODULE_SWEEP));
        return;
    }
    PRINT_INFO ("stop_sweep\n\r");
#ifdef USE_WAKEUP
    // In case sweep was active and in sleep
    if ((is_running == true) && (sleepState == SLEEP_STATE_SLEEP))
    {
        // Stop timer to wake and do sweep
        sl_sleeptimer_stop_timer (&timer_startsweep);
    }
#endif
    // TODO: How did active_module = 0; work in v25
    sweepState = SWEEP_STATE_END;
}

#ifdef USE_PPG

static void dostartppg (void)
{
    if (active_module)
    {
        PRINT_ERROR ("Module %s already active\n\r", getModuleName(active_module));
        return;
    }
    PRINT_INFO ("start_ppg\n\r");
    active_module = ACTIVE_MODULE_PPG;
    // Start LED timer to display PPG status
    startLedTimer ();
    // Initialise MAX86171 and set for sample collection
    start_ppg ();
}

static void dostopppg (void)
{
    if (active_module != ACTIVE_MODULE_PPG)
    {
        PRINT_ERROR ("Module %s not active\n\r", getModuleName(ACTIVE_MODULE_PPG));
        return;
    }
    PRINT_INFO ("stop_ppg\n\r");
    // Put MAX86171 into shutdown and disable PPGs
    stop_ppg ();
}

static void dosetppgagc (uint8_t agconoff)
{
    if ((agconoff != 1) && (agconoff != 0))
    {
        PRINT_ERROR("set_ppg_agc: incorrect argument %d\n\r", agconoff);
    }
    else if (active_module == ACTIVE_MODULE_PPG)
    {
        PRINT_ERROR("set_ppg_agc: incorrect state, PPG is already on, first switch off\n\r");
    }
    else
    {
        ppg_agc_on = agconoff;
        // Set all channels to a fixed current which will not be adjusted by AGC
        if (ppg_agc_on == false)
            set_all_channels_restart_pa (&g_agc, &max86171_dev);
    }
}

#endif

static void dostartaccel (void)
{
    if (active_module)
    {
        PRINT_ERROR ("Module %s already active\n\r", getModuleName(active_module));
        return;
    }
    PRINT_INFO ("start_accel\n\r");
    active_module = ACTIVE_MODULE_ACCEL;
    // Start timer to read Accel after 1 second
    sl_sleeptimer_stop_timer (&timer_readaccel);
    sl_sleeptimer_start_timer_ms (&timer_readaccel, 1000, timer_cb_readaccel, NULL, 0, 0);
    // Start LED timer to display Accel status
    startLedTimer ();
    // Put pins in operational mode
    enable_SPI_pins ();
    // Start with fresh data
    accelClearFifo ();
    // Disable pins after use
    disable_SPI_pins ();
}

static void dostopaccel (void)
{
    if (active_module != ACTIVE_MODULE_ACCEL)
    {
        PRINT_ERROR ("Module %s not active\n\r", getModuleName(ACTIVE_MODULE_ACCEL));
        return;
    }
    PRINT_INFO ("stop_accel\n\r");
    // Stop timer to read Accel after 1 second
    sl_sleeptimer_stop_timer (&timer_readaccel);
    stop_module ();
}

static void doprintsamples (uint8_t temp)
{
    if ((temp != 0) && (temp != 1))
    {
        PRINT_ERROR("print_samples: incorrect argument %d\n\r", temp);
    }
    else
    {
        samples_print_enabled = temp;
    }
}

/**************************************************************************//**
 * Function for extracting parameters from BLE writes. Known issues:
 * 1) Commands which need fields are not rejected if the fields are not present.
 * 2) Commands with trailing fields are not rejected.
 *****************************************************************************/

typedef enum
{
    CMD_SWEEP_START = 10,
    CMD_SWEEP_STOP,
    CMD_SWEEP_SET_IQ_OFFSETS,
    CMD_SWEEP_SET_FREQ,
    CMD_SWEEP_RUN_CAL_OFFSETS,
    CMD_SWEEP_SET_LNA_VGA_GAIN,
    CMD_SWEEP_SET_TIMING,
    CMD_SWEEP_TRIG_FREQ,

    CMD_PPG_START = 20,
    CMD_PPG_STOP,
    CMD_PPG_SET_AGC,

    CMD_ACCEL_START = 30,
    CMD_ACCEL_STOP,

    CMD_MISC_PRINT_SAMPLES = 40,
} app_commands_t;

// TODO: processblesetting parses into int, but then passes the values to functions taking narrower types.
// So an out-of-range BLE value can wrap before the destination function validates it.
// For example, 256 passed to a uint8_t parameter becomes 0, which can then pass a range check.
static void processblesetting (char *payload, int len)
{
    // Validate arguments
    if (!payload || len <= 0) return;
    int temp0 = 0, temp1 = 0, temp2 = 0;
    bool temp1_exists = false, temp2_exists = false;
    // We wont need more than a reasonable sized input
    char str[8];
    int i = 0, k = 0;

    i = 0;
    // Copy the setting
    while ((k < len) && (payload[k] != ',') && (i < (int)sizeof(str)-1))
        str[i++] = payload[k++];
    str[i] = 0;
    // Reject the case when the end of the local str was reached without a comma
    if (((i == (int)sizeof(str)-1) && (k < len) && (payload[k] != ',')) || !i)
    {
        PRINT_ERROR ("ERROR: Invalid BLE setting payload: field 1\n\r");
        return;
    }
    temp0 = atoi (str);

    // Continue only if there's more input
    if (k < len)
    {
        // Skip the comma
        k++;
        i = 0;
        // Copy the setting
        while ((k < len) && (payload[k] != ',') && (i < (int)sizeof(str)-1))
            str[i++] = payload[k++];
        str[i] = 0;
        // Reject the case when the end of the local str was reached without a comma
        if (((i == (int)sizeof(str)-1) && (k < len) && (payload[k] != ',')) || !i)
        {
            PRINT_ERROR ("ERROR: Invalid BLE setting payload: field 2\n\r");
            return;
        }
        temp1 = atoi (str);
        temp1_exists = true;

        // Continue only if there's more input
        if (k < len)
        {
            // Skip the comma
            k++;
            i = 0;
            // Copy the setting
            while ((k < len) && (payload[k] != ',') && (i < (int)sizeof(str)-1))
                str[i++] = payload[k++];
            str[i] = 0;
            // Reject the case when the end of the local str was reached without a comma
            if (((i == (int)sizeof(str)-1) && (k < len) && (payload[k] != ',')) || !i)
            {
                PRINT_ERROR ("ERROR: Invalid BLE setting payload: field 3\n\r");
                return;
            }
            temp2 = atoi (str);
            temp2_exists = true;
        }
    }

    switch (temp0)
    {
        case CMD_SWEEP_START:
        dosweep ();
        break;

        case CMD_SWEEP_STOP:
        dostopsweep ();
        break;

        case CMD_SWEEP_SET_IQ_OFFSETS:
        // Make sure we did receive these fields
        if ((temp1_exists == true) && (temp2_exists == true))
            dosetiqoffsets (temp1, temp2);
        else
        {
            PRINT_ERROR ("ERROR: Missing argument(s) for command %d\n\r", temp0);
        }
        break;

        case CMD_SWEEP_SET_FREQ:
        // Make sure we did receive these fields
        if (temp1_exists == true)
            dosetfreq (temp1);
        else
        {
            PRINT_ERROR ("ERROR: Missing argument(s) for command %d\n\r", temp0);
        }
        break;

        case CMD_SWEEP_RUN_CAL_OFFSETS:
        run_cal_offsets = true;
        dosweep ();
        break;

        case CMD_SWEEP_SET_LNA_VGA_GAIN:
        // Make sure we did receive these fields
        if ((temp1_exists == true) && (temp2_exists == true))
            dosetlnavgagain (temp1, temp2);
        else
        {
            PRINT_ERROR ("ERROR: Missing argument(s) for command %d\n\r", temp0);
        }
        break;

        case CMD_SWEEP_SET_TIMING:
        // Make sure we did receive these fields
        if ((temp1_exists == true) && (temp2_exists == true))
            dosetsweeptiming (temp1, temp2);
        else
        {
            PRINT_ERROR ("ERROR: Missing argument(s) for command %d\n\r", temp0);
        }
        break;

        case CMD_SWEEP_TRIG_FREQ:
        // Make sure we did receive the field
        if (temp1_exists == true)
            dosettrigfreq (temp1);
        else
        {
            PRINT_ERROR ("ERROR: Missing argument(s) for command %d\n\r", temp0);
        }
        break;

        case CMD_ACCEL_START:
        dostartaccel ();
        break;

        case CMD_ACCEL_STOP:
        dostopaccel ();
        break;

#ifdef USE_PPG
        case CMD_PPG_START:
        dostartppg ();
        break;

        case CMD_PPG_STOP:
        dostopppg ();
        break;

        case CMD_PPG_SET_AGC:
        if (temp1_exists == true)
            dosetppgagc (temp1);
        else
        {
            PRINT_ERROR ("ERROR: Missing argument(s) for command %d\n\r", temp0);
        }
        break;
#endif

        case CMD_MISC_PRINT_SAMPLES:
        if (temp1_exists == true)
            doprintsamples (temp1);
        else
        {
            PRINT_ERROR ("ERROR: Missing argument(s) for command %d\n\r", temp0);
        }
        break;

        default:
        PRINT_ERROR ("ERROR: Invalid control command %d\n\r", temp0);
        break;
    }
}

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
#ifdef MEASURE_EM2_CURRENT
    enable_SPI_pins ();
    max86171_shutdown(&max86171_dev);
    shutoffRFICs ();
    accelPowerOff();
    setLedShutdown ();
    BOARD_pins_uninit ();
    while (1)
    {
        EMU_EnterEM2 (true);
    }
#endif
    PRINT_INIT ("Orbio Sensor version %d starting up ...\n\r", VERSION);
    enable_SPI_pins ();
    // Keep Accel always on for now, later put into low power mode in sleep if needed
    PRINT_INIT ("accelInit returned %d\n\r", accelInit ());
    PRINT_INIT ("KTD2052A ID = 0x%02x, expected 0x%02x\n\r", getLedID (), REG_ID_VAL);
    PRINT_INIT ("max86171_init returned %d\n\r", max86171_init(&max86171_dev));
    // Initialise CLI command table
    cli_custom_cmd_init();
    // Set default value for PPG variables
    ppg_init_done = false;
    ppg_agc_on = true;
    // Average both PPG1 & PPG2
    for (int i = 0; i < MAX86171_NUM_MEAS - 1; i++) PPG_Average_Type[i] = 3;
    // Set default values for variables set by command line
    trigfreq = 0;
    pll_gain = 8;
    sync_mode = 0;
    frequency_to_set_khz = 0;
    samples_print_enabled = false;
    run_cal_offsets = false;
    lnagain = 0;
    vgagain = CMX994_MAX_VGA_GAIN;
    clearIQOffsets ();
    sweep_interval_sec = SWEEP_INTERVAL_DEFAULT_SEC;
    active_time_sec = ACTIVE_TIME_DEFAULT_SEC;
    sweepState = SWEEP_STATE_INIT;
    sleepState = SLEEP_STATE_WAKE;
    active_module = ACTIVE_MODULE_NONE;
    // Setup Accel and PPG interrupts
    setup_interrupts ();
#ifdef USE_BLE
    // Setup defaults for BLE variables
    advertising_set_handle = SL_BT_INVALID_ADVERTISING_SET_HANDLE;
    OpenedConnectionHandle = SL_BT_INVALID_CONNECTION_HANDLE;
    transferEnabled = false;
    sendNotification = false;
    bytesRemaining = 0;
#endif

    // Display startup via LED briefly
    enableI2CLED ();
    setLed (LED_1, COLOUR_WHITE);
    sl_sleeptimer_delay_millisecond (LED_BLIP_TIME_MSEC);
    disableI2CLED ();

    PRINT_INIT ("Orbio Sensor initialized\n\r");
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////

#ifdef USE_BLE
    sendNextChunk ();
#endif
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
#ifdef USE_BLE
      setupDeviceName ();
      setupAdvertising ();
#ifdef BLE_KEEP_ALIVE
      startAdvertising ();
#endif
#endif
#ifdef CLI_KEEP_ALIVE
      // Shutdown / sleep ICs and disable pins
      // If CLI is available in sleep we do not need to start the sweep automatically
      setLowPowerMode ();
#else
      // Set to operating mode which is sweep by default
      unsetLowPowerMode ();
      // Start the sweep immediately
      // Control via CLI/BLE in wake, and BLE in sleep
      sl_sleeptimer_start_timer_ms (&timer_startsweep, SWEEP_START_DELAY_MSEC, timer_cb_startsweep, NULL, 0, 0);
#endif
      break;

    // -------------------------------
    // This event indicates that MTU value has been exchanged
    case sl_bt_evt_gatt_mtu_exchanged_id:
      PRINT_INFO ("MTU exchanged value = %d bytes\n\r", evt->data.evt_gatt_mtu_exchanged.mtu);
      break;

    // -------------------------------
    // This event indicates that connection parameters have changed
    case sl_bt_evt_connection_parameters_id:
    {
      uint16_t mtu_size;
      int connint, pdusize, latency, timeout;
      (void)mtu_size;
      (void)connint;
      (void)pdusize;
      (void)latency;
      (void)timeout;
      connint = (evt->data.evt_connection_parameters.interval * 50) / 4;
      PRINT_INFO ("Connection interval = %d.%d msec\n\r", connint / 10, connint % 10);
      pdusize = (evt->data.evt_connection_parameters.txsize);
      PRINT_INFO ("PDU size = %d bytes\n\r", pdusize);
      latency = (evt->data.evt_connection_parameters.latency);
      PRINT_INFO ("Connection latency = %d intervals\n\r", latency);
      timeout = (evt->data.evt_connection_parameters.timeout);
      PRINT_INFO ("Connection timeout = %d msec\n\r", 10 * timeout);
      sl_bt_gatt_server_get_mtu(evt->data.evt_connection_parameters.connection, &mtu_size);
      PRINT_INFO ("MTU size = %d bytes\n\r", mtu_size);
    }
    break;

    // -------------------------------
    // This event indicates that connection data lengths have changed
    case sl_bt_evt_connection_data_length_id:
    {
      int pdusize;
      pdusize = (evt->data.evt_connection_data_length.tx_data_len);
      (void)pdusize;
      PRINT_INFO ("PDU size in data length event = %d bytes\n\r", pdusize);
    }
    break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
#ifdef USE_BLE
      PRINT_INFO ("Opened BLE connection\n\r");
      // Note the connection handle for future use
      OpenedConnectionHandle = (evt->data.evt_connection_opened.connection);
      // To be enabled by client
      transferEnabled = false;
      sendNotification = false;
      bytesRemaining = 0;
      sl_bt_connection_set_data_length (OpenedConnectionHandle, 251, 2120);
      // Start LED timer to display BLE status
      startLedTimer ();
#endif
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
#ifdef USE_BLE
      OpenedConnectionHandle = SL_BT_INVALID_CONNECTION_HANDLE;
      // Set to default
      transferEnabled = false;
      sendNotification = false;
      bytesRemaining = 0;
      // If awake and collecting samples but BLE connection is lost then no
      // point collecting samples so go to sleep
      if (sleepState == SLEEP_STATE_WAKE)
      {
          PRINT_INFO ("BLE connection closed, go to sleep\n\r");
#ifdef USE_WAKEUP
          // Start timer to sleep till next sweep
          sl_sleeptimer_stop_timer (&timer_startsweep);
          sl_sleeptimer_start_timer_ms (&timer_startsweep, sweep_interval_sec * 1000L, timer_cb_startsweep, NULL, 0, 0);
          PRINT_INFO ("Sleep and set wakeup after %d sec\n\r", sweep_interval_sec);
#endif
          // Go to sleep
          setLowPowerMode ();
      }
      else
      {
          PRINT_INFO ("BLE connection closed and already in sleep\n\r");
      }

      // Stop LED timer for BLE status
      stopLedTimer ();

      // To keep logic simple, unconditionally either start or stop advertising here
      // depending on what is needed
#ifdef BLE_KEEP_ALIVE
      startAdvertising ();
#else
      stopAdvertising ();
#endif
#endif
      break;

#ifdef USE_BLE
      /* This event is generated when a connected client has either
       * 1) changed a Characteristic Client Configuration, meaning that they have enabled
       * or disabled Notifications or Indications, or
       * 2) sent a confirmation upon a successful reception of the indication. */
      case sl_bt_evt_gatt_server_characteristic_status_id:
        /* Check the characteristic in question using its ID.
         * Also check client_config_flags, meaning that the characteristic client configuration
         * was changed (notifications or indications enabled or disabled). */
        if ((evt->data.evt_gatt_server_characteristic_status.characteristic == gattdb_sweepdata)
            && (evt->data.evt_gatt_server_characteristic_status.status_flags == sl_bt_gatt_server_client_config)) {
          uint16_t ccf = evt->data.evt_gatt_server_characteristic_status.client_config_flags;
          if (ccf == sl_bt_gatt_notification) {
            /* Transfers have been turned ON by the client */
            transferEnabled = true;
            sendNotification = true;
            bytesRemaining = 0;
            sweepcount = 0;
            blestarttime = sl_sleeptimer_tick_to_ms (sl_sleeptimer_get_tick_count());
            PRINT_INFO ("Data Notifications turned on by the App\n\r");
          }
          else if (ccf == sl_bt_gatt_indication) {
            transferEnabled = false;
            sendNotification = false;
            bytesRemaining = 0;
            PRINT_INFO ("Data Indications turned on by the App: not supported as it is slow\n\r");
          } else if (evt->data.evt_gatt_server_characteristic_status.client_config_flags == 0x00) {
            /* Transfers have been turned OFF by the client */
            transferEnabled = false;
            sendNotification = false;
            bytesRemaining = 0;
            PRINT_INFO ("Data transfers turned off by the App\n\r");
          }
        }
        break;
#endif

    // This event indicates that a signal was posted from an interrupt or timer
    case sl_bt_evt_system_external_signal_id:
      // Wake via Accel timer
      // The LIS2DH12 FIFO is 32 deep, so we can read it every 1 second with ample margin
      // for timing.
      // Accel data rate: 25 Hz, 3 axes, 2 bytes per axis = 150 bytes per second data
      if (evt->data.evt_system_external_signal.extsignals & SIGNAL_ACCEL)
      {
          int num = read_accel_fifo ();
#ifdef USE_BLE
          handleNewPacket (ACTIVE_MODULE_ACCEL, (uint8_t *)resultArrayAccel, num * NUM_BYTES_PER_ACCEL_SAMPLE);
#endif
          sl_sleeptimer_stop_timer (&timer_readaccel);
          // Check if Accel should continue
          if (active_module == ACTIVE_MODULE_ACCEL)
          {
              // Start timer to read Accel after 1 second
              sl_sleeptimer_start_timer_ms (&timer_readaccel, 1000, timer_cb_readaccel, NULL, 0, 0);
          }
      }
#ifdef USE_PPG
      // Wake via PPG interrupt
      if (evt->data.evt_system_external_signal.extsignals & SIGNAL_PPG)
      {
          // Don't respond to interrupts if not initialised yet
          if (ppg_init_done)
          {
              // Read the PPG FIFO and parse if no errors during the read
              if (read_ppg_fifo () == true)
                  parse_ppg_fifo ();
          }
      }
#endif
      // Wake via sweep timer
      if (evt->data.evt_system_external_signal.extsignals & SIGNAL_SWEEP)
      {
          run_sweep_statemachine ();
      }
      // Wake via Accel interrupt
      if (evt->data.evt_system_external_signal.extsignals & SIGNAL_WAKE)
      {
          // Set to operating mode
          unsetLowPowerMode ();
      }
      break;

    // -------------------------------
    // This event indicates that client did a write
    case sl_bt_evt_gatt_server_user_write_request_id:
        if (evt->data.evt_gatt_server_user_write_request.characteristic == gattdb_set_parameters) {
          PRINT_INFO ("Got set parameters from BLE client\n\r");
          /* Send response to Write Request */
          sl_bt_gatt_server_send_user_write_response(
            evt->data.evt_gatt_server_user_write_request.connection,
            gattdb_set_parameters,
            SL_STATUS_OK);
          char *payload = (char *)evt->data.evt_gatt_server_user_write_request.value.data;
          int len = evt->data.evt_gatt_server_user_write_request.value.len;
          processblesetting (payload, len);
        }
    break;

    // This event indicates that client did a read
    case sl_bt_evt_gatt_server_user_read_request_id:
        if (evt->data.evt_gatt_server_user_read_request.characteristic == gattdb_report_fw) {
          uint8_t fw = VERSION;
          PRINT_INFO ("Got report firmware version from BLE client\n\r");
          /* Send response to Read Request */
          sl_bt_gatt_server_send_user_read_response(
            evt->data.evt_gatt_server_user_read_request.connection,
            gattdb_report_fw,
            SL_STATUS_OK,
            sizeof(fw),
            &fw,
            NULL);
        }
        else if (evt->data.evt_gatt_server_user_read_request.characteristic == gattdb_report_parameters) {
          PRINT_INFO ("Got report settings from BLE client\n\r");
          memset (reportsettings, 0, sizeof (reportsettings));
          snprintf (reportsettings, sizeof(reportsettings) - 1, "%d,%d,%d,%d,%d,%d", IQPRECISION, pll_gain, lnagain, vgagain, active_time_sec, sweep_interval_sec);
          /* Send response to Read Request */
          sl_bt_gatt_server_send_user_read_response(
            evt->data.evt_gatt_server_user_read_request.connection,
            gattdb_report_parameters,
            SL_STATUS_OK,
            strlen(reportsettings),
            (uint8_t *)&reportsettings,
            NULL);
        }
    break;

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}

/**************************************************************************//**
 * Functions called by CLI
 *****************************************************************************/

void cli_set_trigfreq(sl_cli_command_arg_t *arguments)
{
    int argc = sl_cli_get_argument_count(arguments);
    if (argc != 1)
    {
        PRINT_ERROR("set_trigfreq: incorrect number of arguments. Expected 1!\n\r");
        return;
    }
    uint8_t temp = sl_cli_get_argument_uint8(arguments, 0);
    dosettrigfreq (temp);
}

void cli_set_iq_offsets(sl_cli_command_arg_t *arguments)
{
    int argc = sl_cli_get_argument_count(arguments);
    if (argc != 2)
    {
        PRINT_ERROR("set_iq_offsets: incorrect number of arguments. Expected 2!\n\r");
        return;
    }
    int8_t temp1 = sl_cli_get_argument_int8(arguments, 0);
    int8_t temp2 = sl_cli_get_argument_int8(arguments, 1);
    dosetiqoffsets (temp1, temp2);
}

void cli_set_lna_vga_gain(sl_cli_command_arg_t *arguments)
{
    int argc = sl_cli_get_argument_count(arguments);
    if (argc != 2)
    {
        PRINT_ERROR("set_lna_vga_gain: incorrect number of arguments. Expected 2!\n\r");
        return;
    }
    uint8_t temp1 = sl_cli_get_argument_uint8(arguments, 0);
    uint8_t temp2 = sl_cli_get_argument_uint8(arguments, 1);
    dosetlnavgagain (temp1, temp2);
}

void cli_start_accel(sl_cli_command_arg_t *arguments)
{
    (void)arguments;
    dostartaccel ();
}

void cli_stop_accel(sl_cli_command_arg_t *arguments)
{
    (void)arguments;
    dostopaccel ();
}

#ifdef USE_PPG

void cli_start_ppg(sl_cli_command_arg_t *arguments)
{
    (void)arguments;
    dostartppg ();
}

void cli_stop_ppg(sl_cli_command_arg_t *arguments)
{
    (void)arguments;
    dostopppg ();
}

void cli_set_ppg_agc(sl_cli_command_arg_t *arguments)
{
    int argc = sl_cli_get_argument_count(arguments);
    if (argc != 1)
    {
        PRINT_ERROR("set_ppg_agc: incorrect number of arguments. Expected 1!\n\r");
        return;
    }
    uint8_t temp = sl_cli_get_argument_uint8(arguments, 0);
    dosetppgagc (temp);
}

#endif

void cli_dosweep(sl_cli_command_arg_t *arguments)
{
    (void)arguments;
    dosweep ();
}

void cli_stop_sweep(sl_cli_command_arg_t *arguments)
{
    (void)arguments;
    dostopsweep ();
}

void cli_set_sweep_timing(sl_cli_command_arg_t *arguments)
{
    int argc = sl_cli_get_argument_count(arguments);
    if (argc != 2)
    {
        PRINT_ERROR("set_sweep_timing: incorrect number of arguments. Expected 2!\n\r");
        return;
    }
    int temp1 = sl_cli_get_argument_uint16(arguments, 0);
    int temp2 = sl_cli_get_argument_uint16(arguments, 1);
    dosetsweeptiming (temp1, temp2);
}

void cli_set_freq(sl_cli_command_arg_t *arguments)
{
    int argc = sl_cli_get_argument_count(arguments);
    if (argc != 1)
    {
        PRINT_ERROR("set_freq: incorrect number of arguments. Expected 1!\n\r");
        return;
    }
    uint16_t temp = sl_cli_get_argument_uint16(arguments, 0);
    dosetfreq (temp);
}

void cli_cal_offsets(sl_cli_command_arg_t *arguments)
{
    (void)arguments;
    run_cal_offsets = true;
    dosweep ();
}

void cli_print_samples(sl_cli_command_arg_t *arguments)
{
    int argc = sl_cli_get_argument_count(arguments);
    if (argc != 1)
    {
        PRINT_ERROR("print_samples: incorrect number of arguments. Expected 1!\n\r");
        return;
    }
    uint8_t temp = sl_cli_get_argument_uint8(arguments, 0);
    doprintsamples (temp);
}

void cli_print_unit_id(sl_cli_command_arg_t *arguments)
{
    (void)arguments;
    PRINT_INFO ("Unit ID %s\n\r", devicename);
}

void cli_print_fw_ver(sl_cli_command_arg_t *arguments)
{
    (void)arguments;
    PRINT_INFO ("Firmware Version v%d\n\r", VERSION);
}

/* End of file */
