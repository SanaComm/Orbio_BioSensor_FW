#include "sl_cli_handles.h"
#include "app.h"

void cli_set_trigfreq(sl_cli_command_arg_t *arguments);
void cli_set_iq_offsets(sl_cli_command_arg_t *arguments);
void cli_set_pll_gain(sl_cli_command_arg_t *arguments);
void cli_start_accel(sl_cli_command_arg_t *arguments);
void cli_stop_accel(sl_cli_command_arg_t *arguments);
#ifdef USE_PPG
void cli_start_ppg(sl_cli_command_arg_t *arguments);
void cli_stop_ppg(sl_cli_command_arg_t *arguments);
#endif
void cli_dosweep(sl_cli_command_arg_t *arguments);
void cli_stop_sweep(sl_cli_command_arg_t *arguments);
void cli_set_sweep_timing(sl_cli_command_arg_t *arguments);
void cli_msweep(sl_cli_command_arg_t *arguments);
void cli_print_iq(sl_cli_command_arg_t *arguments);
void cli_print_unit_id(sl_cli_command_arg_t *arguments);
void cli_print_fw_ver(sl_cli_command_arg_t *arguments);

static const sl_cli_command_info_t cmd_set_trigfreq =
    SL_CLI_COMMAND(cli_set_trigfreq, "Set TP1 trigger frequency (default 0)\r",
                    "frequency 0 (All), 1 to 20 (Individual)\r" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_UINT8,
                        SL_CLI_ARG_END,
                    });

static const sl_cli_command_info_t cmd_set_iq_offsets =
    SL_CLI_COMMAND(cli_set_iq_offsets, "Set I/Q Offsets in mV (default 0)\r",
                    "ioffset -775 to +775\r" SL_CLI_UNIT_SEPARATOR "qoffset -775 to +775\r" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_INT16,
                        SL_CLI_ARG_INT16,
                        SL_CLI_ARG_END,
                    });

static const sl_cli_command_info_t cmd_set_pll_gain =
    SL_CLI_COMMAND(cli_set_pll_gain, "Set PLL gain in 1 dB steps (default 8)\r",
                    "gain 0 to 18\r" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_UINT8,
                        SL_CLI_ARG_END,
                    });

static const sl_cli_command_info_t cmd_start_accel =
    SL_CLI_COMMAND(cli_start_accel, "Start periodic Accel read)\r",
                    "" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_END,
                    });

static const sl_cli_command_info_t cmd_stop_accel =
    SL_CLI_COMMAND(cli_stop_accel, "Stop periodic Accel read\r",
                    "" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_END,
                    });

#ifdef USE_PPG
static const sl_cli_command_info_t cmd_start_ppg =
    SL_CLI_COMMAND(cli_start_ppg, "Start PPG\r",
                    "" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_END,
                    });

static const sl_cli_command_info_t cmd_stop_ppg =
    SL_CLI_COMMAND(cli_stop_ppg, "Stop PPG\r",
                    "" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_END,
                    });
#endif

static const sl_cli_command_info_t cmd_dosweep =
    SL_CLI_COMMAND(cli_dosweep, "Automatic sweep\r",
                    "" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_END,
                    });

static const sl_cli_command_info_t cmd_stop_sweep =
    SL_CLI_COMMAND(cli_stop_sweep, "Stop sweep\r",
                    "" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_END,
                    });

static const sl_cli_command_info_t cmd_set_sweep_timing =
    SL_CLI_COMMAND(cli_set_sweep_timing, "Set sweep wake and sleep times in seconds (default 30, 600)\r",
                    "wake time 10 to 6000\r" SL_CLI_UNIT_SEPARATOR "sleep time 10 to 6000\r" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_UINT16,
                        SL_CLI_ARG_UINT16,
                        SL_CLI_ARG_END,
                    });

static const sl_cli_command_info_t cmd_msweep =
    SL_CLI_COMMAND(cli_msweep, "Manual sweep\r",
                    "" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_END,
                    });

static const sl_cli_command_info_t cmd_print_iq =
    SL_CLI_COMMAND(cli_print_iq, "Print IQ samples (default 0)\r",
                    "onoff 1:on 0:off\r" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_UINT8,
                        SL_CLI_ARG_END,
                    });

static const sl_cli_command_info_t cmd_print_unit_id =
    SL_CLI_COMMAND(cli_print_unit_id, "Print unit ID\r",
                    "" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_END,
                    });

static const sl_cli_command_info_t cmd_print_fw_ver =
    SL_CLI_COMMAND(cli_print_fw_ver, "Print Firmware Version Number\r",
                    "" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_END,
                    });

static sl_cli_command_entry_t custom_cmd_table[] =
{
    {"set_trigfreq", &cmd_set_trigfreq, false},
    {"set_iq_offsets", &cmd_set_iq_offsets, false},
    {"set_pll_gain", &cmd_set_pll_gain, false},
    {"start_accel", &cmd_start_accel, false},
    {"stop_accel", &cmd_stop_accel, false},
#ifdef USE_PPG
    {"start_ppg", &cmd_start_ppg, false},
    {"stop_ppg", &cmd_stop_ppg, false},
#endif
    {"set_sweep_timing", &cmd_set_sweep_timing, false},
    {"dosweep", &cmd_dosweep, false},
    {"stop_sweep", &cmd_stop_sweep, false},
    {"msweep", &cmd_msweep, false},
    {"print_iq", &cmd_print_iq, false},
    {"print_unit_id", &cmd_print_unit_id, false},
    {"print_fw_ver", &cmd_print_fw_ver, false},
    {NULL, NULL, false},
};

static sl_cli_command_group_t custom_cmd_group =
{
    {NULL},
    false,
    custom_cmd_table,
};

void cli_custom_cmd_init(void)
{
    sl_cli_command_add_command_group(sl_cli_inst0_handle, &custom_cmd_group);
}

/* End of file */
