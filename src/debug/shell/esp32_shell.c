// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <sof/audio/pipeline/sof_static_pipeline.h>
#include <string.h>
#include <stdlib.h>

static int cmd_sof_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	struct sof_static_pipeline_status status;
	sof_static_pipeline_get_status(&status);

	shell_print(sh, "=== Sound Open Firmware (SOF) Status ===");
	shell_print(sh, "  Playback Pipeline: %s", status.playback_active ? "RUNNING" : "STOPPED");
	shell_print(sh, "  Capture Pipeline:  %s", status.capture_active ? "RUNNING" : "STOPPED");
	shell_print(sh, "  Active Interface:  %s", status.active_interface == SOF_AUDIO_IF_I2S ? "I2S0" : "PDM0");
	shell_print(sh, "  Clock Mode:        %s", status.clock_mode == SOF_CLOCK_MASTER ? "MASTER" : "SLAVE (Default)");
	shell_print(sh, "  Sample Rate:       %u Hz", status.sample_rate);
	shell_print(sh, "  Playback Volume:   %d dB (Mute: %s)", status.playback_volume / 256, status.playback_mute ? "YES" : "NO");
	shell_print(sh, "  Capture Volume:    %d dB (Mute: %s)", status.capture_volume / 256, status.capture_mute ? "YES" : "NO");
	shell_print(sh, "  Playback EQ:       %s", status.eq_playback_bypassed ? "BYPASS" : "ENABLED");
	shell_print(sh, "  Playback DRC:      %s", status.drc_playback_bypassed ? "BYPASS" : "ENABLED");
	shell_print(sh, "  Capture TDFB:      %s", status.tdfb_capture_bypassed ? "BYPASS" : "ENABLED");
	shell_print(sh, "  Capture EQ:        %s", status.eq_capture_bypassed ? "BYPASS" : "ENABLED");
	shell_print(sh, "========================================");

	return 0;
}

static int cmd_sof_mode(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 3) {
		shell_error(sh, "Usage: sof mode <i2s|pdm> <master|slave>");
		return -EINVAL;
	}

	enum sof_audio_interface iface;
	if (strcmp(argv[1], "i2s") == 0) {
		iface = SOF_AUDIO_IF_I2S;
	} else if (strcmp(argv[1], "pdm") == 0) {
		iface = SOF_AUDIO_IF_PDM;
	} else {
		shell_error(sh, "Invalid interface: %s (choose i2s or pdm)", argv[1]);
		return -EINVAL;
	}

	enum sof_clock_mode mode;
	if (strcmp(argv[2], "master") == 0) {
		mode = SOF_CLOCK_MASTER;
	} else if (strcmp(argv[2], "slave") == 0) {
		mode = SOF_CLOCK_SLAVE;
	} else {
		shell_error(sh, "Invalid mode: %s (choose master or slave)", argv[2]);
		return -EINVAL;
	}

	sof_static_pipeline_set_clock_mode(iface, mode);
	shell_print(sh, "Interface %s set to %s mode.", argv[1], argv[2]);
	return 0;
}

static int cmd_sof_eq(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 3) {
		shell_error(sh, "Usage: sof eq <playback|capture> <enable|bypass>");
		return -EINVAL;
	}

	bool is_capture = (strcmp(argv[1], "capture") == 0);
	bool bypass = (strcmp(argv[2], "bypass") == 0);

	sof_static_pipeline_set_eq_bypass(is_capture, bypass);
	shell_print(sh, "%s EQ set to %s.", is_capture ? "Capture" : "Playback", bypass ? "BYPASS" : "ENABLED");
	return 0;
}

static int cmd_sof_drc(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(sh, "Usage: sof drc <enable|bypass>");
		return -EINVAL;
	}

	bool bypass = (strcmp(argv[1], "bypass") == 0);
	sof_static_pipeline_set_drc_bypass(bypass);
	shell_print(sh, "Playback DRC set to %s.", bypass ? "BYPASS" : "ENABLED");
	return 0;
}

static int cmd_sof_tdfb(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(sh, "Usage: sof tdfb <enable|bypass>");
		return -EINVAL;
	}

	bool bypass = (strcmp(argv[1], "bypass") == 0);
	sof_static_pipeline_set_tdfb_bypass(bypass);
	shell_print(sh, "Capture TDFB set to %s.", bypass ? "BYPASS" : "ENABLED");
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sof_cmds,
	SHELL_CMD(status, NULL, "Print current SOF pipeline and audio interface status", cmd_sof_status),
	SHELL_CMD(mode, NULL, "Configure interface clock mode (sof mode <i2s|pdm> <master|slave>)", cmd_sof_mode),
	SHELL_CMD(eq, NULL, "Control Equalizer bypass (sof eq <playback|capture> <enable|bypass>)", cmd_sof_eq),
	SHELL_CMD(drc, NULL, "Control DRC bypass (sof drc <enable|bypass>)", cmd_sof_drc),
	SHELL_CMD(tdfb, NULL, "Control TDFB beamformer bypass (sof tdfb <enable|bypass>)", cmd_sof_tdfb),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sof, &sof_cmds, "Sound Open Firmware (SOF) commands", NULL);
