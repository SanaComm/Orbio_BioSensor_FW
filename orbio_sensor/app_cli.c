#include "sl_cli_handles.h"
#include "app.h"

void cli_set_trigfreq(sl_cli_command_arg_t *arguments);
void cli_set_iq_offsets(sl_cli_command_arg_t *arguments);
void cli_set_lna_vga_gain(sl_cli_command_arg_t *arguments);
void cli_start_accel(sl_cli_command_arg_t *arguments);
void cli_stop_accel(sl_cli_command_arg_t *arguments);
#ifdef USE_PPG
void cli_start_ppg(sl_cli_command_arg_t *arguments);
void cli_stop_ppg(sl_cli_command_arg_t *arguments);
void cli_set_ppg_agc(sl_cli_command_arg_t *arguments);
void cli_set_ppg_meas(sl_cli_command_arg_t *arguments);
#endif
void cli_dosweep(sl_cli_command_arg_t *arguments);
void cli_stop_sweep(sl_cli_command_arg_t *arguments);
void cli_set_sweep_timing(sl_cli_command_arg_t *arguments);
void cli_set_freq(sl_cli_command_arg_t *arguments);
void cli_cal_offsets(sl_cli_command_arg_t *arguments);
void cli_print_samples(sl_cli_command_arg_t *arguments);
void cli_print_unit_id(sl_cli_command_arg_t *arguments);
void cli_print_fw_ver(sl_cli_command_arg_t *arguments);

static const sl_cli_command_info_t cmd_set_trigfreq =
    SL_CLI_COMMAND(cli_set_trigfreq, "Set TP1 trigger frequency (default 0)\r",
                    "frequency 0 (All), 1 to 39 (Individual)\r" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_UINT8,
                        SL_CLI_ARG_END,
                    });

static const sl_cli_command_info_t cmd_set_iq_offsets =
    SL_CLI_COMMAND(cli_set_iq_offsets, "Set I/Q Offsets (default 0) Step ranges from 25 mV to 0.2 mV depending on VGA gain\r",
                    "ioffset -31 to +31\r" SL_CLI_UNIT_SEPARATOR "qoffset -31 to +31\r" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_INT8,
                        SL_CLI_ARG_INT8,
                        SL_CLI_ARG_END,
                    });

static const sl_cli_command_info_t cmd_set_lna_vga_gain =
    SL_CLI_COMMAND(cli_set_lna_vga_gain, "Set Mixer LNA (default 0) and VGA (default 7) gains\r",
                    "LNA gain 0 to 3 (0 to -18 dB)\r" SL_CLI_UNIT_SEPARATOR "VGA gain 0 to 7 (0 to -42 dB)\r" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_UINT8,
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

static const sl_cli_command_info_t cmd_set_ppg_agc =
    SL_CLI_COMMAND(cli_set_ppg_agc, "Set PPG AGC (default 1)\r",
                    "Set PPG AGC to On (1) or Off (0)\r" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_UINT8,
                        SL_CLI_ARG_END,
                    });

static const sl_cli_command_info_t cmd_set_ppg_meas =
    SL_CLI_COMMAND(cli_set_ppg_meas, "Set PPG Measurement parameters\r",
                    "Choose PPG measurement (1 to 8)\r" SL_CLI_UNIT_SEPARATOR "Set average (1 (PPG1), 2 (PPG2), 3 (default both))\r" SL_CLI_UNIT_SEPARATOR "Set current for DRVA/DRVB/DRVC (0 (no change) to 255 (32 mA)) in steps of 0.1255 mA\r" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_UINT8,
                        SL_CLI_ARG_UINT8,
                        SL_CLI_ARG_UINT8,
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

static const sl_cli_command_info_t cmd_set_freq =
    SL_CLI_COMMAND(cli_set_freq, "Set desired frequency\r",
                    "freq 700 MHz to 1080 MHz in steps of 10 MHz\r" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_UINT16,
                        SL_CLI_ARG_END,
                    });

static const sl_cli_command_info_t cmd_cal_offsets =
    SL_CLI_COMMAND(cli_cal_offsets, "Calibrate IQ offsets\r",
                    "" SL_CLI_UNIT_SEPARATOR,
                    {
                        SL_CLI_ARG_END,
                    });

static const sl_cli_command_info_t cmd_print_samples =
    SL_CLI_COMMAND(cli_print_samples, "Print samples (default 0)\r",
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
    {"set_lna_vga_gain", &cmd_set_lna_vga_gain, false},
    {"start_accel", &cmd_start_accel, false},
    {"stop_accel", &cmd_stop_accel, false},
#ifdef USE_PPG
    {"start_ppg", &cmd_start_ppg, false},
    {"stop_ppg", &cmd_stop_ppg, false},
    {"set_ppg_agc", &cmd_set_ppg_agc, false},
    {"set_ppg_meas", &cmd_set_ppg_meas, false},
#endif
    {"set_sweep_timing", &cmd_set_sweep_timing, false},
    {"dosweep", &cmd_dosweep, false},
    {"stop_sweep", &cmd_stop_sweep, false},
    {"set_freq", &cmd_set_freq, false},
    {"cal_offsets", &cmd_cal_offsets, false},
    {"print_samples", &cmd_print_samples, false},
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
