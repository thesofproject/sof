// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018-2024 Intel Corporation.

#if CONFIG_IPC_MAJOR_4

#include <sof/audio/component_ext.h>
#include <sof/lib/notifier.h>
#include <sof/audio/component_ext.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <ipc/stream.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "testbench/utils.h"
#include "testbench/file.h"
#include "testbench/topology_ipc4.h"
#include "testbench/trace.h"

#if defined __XCC__
#include <xtensa/tie/xt_timer.h>
#endif

SOF_DEFINE_REG_UUID(testbench);
DECLARE_TR_CTX(testbench_tr, SOF_UUID(testbench_uuid), LOG_LEVEL_INFO);
LOG_MODULE_REGISTER(testbench, CONFIG_SOF_LOG_LEVEL);

/* testbench helper functions for pipeline setup and trigger */

int tb_setup(struct sof *sof, struct testbench_prm *tp)
{
	struct ll_schedule_domain domain = {0};
	int bits;
	int krate;

	/* init components */
	sys_comp_init(sof);

	/* Module adapter components */
	sys_comp_module_aria_interface_init();
	sys_comp_module_crossover_interface_init();
	sys_comp_module_dcblock_interface_init();
	sys_comp_module_demux_interface_init();
	sys_comp_module_drc_interface_init();
	sys_comp_module_eq_fir_interface_init();
	sys_comp_module_eq_iir_interface_init();
	sys_comp_module_file_interface_init();
	sys_comp_module_gain_interface_init();
	sys_comp_module_google_rtc_audio_processing_interface_init();
	sys_comp_module_igo_nr_interface_init();
	sys_comp_module_level_multiplier_interface_init();
	sys_comp_module_mfcc_interface_init();
	sys_comp_module_mixin_interface_init();
	sys_comp_module_mixout_interface_init();
	sys_comp_module_multiband_drc_interface_init();
	sys_comp_module_mux_interface_init();
	sys_comp_module_rtnr_interface_init();
	sys_comp_module_selector_interface_init();
	sys_comp_module_src_interface_init();
	sys_comp_module_asrc_interface_init();
	sys_comp_module_tdfb_interface_init();
	sys_comp_module_template_comp_interface_init();
	sys_comp_module_volume_interface_init();

	/* other necessary initializations */
	pipeline_posn_init(sof);
	init_system_notify(sof);
	tb_enable_trace(tp->trace_level);

	/* init IPC */
	if (ipc_init(sof) < 0) {
		fprintf(stderr, "error: IPC init\n");
		return -EINVAL;
	}

	/* Trace */
	ipc_tr.level = LOG_LEVEL_INFO;
	ipc_tr.uuid_p = SOF_UUID(testbench_uuid);

	/* init LL scheduler */
	if (scheduler_init_ll(&domain) < 0) {
		fprintf(stderr, "error: edf scheduler init\n");
		return -EINVAL;
	}

	/* init EDF scheduler */
	if (scheduler_init_edf() < 0) {
		fprintf(stderr, "error: edf scheduler init\n");
		return -EINVAL;
	}

	tb_debug_print("ipc and scheduler initialized\n");

	/* setup IPC4 audio format */
	tp->num_configs = 1;
	krate = tp->fs_in / 1000;
	switch (tp->frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		bits = 16;
		break;
	case SOF_IPC_FRAME_S24_4LE:
		bits = 24;
		break;
	case SOF_IPC_FRAME_S32_LE:
		bits = 32;
		break;
	default:
		fprintf(stderr, "error: unsupported frame format %d\n", tp->frame_fmt);
		return -EINVAL;
	}

	/* TODO 44.1 kHz like rates */
	snprintf(tp->config[0].name, TB_MAX_CONFIG_NAME_SIZE, "%dk%dc%db",
		 krate, tp->channels_in, bits);
	tp->num_configs = 1;
	tp->config[0].buffer_frames = 2 * krate;
	tp->config[0].buffer_time = 0;
	tp->config[0].period_frames = krate;
	tp->config[0].period_time = 0;
	tp->config[0].rate = tp->fs_in;
	tp->config[0].channels = tp->channels_in;
	tp->config[0].format = tp->frame_fmt;
	tp->period_frames = krate;

	/* TODO: Need to set this later for larger topologies with multiple PCMs. The
	 * pipelines are determined based on just the PCM ID for the device that we
	 * want to start playback/capture on.
	 */
	tp->pcm_id = 0;

	return 0;
}

static int tb_prepare_widget(struct testbench_prm *tp, struct tplg_pcm_info *pcm_info,
			     struct tplg_comp_info *comp_info, int dir)
{
	struct tplg_pipeline_list *pipeline_list;
	int ret, i;

	if (dir)
		pipeline_list = &pcm_info->capture_pipeline_list;
	else
		pipeline_list = &pcm_info->playback_pipeline_list;

	/* populate base config */
	ret = tb_set_up_widget_base_config(tp, comp_info);
	if (ret < 0)
		return ret;

	tb_pipeline_update_resource_usage(tp, comp_info);

	/* add pipeline to pcm pipeline_list if needed */
	for (i = 0; i < pipeline_list->count; i++) {
		struct tplg_pipeline_info *pipe_info = pipeline_list->pipelines[i];

		if (pipe_info == comp_info->pipe_info)
			break;
	}

	if (i == pipeline_list->count) {
		pipeline_list->pipelines[pipeline_list->count] = comp_info->pipe_info;
		pipeline_list->count++;
		if (pipeline_list->count == TPLG_MAX_PCM_PIPELINES) {
			fprintf(stderr, "error: pipelines count exceeds %d",
				TPLG_MAX_PCM_PIPELINES);
			return -EINVAL;
		}
	}

	return 0;
}

static int tb_prepare_widgets_playback(struct testbench_prm *tp, struct tplg_pcm_info *pcm_info,
				       struct tplg_comp_info *starting_comp_info,
				       struct tplg_comp_info *current_comp_info)
{
	struct list_item *item;
	int ret;

	/* for playback */
	list_for_item(item, &tp->route_list) {
		struct tplg_route_info *route_info = container_of(item, struct tplg_route_info,
								  item);

		if (route_info->source != current_comp_info)
			continue;

		/* set up source widget if it is the starting widget */
		if (starting_comp_info == current_comp_info) {
			ret = tb_prepare_widget(tp, pcm_info, current_comp_info, 0);
			if (ret < 0)
				return ret;
		}

		/* set up the sink widget */
		ret = tb_prepare_widget(tp, pcm_info, route_info->sink, 0);
		if (ret < 0)
			return ret;

		/* and then continue down the path */
		if (route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_IN) {
			ret = tb_prepare_widgets_playback(tp, pcm_info, starting_comp_info,
							  route_info->sink);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int tb_prepare_widgets_capture(struct testbench_prm *tp, struct tplg_pcm_info *pcm_info,
				      struct tplg_comp_info *starting_comp_info,
				      struct tplg_comp_info *current_comp_info)
{
	struct list_item *item;
	int ret;

	/* for capture */
	list_for_item(item, &tp->route_list) {
		struct tplg_route_info *route_info = container_of(item, struct tplg_route_info,
								  item);

		if (route_info->sink != current_comp_info)
			continue;

		/* set up sink widget if it is the starting widget */
		if (starting_comp_info == current_comp_info) {
			ret = tb_prepare_widget(tp, pcm_info, current_comp_info, 1);
			if (ret < 0)
				return ret;
		}

		/* set up the source widget */
		ret = tb_prepare_widget(tp, pcm_info, route_info->source, 1);
		if (ret < 0)
			return ret;

		/* and then continue up the path */
		if (route_info->source->type != SND_SOC_TPLG_DAPM_DAI_OUT) {
			ret = tb_prepare_widgets_capture(tp, pcm_info, starting_comp_info,
							 route_info->source);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int tb_set_up_widget(struct testbench_prm *tp, struct tplg_comp_info *comp_info)
{
	struct tplg_pipeline_info *pipe_info = comp_info->pipe_info;
	struct tb_glb_state *glb = &tp->glb_ctx;
	struct tb_ctl *ctl;
	int ret;
	int i;

	pipe_info->usage_count++;

	/* first set up pipeline if needed, only done once for the first pipeline widget */
	if (pipe_info->usage_count == 1) {
		ret = tb_set_up_pipeline(tp, pipe_info);
		if (ret < 0) {
			pipe_info->usage_count--;
			return ret;
		}
	}

	/* now set up the widget */
	ret = tb_set_up_widget_ipc(tp, comp_info);
	if (ret < 0)
		return ret;

	/* send kcontrol bytes data */
	for (i = 0; i < glb->num_ctls; i++) {
		struct sof_abi_hdr *abi;

		ctl = &glb->ctl[i];

		/* send the bytes data from kcontrols associated with current widget */
		if (ctl->module_id != comp_info->module_id ||
		    ctl->instance_id != comp_info->instance_id ||
		    ctl->type != SND_SOC_TPLG_TYPE_BYTES)
			continue;

		abi = (struct sof_abi_hdr *)ctl->data;

		/* send IPC with kcontrol data */
		ret = tb_send_bytes_data(&tp->ipc_tx, &tp->ipc_rx,
					 comp_info->module_id, comp_info->instance_id, abi);
		if (ret < 0) {
			fprintf(stderr, "Error: Failed to set bytes data for widget %s.\n",
				comp_info->name);
			return ret;
		}
	}

	return 0;
}

static int tb_set_up_widgets_playback(struct testbench_prm *tp,
				      struct tplg_comp_info *starting_comp_info,
				      struct tplg_comp_info *current_comp_info)
{
	struct list_item *item;
	int ret;

	/* for playback */
	list_for_item(item, &tp->route_list) {
		struct tplg_route_info *route_info = container_of(item, struct tplg_route_info,
								  item);

		if (route_info->source != current_comp_info)
			continue;

		/* set up source widget if it is the starting widget */
		if (starting_comp_info == current_comp_info) {
			ret = tb_set_up_widget(tp, current_comp_info);
			if (ret < 0)
				return ret;
		}

		/* set up the sink widget */
		ret = tb_set_up_widget(tp, route_info->sink);
		if (ret < 0)
			return ret;

		/* source and sink widgets are up, so set up route now */
		ret = tb_set_up_route(tp, route_info);
		if (ret < 0)
			return ret;

		/* and then continue down the path */
		if (route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_IN) {
			ret = tb_set_up_widgets_playback(tp, starting_comp_info, route_info->sink);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int tb_set_up_widgets_capture(struct testbench_prm *tp,
				     struct tplg_comp_info *starting_comp_info,
				     struct tplg_comp_info *current_comp_info)
{
	struct list_item *item;
	int ret;

	/* for playback */
	list_for_item(item, &tp->route_list) {
		struct tplg_route_info *route_info = container_of(item, struct tplg_route_info,
								  item);

		if (route_info->sink != current_comp_info)
			continue;

		/* set up source widget if it is the starting widget */
		if (starting_comp_info == current_comp_info) {
			ret = tb_set_up_widget(tp, current_comp_info);
			if (ret < 0)
				return ret;
		}

		/* set up the sink widget */
		ret = tb_set_up_widget(tp, route_info->source);
		if (ret < 0)
			return ret;

		/* source and sink widgets are up, so set up route now */
		ret = tb_set_up_route(tp, route_info);
		if (ret < 0)
			return ret;

		/* and then continue down the path */
		if (route_info->source->type != SND_SOC_TPLG_DAPM_DAI_OUT) {
			ret = tb_set_up_widgets_capture(tp, starting_comp_info, route_info->source);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int tb_set_up_pipelines(struct testbench_prm *tp, int dir)
{
	struct tplg_comp_info *host = NULL;
	struct tplg_pcm_info *pcm_info;
	struct list_item *item;
	int ret;

	list_for_item(item, &tp->pcm_list) {
		pcm_info = container_of(item, struct tplg_pcm_info, item);

		if (pcm_info->id == tp->pcm_id) {
			if (dir)
				host = pcm_info->capture_host;
			else
				host = pcm_info->playback_host;
			break;
		}
	}

	if (!host) {
		fprintf(stderr, "No host component found for PCM ID: %d\n", tp->pcm_id);
		return -EINVAL;
	}

	if (!tb_is_pipeline_enabled(tp, host->pipeline_id))
		return 0;

	tp->pcm_info = pcm_info; /* TODO must be an array for multiple PCMs */

	if (dir) {
		ret = tb_prepare_widgets_capture(tp, pcm_info, host, host);
		if (ret < 0)
			return ret;

		ret = tb_set_up_widgets_capture(tp, host, host);
		if (ret < 0)
			return ret;

		tb_debug_print("Setting up capture pipelines complete\n");

		return 0;
	}

	ret = tb_prepare_widgets_playback(tp, pcm_info, host, host);
	if (ret < 0)
		return ret;

	ret = tb_set_up_widgets_playback(tp, host, host);
	if (ret < 0)
		return ret;

	tb_debug_print("Setting up playback pipelines complete\n");

	return 0;
}

int tb_set_up_all_pipelines(struct testbench_prm *tp)
{
	int ret;

	ret = tb_set_up_pipelines(tp, SOF_IPC_STREAM_PLAYBACK);
	if (ret) {
		fprintf(stderr, "error: Failed tb_set_up_pipelines for playback\n");
		return ret;
	}

	ret = tb_set_up_pipelines(tp, SOF_IPC_STREAM_CAPTURE);
	if (ret) {
		fprintf(stderr, "error: Failed tb_set_up_pipelines for capture\n");
		return ret;
	}

	tb_debug_print("pipelines set up complete\n");
	return 0;
}

static int tb_free_widgets_playback(struct testbench_prm *tp,
				    struct tplg_comp_info *starting_comp_info,
				    struct tplg_comp_info *current_comp_info)
{
	struct tplg_route_info *route_info;
	struct list_item *item;
	int ret;

	/* for playback */
	list_for_item(item, &tp->route_list) {
		route_info = container_of(item, struct tplg_route_info, item);
		if (route_info->source != current_comp_info)
			continue;

		/* Widgets will be freed when the pipeline is deleted, so just unbind modules */
		ret = tb_free_route(tp, route_info);
		if (ret < 0)
			return ret;

		/* and then continue down the path */
		if (route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_IN) {
			ret = tb_free_widgets_playback(tp, starting_comp_info, route_info->sink);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int tb_free_widgets_capture(struct testbench_prm *tp,
				   struct tplg_comp_info *starting_comp_info,
				   struct tplg_comp_info *current_comp_info)
{
	struct tplg_route_info *route_info;
	struct list_item *item;
	int ret;

	/* for playback */
	list_for_item(item, &tp->route_list) {
		route_info = container_of(item, struct tplg_route_info, item);
		if (route_info->sink != current_comp_info)
			continue;

		/* Widgets will be freed when the pipeline is deleted, so just unbind modules */
		ret = tb_free_route(tp, route_info);
		if (ret < 0)
			return ret;

		/* and then continue down the path */
		if (route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_OUT) {
			ret = tb_free_widgets_capture(tp, starting_comp_info, route_info->source);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int tb_free_pipelines(struct testbench_prm *tp, int dir)
{
	struct tplg_pipeline_list *pipeline_list;
	struct tplg_pcm_info *pcm_info;
	struct list_item *item;
	struct tplg_comp_info *host = NULL;
	int ret, i;

	list_for_item(item, &tp->pcm_list) {
		pcm_info = container_of(item, struct tplg_pcm_info, item);
		if (dir)
			host = pcm_info->capture_host;
		else
			host = pcm_info->playback_host;

		if (!host || !tb_is_pipeline_enabled(tp, host->pipeline_id))
			continue;

		if (dir) {
			pipeline_list = &tp->pcm_info->capture_pipeline_list;
			ret = tb_free_widgets_capture(tp, host, host);
			if (ret < 0) {
				fprintf(stderr, "failed to free widgets for capture PCM\n");
				return ret;
			}
		} else {
			pipeline_list = &tp->pcm_info->playback_pipeline_list;
			ret = tb_free_widgets_playback(tp, host, host);
			if (ret < 0) {
				fprintf(stderr, "failed to free widgets for playback PCM\n");
				return ret;
			}
		}
		for (i = 0; i < pipeline_list->count; i++) {
			struct tplg_pipeline_info *pipe_info = pipeline_list->pipelines[i];

			ret = tb_delete_pipeline(tp, pipe_info);
			if (ret < 0)
				return ret;
		}
	}

	tp->instance_ids[SND_SOC_TPLG_DAPM_SCHEDULER] = 0;
	return 0;
}

int tb_free_all_pipelines(struct testbench_prm *tp)
{
	tb_debug_print("freeing playback direction\n");
	tb_free_pipelines(tp, SOF_IPC_STREAM_PLAYBACK);

	tb_debug_print("freeing capture direction\n");
	tb_free_pipelines(tp, SOF_IPC_STREAM_CAPTURE);
	return 0;
}

void tb_free_topology(struct testbench_prm *tp)
{
	struct tplg_pcm_info *pcm_info;
	struct tplg_comp_info *comp_info;
	struct tplg_route_info *route_info;
	struct tplg_pipeline_info *pipe_info;
	struct tplg_context *ctx = &tp->tplg;
	struct sof_ipc4_available_audio_format *available_fmts;
	struct list_item *item, *_item;

	list_for_item_safe(item, _item, &tp->pcm_list) {
		pcm_info = container_of(item, struct tplg_pcm_info, item);
		list_item_del(item);
		free(pcm_info->name);
		free(pcm_info);
	}

	list_for_item_safe(item, _item, &tp->widget_list) {
		comp_info = container_of(item, struct tplg_comp_info, item);
		available_fmts = &comp_info->available_fmt;
		list_item_del(item);
		free(available_fmts->output_pin_fmts);
		free(available_fmts->input_pin_fmts);
		free(comp_info->name);
		free(comp_info->stream_name);
		free(comp_info->ipc_payload);
		free(comp_info);
	}

	list_for_item_safe(item, _item, &tp->route_list) {
		route_info = container_of(item, struct tplg_route_info, item);
		list_item_del(item);
		free(route_info);
	}

	list_for_item_safe(item, _item, &tp->pipeline_list) {
		pipe_info = container_of(item, struct tplg_pipeline_info, item);
		list_item_del(item);
		free(pipe_info->name);
		free(pipe_info);
	}

	free(ctx->tplg_base);
	free(tp->glb_ctx.ctl);
	tb_debug_print("freed all pipelines, widgets, routes and pcms\n");
}

int tb_set_enum_control(struct testbench_prm *tp, struct tb_ctl *ctl, char *control_params)
{
	int control_values[PLATFORM_MAX_CHANNELS];
	char *token, *rest;
	int ret = 0;
	int n = 0;

	rest = control_params;
	while ((token = strtok_r(rest, ",", &rest))) {
		if (n == PLATFORM_MAX_CHANNELS) {
			fprintf(stderr,
				"error: number of control values exceeds max channels count.\n");
			return -EINVAL;
		}

		control_values[n] = tb_decode_enum(&ctl->enum_ctl, token);
		if (control_values[n] < 0) {
			fprintf(stderr,
				"error: not able to decode enum %s.\n", token);
			return -EINVAL;
		}
		n++;
	}

	ret = tb_send_alsa_control(&tp->ipc_tx, &tp->ipc_rx, ctl, control_values, n,
				   SOF_IPC4_ENUM_CONTROL_PARAM_ID);
	return ret;
}

int tb_set_mixer_control(struct testbench_prm *tp, struct tb_ctl *ctl, char *control_params)
{
	int control_values[PLATFORM_MAX_CHANNELS];
	char *token, *rest;
	int n = 0;
	int ret;

	rest = control_params;
	while ((token = strtok_r(rest, ",", &rest))) {
		if (n == PLATFORM_MAX_CHANNELS) {
			fprintf(stderr,
				"error: number of control values exceeds max channels count.\n");
			return -EINVAL;
		}

		if (strcmp(token, "on") == 0)
			control_values[n] = 1;
		else if (strcmp(token, "off") == 0)
			control_values[n] = 0;
		else
			control_values[n] = atoi(token);

		n++;
	}

	if (ctl->mixer_ctl.max == 1)
		ret = tb_send_alsa_control(&tp->ipc_tx, &tp->ipc_rx, ctl, control_values, n,
					   SOF_IPC4_SWITCH_CONTROL_PARAM_ID);
	else
		ret = tb_send_volume_control(&tp->ipc_tx, &tp->ipc_rx, ctl, control_values, n);

	return ret;
}

int tb_set_bytes_control(struct testbench_prm *tp, struct tb_ctl *ctl, uint32_t *data)
{
	return tb_send_bytes_data(&tp->ipc_tx, &tp->ipc_rx,
				  ctl->module_id, ctl->instance_id,
				  (struct sof_abi_hdr *)data);
}

#endif /* CONFIG_IPC_MAJOR_4 */
