// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <sof/audio/pipeline/sof_static_pipeline.h>
#include <sof/audio/pipeline/static_pipeline.h>
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

static int cmd_sof_vol(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 3) {
		shell_error(sh, "Usage: sof vol <playback|capture|pb|cap> <dB>");
		return -EINVAL;
	}

	uint32_t pipe_id = (strcmp(argv[1], "playback") == 0 || strcmp(argv[1], "pb") == 0) ? 1 : 2;
	int db = atoi(argv[2]);
	int16_t uac_vol = (int16_t)(db * 256);

	sof_static_pipeline_set_volume(pipe_id, uac_vol);
	shell_print(sh, "%s volume set to %d dB.", pipe_id == 1 ? "Playback" : "Capture", db);
	return 0;
}

static int cmd_sof_play(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(sh, "Usage: sof play <start|stop>");
		return -EINVAL;
	}

	bool start = (strcmp(argv[1], "start") == 0);
	sof_static_pipeline_set_playback_active(start);
	shell_print(sh, "Playback pipeline %s.", start ? "STARTED" : "STOPPED");
	return 0;
}

static int cmd_sof_cap(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(sh, "Usage: sof cap <start|stop>");
		return -EINVAL;
	}

	bool start = (strcmp(argv[1], "start") == 0);
	sof_static_pipeline_set_capture_active(start);
	shell_print(sh, "Capture pipeline %s.", start ? "STARTED" : "STOPPED");
	return 0;
}

static int cmd_sof_ctl(const struct shell *sh, size_t argc, char **argv)
{
	const struct sof_static_topology *topo = sof_static_topology_get();
	if (!topo) {
		shell_error(sh, "No active static topology");
		return -ENODEV;
	}

	if (argc < 2 || strcmp(argv[1], "list") == 0) {
		shell_print(sh, "=== SOF Static Topology Controls (%s) ===", topo->name ? topo->name : "");
		shell_print(sh, "%-4s %-28s %-6s %-10s %s", "ID", "Name", "Comp", "Type", "Value");
		shell_print(sh, "-------------------------------------------------------------");
		for (size_t i = 0; i < topo->num_controls; i++) {
			const struct sof_static_kcontrol *ctl = &topo->controls[i];
			int32_t val = 0;
			sof_static_kcontrol_get(ctl->id, &val);
			const char *type_str = (ctl->type == SOF_STATIC_CTRL_VOLUME) ? "VOLUME" :
					       (ctl->type == SOF_STATIC_CTRL_SWITCH) ? "SWITCH" :
					       (ctl->type == SOF_STATIC_CTRL_ENUM) ? "ENUM" : "BINARY";
			shell_print(sh, "[%2u] %-28s comp:%-2u %-10s %d (0x%08x)",
				    ctl->id, ctl->name, ctl->target_comp_id, type_str, val, (uint32_t)val);
		}
		return 0;
	}

	if (strcmp(argv[1], "get") == 0) {
		if (argc < 3) {
			shell_error(sh, "Usage: sof ctl get <id|name>");
			return -EINVAL;
		}
		int ctrl_id = atoi(argv[2]);
		if (ctrl_id == 0 && strcmp(argv[2], "0") != 0) {
			ctrl_id = sof_static_kcontrol_find_by_name(argv[2]);
		}
		if (ctrl_id <= 0) {
			shell_error(sh, "Control not found: %s", argv[2]);
			return -ENOENT;
		}
		int32_t val = 0;
		int ret = sof_static_kcontrol_get((uint32_t)ctrl_id, &val);
		if (ret < 0) {
			shell_error(sh, "Failed to get control %d (ret %d)", ctrl_id, ret);
			return ret;
		}
		shell_print(sh, "Control [%d] value = %d (0x%x)", ctrl_id, val, (uint32_t)val);
		return 0;
	}

	if (strcmp(argv[1], "set") == 0) {
		if (argc < 4) {
			shell_error(sh, "Usage: sof ctl set <id|name> <value>");
			return -EINVAL;
		}
		int ctrl_id = atoi(argv[2]);
		if (ctrl_id == 0 && strcmp(argv[2], "0") != 0) {
			ctrl_id = sof_static_kcontrol_find_by_name(argv[2]);
		}
		if (ctrl_id <= 0) {
			shell_error(sh, "Control not found: %s", argv[2]);
			return -ENOENT;
		}
		int32_t val = (int32_t)strtol(argv[3], NULL, 0);
		int ret = sof_static_kcontrol_set((uint32_t)ctrl_id, val);
		if (ret < 0) {
			shell_error(sh, "Failed to set control %d (ret %d)", ctrl_id, ret);
			return ret;
		}
		shell_print(sh, "Control [%d] set to %d (0x%x)", ctrl_id, val, (uint32_t)val);
		return 0;
	}

	shell_error(sh, "Usage: sof ctl <list|get|set> [args]");
	return -EINVAL;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sof_cmds,
	SHELL_CMD(status, NULL, "Print current SOF pipeline and audio interface status", cmd_sof_status),
	SHELL_CMD(ctl, NULL, "List or set static kcontrols (sof ctl <list|get|set>)", cmd_sof_ctl),
	SHELL_CMD(play, NULL, "Start/stop playback pipeline (sof play <start|stop>)", cmd_sof_play),
	SHELL_CMD(cap, NULL, "Start/stop capture pipeline (sof cap <start|stop>)", cmd_sof_cap),
	SHELL_CMD(vol, NULL, "Set volume in dB (sof vol <pb|cap> <dB>)", cmd_sof_vol),
	SHELL_CMD(mode, NULL, "Configure interface clock mode (sof mode <i2s|pdm> <master|slave>)", cmd_sof_mode),
	SHELL_CMD(eq, NULL, "Control Equalizer bypass (sof eq <playback|capture> <enable|bypass>)", cmd_sof_eq),
	SHELL_CMD(drc, NULL, "Control DRC bypass (sof drc <enable|bypass>)", cmd_sof_drc),
	SHELL_CMD(tdfb, NULL, "Control TDFB beamformer bypass (sof tdfb <enable|bypass>)", cmd_sof_tdfb),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sof, &sof_cmds, "Sound Open Firmware (SOF) commands", NULL);

