#include "sl_event_handler.h"

#include "em_chip.h"
#include "sl_interrupt_manager.h"
#include "sl_clock_manager_init.h"
#include "sl_device_init_dcdc.h"
#include "sl_clock_manager.h"
#include "sl_hfxo_manager.h"
#include "sl_memory_manager.h"
#include "pa_conversions_efr32.h"
#include "sl_rail_util_power_manager_init.h"
#include "sl_rail_util_pti.h"
#include "btl_interface.h"
#include "sl_sleeptimer.h"
#include "sl_mpu.h"
#include "app_timer_internal.h"
#include "sl_bluetooth.h"
#include "gpiointerrupt.h"
#include "sl_i2cspm_instances.h"
#include "sl_iostream_init_usart_instances.h"
#include "sl_mbedtls.h"
#include "nvm3_default.h"
#include "sl_spidrv_instances.h"
#include "sl_cli_instances.h"
#include "psa/crypto.h"
#include "sli_protocol_crypto.h"
#include "sl_iostream_init_instances.h"
#include "sl_power_manager.h"

void my_en_power_init (void);

void sl_platform_init(void)
{
  CHIP_Init();
  my_en_power_init ();
  sl_interrupt_manager_init();
  sl_clock_manager_init();
  sl_device_init_dcdc();
  sl_clock_manager_runtime_init();
  sl_hfxo_manager_init_hardware();
  sl_memory_init();
  bootloader_init();
  nvm3_initDefault();
  sl_power_manager_init();
}

void sl_driver_init(void)
{
  GPIOINT_Init();
  sl_i2cspm_init_instances();
  sl_spidrv_init_instances();
}

void sl_service_init(void)
{
  sl_sleeptimer_init();
  sl_hfxo_manager_init();
  sl_mpu_disable_execute_from_ram();
  sl_mbedtls_init();
  psa_crypto_init();
  sli_aes_seed_mask();
  sl_iostream_init_instances();
  sl_cli_instances_init();
}

void sl_stack_init(void)
{
  sl_rail_util_pa_init();
  sl_rail_util_power_manager_init();
  sl_rail_util_pti_init();
  sl_bt_init();
}

void sl_internal_app_init(void)
{
}

void sl_platform_process_action(void)
{
}

void sl_service_process_action(void)
{
  sli_app_timer_step();
  sl_cli_instances_tick();
}

void sl_stack_process_action(void)
{
  sl_bt_step();
}

void sl_internal_app_process_action(void)
{
}

void sl_iostream_init_instances(void)
{
  sl_iostream_usart_init_instances();
}

