// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018-2024 Intel Corporation.

#if CONFIG_IPC_MAJOR_4

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component_ext.h>
#include <sof/lib/notifier.h>
#include <sof/audio/component_ext.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/dp_schedule.h>
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

int tb_configure_audio_params(struct testbench_prm *tp)
{
	int bits;
	int krate;

	if (!tp)
		return -EINVAL;

	if (!tp->fs_in)
		tp->fs_in = 48000;
	if (!tp->fs_out)
		tp->fs_out = tp->fs_in;
	if (!tp->channels_in)
		tp->channels_in = 2;
	if (!tp->channels_out)
		tp->channels_out = tp->channels_in;

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

int tb_setup(struct sof *sof, struct testbench_prm *tp)
{
	struct ll_schedule_domain domain = {0};

	/* init components */
	sys_comp_init(sof);

	/* Module adapter components */
	sys_comp_module_aria_interface_init();
	sys_comp_module_asrc_interface_init();
	sys_comp_module_copier_interface_init();
	sys_comp_module_crossover_interface_init();
	sys_comp_module_dcblock_interface_init();
	sys_comp_module_demux_interface_init();
	sys_comp_module_dolby_dax_audio_processing_interface_init();
	sys_comp_module_drc_interface_init();
	sys_comp_module_eq_fir_interface_init();
	sys_comp_module_eq_iir_interface_init();
	sys_comp_module_file_interface_init();
	sys_comp_module_gain_interface_init();
#if CONFIG_COMP_GOOGLE_CTC_AUDIO_PROCESSING
	sys_comp_module_google_ctc_audio_processing_interface_init();
#endif
	sys_comp_module_google_rtc_audio_processing_interface_init();
	sys_comp_module_igo_nr_interface_init();
	sys_comp_module_level_multiplier_interface_init();
	sys_comp_module_mfcc_interface_init();
	sys_comp_module_mixin_interface_init();
	sys_comp_module_mixout_interface_init();
	sys_comp_module_multiband_drc_interface_init();
	sys_comp_module_mux_interface_init();
	sys_comp_module_phase_vocoder_interface_init();
	sys_comp_module_rtnr_interface_init();
	sys_comp_module_selector_interface_init();
	sys_comp_module_sound_dose_interface_init();
	sys_comp_module_src_interface_init();
	sys_comp_module_src_lite_interface_init();
	sys_comp_module_stft_process_interface_init();
	sys_comp_module_tdfb_interface_init();
	sys_comp_module_template_interface_init();
	sys_comp_module_tone_interface_init();
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

	/* init DP scheduler */
	if (scheduler_dp_init() < 0) {
		fprintf(stderr, "error: dp scheduler init\n");
		return -EINVAL;
	}

	tb_debug_print("ipc and scheduler initialized\n");

	return tb_configure_audio_params(tp);
}

static int tb_prepare_widget(struct testbench_prm *tp,
			     struct tplg_comp_info *comp_info, int dir)
{
	struct tplg_pipeline_list *pipeline_list;
	int ret, i;

	if (dir)
		pipeline_list = &tp->capture_pipeline_list;
	else
		pipeline_list = &tp->playback_pipeline_list;

	if (!comp_info->prepared) {
		/* populate base config */
		ret = tb_set_up_widget_base_config(tp, comp_info);
		if (ret < 0)
			return ret;

		tb_pipeline_update_resource_usage(tp, comp_info);
		comp_info->prepared = true;
	}

	/* add pipeline to pipeline_list if needed */
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

static int tb_prepare_widgets_playback(struct testbench_prm *tp,
				       struct tplg_comp_info *starting_comp_info,
				       struct tplg_comp_info *current_comp_info)
{
	struct list_item *item;
	int ret;

	/* for playback */
	list_for_item(item, &tp->route_list) {
		struct tplg_route_info *route_info = container_of(item, struct tplg_route_info,
								  item);
		bool sink_already_prepared;

		if (route_info->source != current_comp_info)
			continue;

		if (!tb_is_pipeline_enabled(tp, route_info->source->pipeline_id) ||
		    !tb_is_pipeline_enabled(tp, route_info->sink->pipeline_id))
			continue;

		/* set up source widget if it is the starting widget */
		if (starting_comp_info == current_comp_info) {
			ret = tb_prepare_widget(tp, current_comp_info, 0);
			if (ret < 0)
				return ret;
		}

		sink_already_prepared = route_info->sink->prepared;

		/* set up the sink widget */
		ret = tb_prepare_widget(tp, route_info->sink, 0);
		if (ret < 0)
			return ret;

		/* and then continue down the path */
		if (!sink_already_prepared &&
		    route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_IN) {
			ret = tb_prepare_widgets_playback(tp, starting_comp_info,
							  route_info->sink);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int tb_prepare_widgets_capture(struct testbench_prm *tp,
				      struct tplg_comp_info *starting_comp_info,
				      struct tplg_comp_info *current_comp_info)
{
	struct list_item *item;
	int ret;

	/* for capture */
	list_for_item(item, &tp->route_list) {
		struct tplg_route_info *route_info = container_of(item, struct tplg_route_info,
								  item);
		bool source_already_prepared;

		if (route_info->sink != current_comp_info)
			continue;

		if (!tb_is_pipeline_enabled(tp, route_info->source->pipeline_id) ||
		    !tb_is_pipeline_enabled(tp, route_info->sink->pipeline_id))
			continue;

		/* set up sink widget if it is the starting widget */
		if (starting_comp_info == current_comp_info) {
			ret = tb_prepare_widget(tp, current_comp_info, 1);
			if (ret < 0)
				return ret;
		}

		source_already_prepared = route_info->source->prepared;

		/* set up the source widget */
		ret = tb_prepare_widget(tp, route_info->source, 1);
		if (ret < 0)
			return ret;

		/* and then continue up the path */
		if (!source_already_prepared &&
		    route_info->source->type != SND_SOC_TPLG_DAPM_DAI_OUT) {
			ret = tb_prepare_widgets_capture(tp, starting_comp_info,
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

	if (comp_info->set_up)
		return 0;

	pipe_info->usage_count++;

	/* first set up pipeline if needed, only done once for the first pipeline widget */
	if (!pipe_info->set_up) {
		ret = tb_set_up_pipeline(tp, pipe_info);
		if (ret < 0) {
			pipe_info->usage_count--;
			return ret;
		}
		pipe_info->set_up = true;
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
		    ctl->type != SND_SOC_TPLG_TYPE_BYTES ||
		    !ctl->data)
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

	comp_info->set_up = true;

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
		bool sink_already_set_up;

		if (route_info->source != current_comp_info || route_info->set_up)
			continue;

		if (!tb_is_pipeline_enabled(tp, route_info->source->pipeline_id) ||
		    !tb_is_pipeline_enabled(tp, route_info->sink->pipeline_id))
			continue;

		/* set up source widget if it is the starting widget */
		if (starting_comp_info == current_comp_info) {
			ret = tb_set_up_widget(tp, current_comp_info);
			if (ret < 0)
				return ret;
		}

		sink_already_set_up = route_info->sink->set_up;

		/* set up the sink widget */
		ret = tb_set_up_widget(tp, route_info->sink);
		if (ret < 0)
			return ret;

		/* source and sink widgets are up, so set up route now */
		ret = tb_set_up_route(tp, route_info);
		if (ret < 0)
			return ret;

		/* and then continue down the path */
		if (!sink_already_set_up &&
		    route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_IN) {
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

	/* for capture */
	list_for_item(item, &tp->route_list) {
		struct tplg_route_info *route_info = container_of(item, struct tplg_route_info,
								  item);
		bool source_already_set_up;

		if (route_info->sink != current_comp_info || route_info->set_up)
			continue;

		if (!tb_is_pipeline_enabled(tp, route_info->source->pipeline_id) ||
		    !tb_is_pipeline_enabled(tp, route_info->sink->pipeline_id))
			continue;

		/* set up sink widget if it is the starting widget */
		if (starting_comp_info == current_comp_info) {
			ret = tb_set_up_widget(tp, current_comp_info);
			if (ret < 0)
				return ret;
		}

		source_already_set_up = route_info->source->set_up;

		/* set up the source widget */
		ret = tb_set_up_widget(tp, route_info->source);
		if (ret < 0)
			return ret;

		/* source and sink widgets are up, so set up route now */
		ret = tb_set_up_route(tp, route_info);
		if (ret < 0)
			return ret;

		/* and then continue up the path */
		if (!source_already_set_up &&
		    route_info->source->type != SND_SOC_TPLG_DAPM_DAI_OUT) {
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

	/* First pass: prepare widgets and calculate pipeline memory usage across all enabled PCMs */
	list_for_item(item, &tp->pcm_list) {
		pcm_info = container_of(item, struct tplg_pcm_info, item);
		host = dir ? pcm_info->capture_host : pcm_info->playback_host;

		if (!host || !tb_is_pipeline_enabled(tp, host->pipeline_id))
			continue;

		tp->pcm_info = pcm_info;
		if (dir)
			ret = tb_prepare_widgets_capture(tp, host, host);
		else
			ret = tb_prepare_widgets_playback(tp, host, host);
		if (ret < 0)
			return ret;
	}

	/* Second pass: create pipelines, instantiate widgets, and bind routes */
	list_for_item(item, &tp->pcm_list) {
		pcm_info = container_of(item, struct tplg_pcm_info, item);
		host = dir ? pcm_info->capture_host : pcm_info->playback_host;

		if (!host || !tb_is_pipeline_enabled(tp, host->pipeline_id))
			continue;

		tp->pcm_info = pcm_info;
		if (dir)
			ret = tb_set_up_widgets_capture(tp, host, host);
		else
			ret = tb_set_up_widgets_playback(tp, host, host);
		if (ret < 0)
			return ret;
	}

	if (dir)
		tb_debug_print("Setting up capture pipelines complete\n");
	else
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
		if (route_info->source != current_comp_info || !route_info->set_up)
			continue;

		if (!tb_is_pipeline_enabled(tp, route_info->source->pipeline_id) ||
		    !tb_is_pipeline_enabled(tp, route_info->sink->pipeline_id))
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

	/* for capture */
	list_for_item(item, &tp->route_list) {
		route_info = container_of(item, struct tplg_route_info, item);
		if (route_info->sink != current_comp_info || !route_info->set_up)
			continue;

		if (!tb_is_pipeline_enabled(tp, route_info->source->pipeline_id) ||
		    !tb_is_pipeline_enabled(tp, route_info->sink->pipeline_id))
			continue;

		/* Widgets will be freed when the pipeline is deleted, so just unbind modules */
		ret = tb_free_route(tp, route_info);
		if (ret < 0)
			return ret;

		/* and then continue up the path */
		if (route_info->source->type != SND_SOC_TPLG_DAPM_DAI_OUT) {
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

	if (dir)
		pipeline_list = &tp->capture_pipeline_list;
	else
		pipeline_list = &tp->playback_pipeline_list;

	list_for_item(item, &tp->pcm_list) {
		pcm_info = container_of(item, struct tplg_pcm_info, item);
		if (dir)
			host = pcm_info->capture_host;
		else
			host = pcm_info->playback_host;

		if (!host || !tb_is_pipeline_enabled(tp, host->pipeline_id))
			continue;

		if (dir) {
			ret = tb_free_widgets_capture(tp, host, host);
			if (ret < 0) {
				fprintf(stderr, "failed to free widgets for capture PCM\n");
				return ret;
			}
		} else {
			ret = tb_free_widgets_playback(tp, host, host);
			if (ret < 0) {
				fprintf(stderr, "failed to free widgets for playback PCM\n");
				return ret;
			}
		}
	}

	for (i = 0; i < pipeline_list->count; i++) {
		struct tplg_pipeline_info *pipe_info = pipeline_list->pipelines[i];

		if (!pipe_info->set_up)
			continue;

		ret = tb_delete_pipeline(tp, pipe_info);
		if (ret < 0)
			return ret;
		pipe_info->set_up = false;
	}

	pipeline_list->count = 0;
	return 0;
}

int tb_free_all_pipelines(struct testbench_prm *tp)
{
	struct list_item *item;

	tb_debug_print("freeing playback direction\n");
	tb_free_pipelines(tp, SOF_IPC_STREAM_PLAYBACK);

	tb_debug_print("freeing capture direction\n");
	tb_free_pipelines(tp, SOF_IPC_STREAM_CAPTURE);

	/* Ensure any remaining set-up routes or pipelines are cleaned up */
	if (tp->route_list.next) {
		list_for_item(item, &tp->route_list) {
			struct tplg_route_info *route_info =
				container_of(item, struct tplg_route_info, item);

			if (route_info->set_up)
				tb_free_route(tp, route_info);
		}
	}
	if (tp->pipeline_list.next) {
		list_for_item(item, &tp->pipeline_list) {
			struct tplg_pipeline_info *pipe_info =
				container_of(item, struct tplg_pipeline_info, item);

			if (pipe_info->set_up) {
				tb_delete_pipeline(tp, pipe_info);
				pipe_info->set_up = false;
			}
		}
	}

	tp->playback_pipeline_list.count = 0;
	tp->capture_pipeline_list.count = 0;
	memset(tp->instance_ids, 0, sizeof(tp->instance_ids));
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
	uint32_t i;

	if (tp->pcm_list.next) {
		list_for_item_safe(item, _item, &tp->pcm_list) {
			pcm_info = container_of(item, struct tplg_pcm_info, item);
			list_item_del(item);
			free(pcm_info->name);
			free(pcm_info);
		}
	}

	if (tp->widget_list.next) {
		list_for_item_safe(item, _item, &tp->widget_list) {
			comp_info = container_of(item, struct tplg_comp_info, item);
			available_fmts = &comp_info->available_fmt;
			list_item_del(item);
			if (comp_info->pins_info.input_pin_binding) {
				for (i = 0; i < comp_info->pins_info.num_input_pins; i++)
					free(comp_info->pins_info.input_pin_binding[i]);
				free(comp_info->pins_info.input_pin_binding);
			}
			if (comp_info->pins_info.output_pin_binding) {
				for (i = 0; i < comp_info->pins_info.num_output_pins; i++)
					free(comp_info->pins_info.output_pin_binding[i]);
				free(comp_info->pins_info.output_pin_binding);
			}
			free(available_fmts->output_pin_fmts);
			free(available_fmts->input_pin_fmts);
			free(comp_info->name);
			free(comp_info->stream_name);
			free(comp_info->ipc_payload);
			free(comp_info);
		}
	}

	if (tp->route_list.next) {
		list_for_item_safe(item, _item, &tp->route_list) {
			route_info = container_of(item, struct tplg_route_info, item);
			list_item_del(item);
			free(route_info);
		}
	}

	if (tp->pipeline_list.next) {
		list_for_item_safe(item, _item, &tp->pipeline_list) {
			pipe_info = container_of(item, struct tplg_pipeline_info, item);
			list_item_del(item);
			free(pipe_info->name);
			free(pipe_info);
		}
	}

	list_init(&tp->pcm_list);
	list_init(&tp->widget_list);
	list_init(&tp->route_list);
	list_init(&tp->pipeline_list);

	free(ctx->tplg_base);
	ctx->tplg_base = NULL;
	free(tp->glb_ctx.ctl);
	tp->glb_ctx.ctl = NULL;
	tp->glb_ctx.num_ctls = 0;
	tp->glb_ctx.size = 0;
	tp->playback_pipeline_list.count = 0;
	tp->capture_pipeline_list.count = 0;
	memset(tp->instance_ids, 0, sizeof(tp->instance_ids));
	memset(tp->fr, 0, sizeof(tp->fr));
	memset(tp->fw, 0, sizeof(tp->fw));
	for (i = 0; i < TB_MAX_INPUT_FILE_NUM; i++)
		tp->fr[i].id = -1;
	for (i = 0; i < TB_MAX_OUTPUT_FILE_NUM; i++)
		tp->fw[i].id = -1;
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

void tb_collect_heap_usage(struct testbench_prm *tp, struct tb_heap_usage_record *records,
			   int *count_out)
{
	struct list_item *item;
	size_t hwm;
	int count = 0;

	if (tp && tp->widget_list.next && !list_is_empty(&tp->widget_list)) {
		list_for_item(item, &tp->widget_list) {
			struct tplg_comp_info *info = container_of(item, struct tplg_comp_info, item);
			uint32_t comp_id = IPC4_COMP_ID(info->module_id, info->instance_id);
			struct comp_dev *dev = ipc4_get_comp_dev(comp_id);

			if (!dev || !dev->mod)
				continue;

			/* In testbench environment, skip AIF/DAI because they are not real components. */
			if (info->type == SND_SOC_TPLG_DAPM_AIF_IN ||
			    info->type == SND_SOC_TPLG_DAPM_AIF_OUT ||
			    info->type == SND_SOC_TPLG_DAPM_DAI_IN ||
			    info->type == SND_SOC_TPLG_DAPM_DAI_OUT)
				continue;

			if (count >= TB_NUM_WIDGETS_SUPPORTED) {
				fprintf(stderr, "Error: Too many components for heap records, max %d.\n",
					TB_NUM_WIDGETS_SUPPORTED);
				break;
			}

			module_adapter_heap_usage(dev->mod, &hwm);
			records[count].module_name = info->name;
			records[count].heap_max = hwm;
			count++;
		}
	} else if (sof_get() && sof_get()->ipc && sof_get()->ipc->comp_list.next) {
		list_for_item(item, &sof_get()->ipc->comp_list) {
			struct ipc_comp_dev *icd = container_of(item, struct ipc_comp_dev, list);
			uint32_t mid;

			if (icd->type != COMP_TYPE_COMPONENT || !icd->cd || !icd->cd->mod)
				continue;
			mid = IPC4_MOD_ID(icd->id);
			if (mid == TB_FILE_OUT_AIF_MODULE_ID ||
			    mid == TB_FILE_IN_AIF_MODULE_ID ||
			    mid == TB_FILE_OUT_DAI_MODULE_ID ||
			    mid == TB_FILE_IN_DAI_MODULE_ID)
				continue;
			if (count >= TB_NUM_WIDGETS_SUPPORTED)
				break;
			module_adapter_heap_usage(icd->cd->mod, &hwm);
			records[count].module_name =
				(mid == TB_COPIER_MODULE_ID)  ? "module-copier" :
				(mid == TB_MIXIN_MODULE_ID)   ? "mixin" :
				(mid == TB_MIXOUT_MODULE_ID)  ? "mixout" :
				(mid == TB_PGA_MODULE_ID)     ? "gain" :
				(mid == TB_SRC_MODULE_ID)     ? "src" :
				(mid == TB_ASRC_MODULE_ID)    ? "asrc" :
				(mid == TB_PROCESS_MODULE_ID) ? "process" : "module";
			records[count].heap_max = hwm;
			count++;
		}
	}

	*count_out = count;
}

#endif /* CONFIG_IPC_MAJOR_4 */
