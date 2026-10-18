# Orbio BioSensor Firmware

- This repository contains the source code for Firmware
- Firmware version v15 used for new hardware bringup
- Based on Silicon Labs Simplicity Studio Version SV5.12.0.1
 * Project name is orbio_sensor
 * Compiled with GNU ARM Compiler v12.2.1
 * SiLabs Simplicity SDK Suite v2024.6.1
 * Silabs Bluetooth stack v8.1.0
- Contains Command Line and Bluetooth Interface for control of the device
- Type "help" on the command line for a list and description of controls

Description of most recent firmware version changes:

orbio_biosensor_v16:
- Added wait for BLE operations to stop before switching off HFXO/EN_POWER
- Change A_FULL interrupt to no repeat else it triggers before status read and cleared
- BUGFIX: Corrected PWR_RDY interpretation
- Added more MAX86171 status bit prints
- Removed unused MAX86171 functions
- Added pll_gain to report settings BLE response
- Changed LFRCO to high precision mode for use instead of LFXO

==> orbio_biosensor_v15:
- Version used for new board bringup
- Added USE_PPG option for new board without optical section
- Changed LMX2572LP to use the RX range of frequencies as it goes to CMX994 LO input
- Enabled CMX994 TXLO output
- Added a check for IADC disable to be done only if enabled before else it gets stuck
- Added MAX86171 status bit prints
- Added MAX86171 power ready check and retry
- Enable all CS pins in wake whichever mode is active so that those ICs are in a known state
- Changed to 700 - 1080 MHz sweep range
- Added PLL gain change command via CLI & BLE
- Added printing of Accel XYZ samples

==> orbio_biosensor_v14:
- Added accelPowerOn function from existing code for future use
- BUGFIX: Accel data was being read into the same location without advancing the pointer
- BUGFIX: stop_sweep was getting rejected when sweep was active but in sleep

==> orbio_biosensor_v13:
- Added robustness (error handling, retry count) to BLE packet transfer module
- Moved triggerSweep outside the BLE packet transfer functions to decouple it from PPG & Accel
- Added packet count to packet header
- Added backoff time from packet send resource error to the next retry

==> orbio_biosensor_v12:
- BUGFIX: Stop the LED timer in Sweep state when going to sleep
- Added packet send for PPG & Accel
- Combined SendFirstChunk and sendNextChunk into a single function sendChunk
- Added 8-byte header of syncword, packet type, timestamp for each packet start
- Removed overhead/suboptimal sending of syncword in an initial packet by itself

==> orbio_biosensor_v11:
- Added comments, made some functions more robust
- Made changes for latest hardware revision which has only one RGB LED set
- Added a new LED module to blink LED for all modules in a centralised location, no need for LED code inside modules

==> orbio_biosensor_v10:
- Unified debug print macros concept across files
- Added more error code return value handling/printing
- Replaced GPIO_PinModeSet with GPIO_PinOutClear/Set in SPI functions as they are more efficient
- BUGFIX: Corrected index in ppg_agc_init loop to fill AGC struct with driver settings from max86171.h config structure
- BUGFIX: Added check for all bytes sent in first packet to trigger next sweep
- Used SL_BT_INVALID_CONNECTION_HANDLE and SL_BT_INVALID_ADVERTISING_SET_HANDLE instead of hard coded values
- Added active module check for restarting Accel timer

==> orbio_biosensor_v9:
- Added return code checking for a few more SiLabs APIs
- BUGFIX: module names should be indexed by bit position of module and not the shifted bit
- Added start/stop for the modules from BLE
- Added error checks to start/stop for the modules
- Made the BLE command string parsing more robust
- Brought in LMX2572LP code improvement from Orbio Radar project

==> orbio_biosensor_v8:
- Use GPIO_PinModeSet instead of GPIO_PinOutSet/Clear as Accel is initialised before pin modes are setup
- Handled return codes for all MAX86171 function calls for easy debugging
- Changed switch_off/on_en_power_hfxo so each module registers/deregisters its need for EN_POWER or HFXO
- Added search for tag=1 for the first time after initialisation as recommended in MAX86171 datasheet
- Separate functions to enable/disable Accel pins
- CLI_KEEP_ALIVE now set based on project configuration

==> orbio_biosensor_v7:
- Added check of SPIDRV_M* return values
- Classified printf in app.c in three PRINT categories to selectively enable as needed
- Added in event handling from Orbio radar to start BLE transfer
- Added check of sl_bt_gatt_server_send_notification return values
- BUGFIX: Accel read function was not returning the correct result
- Added new function to read Accel FIFO in one continuous operation instead of 6 separate bytes
- Switched to sli_clock_manager_hal_set_sysclk_source instead of CMU_ClockSelectSet for switching SYSCLK

==> orbio_biosensor_v6:
- BUGFIX: Reset current when proximity is not detected is set to a low non-zero value else there will be no signal at all to be able to detect proximity again
- EN_POWER enabling at startup and switching to/from HFRCO at low power mode entry/exit
- Switched SPI clock to 5MHz which is the least of the devices (CMX994: 5 MHz, Accel: 10 MHz, LMX2572LP: 75 MHz, MAX86171: 25 MHz)
- Changed Accel frequency to 25 Hz from 10 Hz
- Added enabling EN_POWER for TCXO in startup
- Changed to HFXO & LFXO active by default on startup
- Feeding raw sign extended sample to AGC instead of 3 bytes
- Some cleanup on variable types and enums
- BUGFIX: Corrected calculation of MAX86171 FIFO sample count
- BUGFIX: Sign extended PPG1 & PPG2 samples before averaging else the average can be very wrong
- Added reading of Accel FIFO using a 1 second timer
- BUGFIX: Set MAX86171 shutdown bits properly

==> orbio_biosensor_v5:
- Added PPG and Accel GPIO interrupts
- Added Accel code from older Iotini Bike project, changed it to use SPI instead of I2C
- Added TP1 GPIO
- BUGFIX: Corrected FIFO Configuration register bit offsets
- Added Radar sweep code from Orbio radar project
- Added low power mode code from Orbio radar project
- Added BLE event handling code from Orbio radar project
- Added CLI commands and functions to start/stop PPG
- Added functions to enable/disable pins for PPG, for entry/exit into low power mode
- Cleaned up misleading comments about LED usage in ppg_agc.c/h
- Added function max86171_shutdown()
- Added comments and cleaned up SPI interfaces
- Added comments for all MAX86171 register settings including each measurement
- BUGFIX: PPG AGC code had its own copy of DRVx settings for each measurement which were out of sync with the actual configuration table in max8671.h. This table was removed and replaced with code to extract the information from a single central source.

==> orbio_biosensor_v4:
- Added MAX86171 code
- Added board initialisations

==> orbio_biosensor_v3:
- Added CLI component
- Imported BLE GATT from Orbio Radar project

==> orbio_biosensor_v2:
- Added BLE code

==> orbio_biosensor_v1:
- First version
- Configured pins for UART printf, I2C, SPI, DCDC, HFXO (EXTCLK), LFXO
- CMX994 driver, LMX2572LP driver, IADC code imported from Orbio Radar project
