// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018-2024 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//	   Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <ipc4/header.h>
#include <tplg_parser/tokens.h>
#include <tplg_parser/topology.h>
#include <stdio.h>
#include <stdlib.h>

#include "testbench/common_test.h"
#include "testbench/topology.h"
#include "testbench/topology_ipc4.h"
#include "testbench/file.h"
#include "testbench/file_ipc4.h"
#include "ipc4/pipeline.h"

#define SOF_IPC4_FW_PAGE(x) ((((x) + BIT(12) - 1) & ~(BIT(12) - 1)) >> 12)
#define SOF_IPC4_FW_ROUNDUP(x) (((x) + BIT(6) - 1) & (~(BIT(6) - 1)))
#define SOF_IPC4_MODULE_INSTANCE_LIST_ITEM_SIZE 12
#define SOF_IPC4_PIPELINE_OBJECT_SIZE 448
#define SOF_IPC4_DATA_QUEUE_OBJECT_SIZE 128
#define SOF_IPC4_LL_TASK_OBJECT_SIZE 72
#define SOF_IPC4_LL_TASK_LIST_ITEM_SIZE 12
#define SOF_IPC4_FW_MAX_QUEUE_COUNT 8

static const struct sof_topology_token ipc4_comp_tokens[] = {
	{SOF_TKN_COMP_IS_PAGES, SND_SOC_TPLG_TUPLE_TYPE_WORD, tplg_token_get_uint32_t,
		offsetof(struct ipc4_base_module_cfg, is_pages)},
};

/*
 * IPC
 */

static int tb_ipc_message(void *mailbox, size_t bytes)
{
	struct ipc *ipc = ipc_get();

	/* reply is copied back to mailbox */
	memcpy(ipc->comp_data, mailbox, bytes);
	ipc_cmd(mailbox);
	memcpy(mailbox, ipc->comp_data, bytes);

	return 0;
}

static int tb_mq_cmd_tx_rx(struct tb_mq_desc *ipc_tx, struct tb_mq_desc *ipc_rx,
			   void *msg, size_t len, void *reply, size_t rlen)
{
	char mailbox[IPC4_MAX_MSG_SIZE];
	struct ipc4_message_reply *reply_msg = reply;


	if (len > IPC4_MAX_MSG_SIZE || rlen > IPC4_MAX_MSG_SIZE) {
		fprintf(stderr, "ipc: message too big len=%ld rlen=%ld\n", len, rlen);
		return -EINVAL;
	}

#if TB_FAKE_IPC
	reply_msg->primary.r.status = IPC4_SUCCESS;
#else

	memset(mailbox, 0, IPC4_MAX_MSG_SIZE);
	memcpy(mailbox, msg, len);
	tb_ipc_message(mailbox, len);

	memcpy(reply, mailbox, rlen);
#endif

	if (reply_msg->primary.r.status != IPC4_SUCCESS)
		return -EINVAL;

	return 0;
}

int tb_parse_ipc4_comp_tokens(struct testbench_prm *tp,
				     struct ipc4_base_module_cfg *base_cfg)
{
	struct tplg_context *ctx = &tp->tplg;
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	struct snd_soc_tplg_vendor_array *array = &ctx->widget->priv.array[0];
	int size = ctx->widget->priv.size;
	int ret;

	ret = sof_parse_token_sets(base_cfg, ipc4_comp_tokens, ARRAY_SIZE(ipc4_comp_tokens),
				   array, size, 1, 0);
	if (ret < 0)
		return ret;

	return sof_parse_tokens(&comp_info->uuid, comp_ext_tokens,
			       ARRAY_SIZE(comp_ext_tokens), array, size);
}

void tb_setup_widget_ipc_msg(struct tplg_comp_info *comp_info)
{
	struct ipc4_module_init_instance *module_init = &comp_info->module_init;

	module_init->primary.r.type = SOF_IPC4_MOD_INIT_INSTANCE;
	module_init->primary.r.module_id = comp_info->module_id;
	module_init->primary.r.instance_id = comp_info->instance_id;
	module_init->primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_MODULE_MSG;
	module_init->primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
}

int tb_set_up_widget_ipc(struct testbench_prm *tb, struct tplg_comp_info *comp_info)
{
	struct ipc4_module_init_instance *module_init = &comp_info->module_init;
	struct ipc4_message_reply reply;
	void *msg;
	int size;
	int ret = 0;

	module_init->extension.r.param_block_size = comp_info->ipc_size >> 2;
	module_init->extension.r.ppl_instance_id = comp_info->pipe_info->instance_id;

	size = sizeof(*module_init) + comp_info->ipc_size;
	msg = calloc(size, 1);
	if (!msg)
		return -ENOMEM;

	memcpy(msg, module_init, sizeof(*module_init));
	memcpy(msg + sizeof(*module_init), comp_info->ipc_payload, comp_info->ipc_size);

	// TODO
	ret = tb_mq_cmd_tx_rx(&tb->ipc_tx, &tb->ipc_rx, msg, size, &reply, sizeof(reply));

	free(msg);
	if (ret < 0) {
		fprintf(stderr, "error: can't set up widget %s\n", comp_info->name);
		return ret;
	}

	if (reply.primary.r.status != IPC4_SUCCESS) {
		fprintf(stderr, "widget %s set up failed with status %d\n",
			comp_info->name, reply.primary.r.status);
		return -EINVAL;
	}
	return 0;
}

int tb_set_up_route(struct testbench_prm *tb, struct tplg_route_info *route_info)
{
	struct tplg_comp_info *src_comp_info = route_info->source;
	struct tplg_comp_info *sink_comp_info = route_info->sink;
	struct ipc4_module_bind_unbind bu;
	struct ipc4_message_reply reply;
	int ret;

	bu.primary.r.module_id = src_comp_info->module_id;
	bu.primary.r.instance_id = src_comp_info->instance_id;
	bu.primary.r.type = SOF_IPC4_MOD_BIND;
	bu.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_MODULE_MSG;
	bu.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;

	bu.extension.r.dst_module_id = sink_comp_info->module_id;
	bu.extension.r.dst_instance_id = sink_comp_info->instance_id;

	/* FIXME: assign queue ID for components with multiple inputs/outputs */
	bu.extension.r.dst_queue = 0;
	bu.extension.r.src_queue = 0;

	ret = tb_mq_cmd_tx_rx(&tb->ipc_tx, &tb->ipc_rx, &bu, sizeof(bu), &reply, sizeof(reply));
	if (ret < 0) {
		fprintf(stderr, "error: can't set up route %s -> %s\n", src_comp_info->name,
			sink_comp_info->name);
		return ret;
	}

	if (reply.primary.r.status != IPC4_SUCCESS) {
		fprintf(stderr, "route %s -> %s ID set up failed with status %d\n",
			src_comp_info->name, sink_comp_info->name, reply.primary.r.status);
		return -EINVAL;
	}

	tplg_debug("route %s -> %s set up\n", src_comp_info->name, sink_comp_info->name);

	return 0;
}

int tb_set_up_pipeline(struct testbench_prm *tb, struct tplg_pipeline_info *pipe_info)
{
	struct ipc4_pipeline_create msg = {.primary.dat = 0, .extension.dat = 0};
	struct ipc4_message_reply reply;
	int ret;

	msg.primary.r.type = SOF_IPC4_GLB_CREATE_PIPELINE;
	msg.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
	msg.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	pipe_info->instance_id = tb->instance_ids[SND_SOC_TPLG_DAPM_SCHEDULER]++;
	msg.primary.r.instance_id = pipe_info->instance_id;
	msg.primary.r.ppl_mem_size = pipe_info->mem_usage;

	// TODO
	ret = tb_mq_cmd_tx_rx(&tb->ipc_tx, &tb->ipc_rx, &msg, sizeof(msg), &reply, sizeof(reply));
	if (ret < 0) {
		fprintf(stderr, "error: can't set up pipeline %s\n", pipe_info->name);
		return ret;
	}

	if (reply.primary.r.status != IPC4_SUCCESS) {
		fprintf(stderr, "pipeline %s instance ID %d set up failed with status %d\n",
			pipe_info->name, pipe_info->instance_id, reply.primary.r.status);
		return -EINVAL;
	}

	tplg_debug("pipeline %s instance_id %d mem_usage %d set up\n", pipe_info->name,
		   pipe_info->instance_id, pipe_info->mem_usage);

	return 0;
}

void tb_pipeline_update_resource_usage(struct testbench_prm *tb,
					      struct tplg_comp_info *comp_info)
{
	struct ipc4_base_module_cfg *base_config = &comp_info->basecfg;
	struct tplg_pipeline_info *pipe_info = comp_info->pipe_info;
	int task_mem, queue_mem;
	int ibs, bss, total;

	ibs = base_config->ibs;
	bss = base_config->is_pages;

	task_mem = SOF_IPC4_PIPELINE_OBJECT_SIZE;
	task_mem += SOF_IPC4_MODULE_INSTANCE_LIST_ITEM_SIZE + bss;

	/* LL modules */
	task_mem += SOF_IPC4_FW_ROUNDUP(SOF_IPC4_LL_TASK_OBJECT_SIZE);
	task_mem += SOF_IPC4_FW_MAX_QUEUE_COUNT * SOF_IPC4_MODULE_INSTANCE_LIST_ITEM_SIZE;
	task_mem += SOF_IPC4_LL_TASK_LIST_ITEM_SIZE;

	ibs = SOF_IPC4_FW_ROUNDUP(ibs);
	queue_mem = SOF_IPC4_FW_MAX_QUEUE_COUNT * (SOF_IPC4_DATA_QUEUE_OBJECT_SIZE +  ibs);

	total = SOF_IPC4_FW_PAGE(task_mem + queue_mem);

	pipe_info->mem_usage += total;
}

/*
 * IPC
 */

int tb_is_single_format(struct sof_ipc4_pin_format *fmts, int num_formats)
{
	struct sof_ipc4_pin_format *fmt = &fmts[0];
	uint32_t _rate, _channels, _valid_bits;
	int i;

	if (!fmt) {
		fprintf(stderr, "Error: Null fmt\n");
		return false;
	}

	_rate = fmt->audio_fmt.sampling_frequency;
	_channels = fmt->audio_fmt.fmt_cfg & MASK(7, 0);
	_valid_bits = (fmt->audio_fmt.fmt_cfg & MASK(15, 8)) >> 8;
	for (i = 1; i < num_formats; i++) {
		struct sof_ipc4_pin_format *fmt = &fmts[i];
		uint32_t rate, channels, valid_bits;

		rate = fmt->audio_fmt.sampling_frequency;
		channels = fmt->audio_fmt.fmt_cfg & MASK(7, 0);
		valid_bits = (fmt->audio_fmt.fmt_cfg & MASK(15, 8)) >> 8;
		if (rate != _rate || channels != _channels || valid_bits != _valid_bits)
			return false;
	}

	return true;
}

int tb_match_audio_format(struct testbench_prm *tb, struct tplg_comp_info *comp_info,
				 struct tb_config *config)
{
	struct sof_ipc4_available_audio_format *available_fmt = &comp_info->available_fmt;
	struct ipc4_base_module_cfg *base_cfg = &comp_info->basecfg;
	struct sof_ipc4_pin_format *fmt;
	int config_valid_bits;
	int i;

	switch (config->format) {
	case SOF_IPC_FRAME_S16_LE:
		config_valid_bits = 16;
		break;
	case SOF_IPC_FRAME_S32_LE:
		config_valid_bits = 32;
		break;
	case SOF_IPC_FRAME_S24_4LE:
		config_valid_bits = 24;
		break;
	default:
		break;
	}

	if (tb_is_single_format(available_fmt->input_pin_fmts,
				available_fmt->num_input_formats)) {
		fmt = &available_fmt->input_pin_fmts[0];
		goto out;
	}

	for (i = 0; i < available_fmt->num_input_formats; i++) {
		uint32_t rate, channels, valid_bits;

		fmt = &available_fmt->input_pin_fmts[i];

		rate = fmt->audio_fmt.sampling_frequency;
		channels = fmt->audio_fmt.fmt_cfg & MASK(7, 0);
		valid_bits = (fmt->audio_fmt.fmt_cfg & MASK(15, 8)) >> 8;

		if (rate == config->rate && channels == config->channels &&
		    valid_bits == config_valid_bits)
			break;
	}

	if (i == available_fmt->num_input_formats) {
		fprintf(stderr,
			"Cannot find matching format for rate %d channels %d valid_bits %d for %s\n",
		       config->rate, config->channels, config_valid_bits, comp_info->name);
		return -EINVAL;
	}
out:

	base_cfg->audio_fmt.sampling_frequency = fmt->audio_fmt.sampling_frequency;
	base_cfg->audio_fmt.depth = fmt->audio_fmt.bit_depth;
	base_cfg->audio_fmt.ch_map = fmt->audio_fmt.ch_map;
	base_cfg->audio_fmt.ch_cfg = fmt->audio_fmt.ch_cfg;
	base_cfg->audio_fmt.interleaving_style = fmt->audio_fmt.interleaving_style;
	base_cfg->audio_fmt.channels_count = fmt->audio_fmt.fmt_cfg & MASK(7, 0);
	base_cfg->audio_fmt.valid_bit_depth =
		(fmt->audio_fmt.fmt_cfg & MASK(15, 8)) >> 8;
	base_cfg->audio_fmt.s_type =
		(fmt->audio_fmt.fmt_cfg & MASK(23, 16)) >> 16;
	base_cfg->ibs = tb->period_size * 2;
	base_cfg->obs = tb->period_size * 2;

	return 0;
}

int tb_set_up_widget_base_config(struct testbench_prm *tb, struct tplg_comp_info *comp_info)
{
	char *config_name = "48k2c32b"; // TODO
	int num_configs = 1; // TODO
	struct tb_config *config;
	bool config_found = false;
	int ret, i;

	for (i = 0; i < num_configs; i++) {
		config = &tb->config[i];

		if (!strcmp(config->name, config_name)) {
			config_found = true;
			break;
		}
	}

	if (!config_found) {
		fprintf(stderr, "unsupported config requested %s\n", config_name);
		return -ENOTSUP;
	}

	/* match audio formats and populate base config */
	ret = tb_match_audio_format(tb, comp_info, config);
	if (ret < 0)
		return ret;

	/* copy the basecfg into the ipc payload */
	memcpy(comp_info->ipc_payload, &comp_info->basecfg, sizeof(struct ipc4_base_module_cfg));

	return 0;
}

static int tb_pipeline_set_state(struct testbench_prm *tb, int state,
				 struct ipc4_pipeline_set_state *pipe_state,
				 struct tplg_pipeline_info *pipe_info,
				 struct tb_mq_desc *ipc_tx, struct tb_mq_desc *ipc_rx)
{
	struct ipc4_message_reply reply = {{ 0 }};
	int ret;

	pipe_state->primary.r.ppl_id = pipe_info->instance_id;

	ret = tb_mq_cmd_tx_rx(ipc_tx, ipc_rx, pipe_state, sizeof(*pipe_state),
			      &reply, sizeof(reply));
	if (ret < 0)
		fprintf(stderr, "failed pipeline %d set state %d\n", pipe_info->instance_id, state);

	return ret;
}

int tb_pipelines_set_state(struct testbench_prm *tb, int state, int dir)
{
	struct ipc4_pipeline_set_state pipe_state = {{ 0 }};
	struct tplg_pipeline_list *pipeline_list;
	struct tplg_pipeline_info *pipe_info;
	int ret;
	int i;

	if (dir == SOF_IPC_STREAM_CAPTURE)
		pipeline_list = &tb->pcm_info->capture_pipeline_list;
	else
		pipeline_list = &tb->pcm_info->playback_pipeline_list;

	pipe_state.primary.r.ppl_state = state;
	pipe_state.primary.r.type = SOF_IPC4_GLB_SET_PIPELINE_STATE;
	pipe_state.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
	pipe_state.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;

	/*
	 * pipeline list is populated starting from the host to DAI. So traverse the list in
	 * the reverse order for capture to start the source pipeline first.
	 */
	if (dir == SOF_IPC_STREAM_CAPTURE) {
		for (i = pipeline_list->count - 1; i >= 0; i--) {
			pipe_info = pipeline_list->pipelines[i];
			ret = tb_pipeline_set_state(tb, state, &pipe_state, pipe_info,
						    &tb->ipc_tx, &tb->ipc_rx);
			if (ret < 0)
				return ret;
		}

		return 0;
	}

	for (i = 0; i < pipeline_list->count; i++) {
		pipe_info = pipeline_list->pipelines[i];
		ret = tb_pipeline_set_state(tb, state, &pipe_state, pipe_info,
					    &tb->ipc_tx, &tb->ipc_rx);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/*
 * Topology widgets
 */

int tb_new_aif_in_out(struct testbench_prm *tb, int dir)
{
	struct tplg_context *ctx = &tb->tplg;
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	struct ipc4_file_module_cfg *file;
	int ret;

	ret = tplg_parse_widget_audio_formats(ctx);
	if (ret < 0)
		return ret;

	comp_info->ipc_payload =  calloc(sizeof(struct ipc4_file_module_cfg), 1);
	if (!comp_info->ipc_payload)
		return -ENOMEM;

	comp_info->ipc_size = sizeof(struct ipc4_file_module_cfg);

	if (dir == SOF_IPC_STREAM_PLAYBACK) {
		/* Set from testbench command line*/
		file = (struct ipc4_file_module_cfg *)comp_info->ipc_payload;
		file->config.mode = FILE_READ;
		file->config.rate = tb->fs_in;
		file->config.channels = tb->channels_in;
		file->config.frame_fmt = tb->frame_fmt;
		file->config.direction = dir;
		if (tb->input_file_index >= tb->input_file_num) {
			fprintf(stderr, "error: not enough input files\n");
			return -EINVAL;
		}

		file->config.fn = tb->input_file[tb->input_file_index];
		comp_info->instance_id = tb->instance_ids[SND_SOC_TPLG_DAPM_AIF_IN]++;
		comp_info->module_id = 0x9a;
		tb->fr[tb->input_file_index].id = comp_info->module_id;
		tb->fr[tb->input_file_index].instance_id = comp_info->instance_id;
		tb->fr[tb->input_file_index].pipeline_id = ctx->pipeline_id;
		tb->input_file_index++;
		tb_setup_widget_ipc_msg(comp_info);
	} else {
		file = (struct ipc4_file_module_cfg *)comp_info->ipc_payload;
		file->config.mode = FILE_WRITE;
		file->config.rate = tb->fs_out;
		file->config.channels = tb->channels_out;
		file->config.frame_fmt = tb->frame_fmt;
		file->config.direction = dir;
		if (tb->output_file_index >= tb->output_file_num) {
			fprintf(stderr, "error: not enough output files\n");
			return -EINVAL;
		}

		file->config.fn = tb->output_file[tb->output_file_index];
		comp_info->instance_id = tb->instance_ids[SND_SOC_TPLG_DAPM_AIF_OUT]++;
		comp_info->module_id = 0x9b;
		tb->fw[tb->output_file_index].id = comp_info->module_id;
		tb->fw[tb->output_file_index].instance_id = comp_info->instance_id;
		tb->fw[tb->output_file_index].pipeline_id = ctx->pipeline_id;
		tb->output_file_index++;
		tb_setup_widget_ipc_msg(comp_info);
	}

	return 0;
}

int tb_new_dai_in_out(struct testbench_prm *tb, int dir)
{
	struct tplg_context *ctx = &tb->tplg;
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	struct ipc4_file_module_cfg *file;
	int ret;

	ret = tplg_parse_widget_audio_formats(ctx);
	if (ret < 0)
		return ret;

	comp_info->ipc_payload =  calloc(sizeof(struct ipc4_file_module_cfg), 1);
	if (!comp_info->ipc_payload)
		return -ENOMEM;

	comp_info->ipc_size = sizeof(struct ipc4_file_module_cfg);

	if (dir == SOF_IPC_STREAM_PLAYBACK) {
		/* Set from testbench command line*/
		file = (struct ipc4_file_module_cfg *)comp_info->ipc_payload;
		file->config.mode = FILE_WRITE;
		file->config.rate = tb->fs_out;
		file->config.channels = tb->channels_out;
		file->config.frame_fmt = tb->frame_fmt;
		file->config.direction = dir;
		if (tb->output_file_index >= tb->output_file_num) {
			fprintf(stderr, "error: not enough output files\n");
			return -EINVAL;
		}

		file->config.fn = tb->output_file[tb->output_file_index];
		comp_info->instance_id = tb->instance_ids[SND_SOC_TPLG_DAPM_DAI_OUT]++;
		comp_info->module_id = 0x9c;
		tb->fw[tb->output_file_index].id = comp_info->module_id;
		tb->fw[tb->output_file_index].instance_id = comp_info->instance_id;
		tb->fw[tb->output_file_index].pipeline_id = ctx->pipeline_id;
		tb->output_file_index++;
		tb_setup_widget_ipc_msg(comp_info);
	} else {
		file = (struct ipc4_file_module_cfg *)comp_info->ipc_payload;
		file->config.mode = FILE_READ;
		file->config.rate = tb->fs_in;
		file->config.channels = tb->channels_in;
		file->config.frame_fmt = tb->frame_fmt;
		file->config.direction = dir;
		if (tb->input_file_index >= tb->input_file_num) {
			fprintf(stderr, "error: not enough input files\n");
			return -EINVAL;
		}

		file->config.fn = tb->input_file[tb->input_file_index];
		comp_info->instance_id = tb->instance_ids[SND_SOC_TPLG_DAPM_DAI_IN]++;
		comp_info->module_id = 0x9d;
		tb->fr[tb->input_file_index].id = comp_info->module_id;
		tb->fr[tb->input_file_index].instance_id = comp_info->instance_id;
		tb->fr[tb->input_file_index].pipeline_id = ctx->pipeline_id;
		tb->input_file_index++;
		tb_setup_widget_ipc_msg(comp_info);
	}

	return 0;
}

int tb_new_pga(struct testbench_prm *tb)
{
	struct tplg_context *ctx = &tb->tplg;
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	struct ipc4_peak_volume_config volume;
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	int ret;

	comp_info->ipc_size =
		sizeof(struct ipc4_peak_volume_config) + sizeof(struct ipc4_base_module_cfg);
	comp_info->ipc_payload = calloc(comp_info->ipc_size, 1);
	if (!comp_info->ipc_payload)
		return -ENOMEM;

	/* FIXME: move this to when the widget is actually set up */
	comp_info->instance_id = tb->instance_ids[SND_SOC_TPLG_DAPM_PGA]++;
	comp_info->module_id = 0x6;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl) {
		free(comp_info->ipc_payload);
		return -ENOMEM;
	}

	ret = tplg_new_pga(ctx, &volume, sizeof(struct ipc4_peak_volume_config),
			   tplg_ctl, ctx->hdr->payload_size);
	if (ret < 0) {
		fprintf(stderr, "error: failed to create PGA\n");
		goto out;
	}

	/* copy volume data to ipc_payload */
	memcpy(comp_info->ipc_payload + sizeof(struct ipc4_base_module_cfg),
	       &volume, sizeof(struct ipc4_peak_volume_config));

	/* skip kcontrols for now */
	ret = tplg_create_controls(ctx, ctx->widget->num_kcontrols, tplg_ctl,
				   ctx->hdr->payload_size, &volume);
	if (ret < 0) {
		fprintf(stderr, "error: loading controls\n");
		goto out;
	}

	tb_setup_widget_ipc_msg(comp_info);
	free(tplg_ctl);
	return ret;

out:
	free(tplg_ctl);
	free(comp_info->ipc_payload);
	return ret;
}

int tb_new_process(struct testbench_prm *tb)
{
	struct tplg_context *ctx = &tb->tplg;
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	int ret;

	ret = tplg_parse_widget_audio_formats(ctx);
	if (ret < 0)
		return ret;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl) {
		free(comp_info->ipc_payload);
		return -ENOMEM;
	}

	/* only base config supported for now. extn support will be added later */
	comp_info->ipc_size = sizeof(struct ipc4_base_module_cfg);
	comp_info->ipc_payload = calloc(comp_info->ipc_size, 1);
	if (!comp_info->ipc_payload)
		return -ENOMEM;

	/* FIXME: move this to when the widget is actually set up */
	comp_info->instance_id = tb->instance_ids[SND_SOC_TPLG_DAPM_EFFECT]++;
	comp_info->module_id = 0x9e; /* dcblock */

	/* skip kcontrols for now, set object to NULL */
	ret = tplg_create_controls(ctx, ctx->widget->num_kcontrols, tplg_ctl,
				   ctx->hdr->payload_size, NULL);
	if (ret < 0) {
		fprintf(stderr, "error: loading controls\n");
		goto out;
	}

	tb_setup_widget_ipc_msg(comp_info);
	return 0;

out:
	free(tplg_ctl);
	free(comp_info->ipc_payload);
	return ret;

}

/*
 * To run
 */

int tb_set_running_state(struct testbench_prm *tb)
{
	int ret;

	ret = tb_pipelines_set_state(tb, SOF_IPC4_PIPELINE_STATE_PAUSED, SOF_IPC_STREAM_PLAYBACK);
	if (ret) {
		fprintf(stderr, "error: failed to set state to paused\n");
		return ret;
	}

	ret = tb_pipelines_set_state(tb, SOF_IPC4_PIPELINE_STATE_PAUSED, SOF_IPC_STREAM_CAPTURE);
	if (ret) {
		fprintf(stderr, "error: failed to set state to paused\n");
		return ret;
	}

	fprintf(stdout, "pipelines are set to paused state\n");
	ret = tb_pipelines_set_state(tb, SOF_IPC4_PIPELINE_STATE_RUNNING, SOF_IPC_STREAM_PLAYBACK);
	if (ret) {
		fprintf(stderr, "error: failed to set state to running\n");
		return ret;
	}

	ret = tb_pipelines_set_state(tb, SOF_IPC4_PIPELINE_STATE_RUNNING, SOF_IPC_STREAM_CAPTURE);
	if (ret)
		fprintf(stderr, "error: failed to set state to running\n");

	fprintf(stdout, "pipelines are set to running state\n");
	return ret;
}

/*
 * To stop
 */

int tb_set_reset_state(struct testbench_prm *tb)
{
	int ret;

	ret = tb_pipelines_set_state(tb, SOF_IPC4_PIPELINE_STATE_PAUSED, SOF_IPC_STREAM_PLAYBACK);
	if (ret) {
		fprintf(stderr, "error: failed to set state to paused\n");
		return ret;
	}

	tb_schedule_pipeline_check_state(tb);

	ret = tb_pipelines_set_state(tb, SOF_IPC4_PIPELINE_STATE_RESET, SOF_IPC_STREAM_PLAYBACK);
	if (ret) {
		fprintf(stderr, "error: failed to set state to reset\n");
		return ret;
	}

	tb_schedule_pipeline_check_state(tb);

	ret = tb_pipelines_set_state(tb, SOF_IPC4_PIPELINE_STATE_PAUSED, SOF_IPC_STREAM_CAPTURE);
	if (ret) {
		fprintf(stderr, "error: failed to set state to paused\n");
		return ret;
	}

	tb_schedule_pipeline_check_state(tb);

	ret = tb_pipelines_set_state(tb, SOF_IPC4_PIPELINE_STATE_RESET, SOF_IPC_STREAM_CAPTURE);
	if (ret)
		fprintf(stderr, "error: failed to set state to reset\n");

	tb_schedule_pipeline_check_state(tb);
	return ret;
}

int tb_delete_pipeline(struct testbench_prm *tb, struct tplg_pipeline_info *pipe_info)
{
	struct ipc4_pipeline_delete msg;
	struct ipc4_message_reply reply;
	int ret;

	msg.primary.r.type = SOF_IPC4_GLB_DELETE_PIPELINE;
	msg.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
	msg.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	msg.primary.r.instance_id = pipe_info->instance_id;

	ret = tb_mq_cmd_tx_rx(&tb->ipc_tx, &tb->ipc_rx, &msg, sizeof(msg), &reply, sizeof(reply));
	if (ret < 0) {
		fprintf(stderr, "error: can't delete pipeline %s\n", pipe_info->name);
		return ret;
	}

	if (reply.primary.r.status != IPC4_SUCCESS) {
		fprintf(stderr, "pipeline %s instance ID %d delete failed with status %d\n",
			pipe_info->name, pipe_info->instance_id, reply.primary.r.status);
		return -EINVAL;
	}

	tplg_debug("pipeline %s instance_id %d freed\n", pipe_info->name,
		   pipe_info->instance_id);

	return 0;
}

int tb_free_route(struct testbench_prm *tb, struct tplg_route_info *route_info)
{
	struct tplg_comp_info *src_comp_info = route_info->source;
	struct tplg_comp_info *sink_comp_info = route_info->sink;
	struct ipc4_module_bind_unbind bu;
	struct ipc4_message_reply reply;
	int ret;

	/* only unbind when widgets belong to separate pipelines */
	if (src_comp_info->pipeline_id == sink_comp_info->pipeline_id)
		return 0;

	bu.primary.r.module_id = src_comp_info->module_id;
	bu.primary.r.instance_id = src_comp_info->instance_id;
	bu.primary.r.type = SOF_IPC4_MOD_UNBIND;
	bu.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_MODULE_MSG;
	bu.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;

	bu.extension.r.dst_module_id = sink_comp_info->module_id;
	bu.extension.r.dst_instance_id = sink_comp_info->instance_id;

	/* FIXME: assign queue ID for components with multiple inputs/outputs */
	bu.extension.r.dst_queue = 0;
	bu.extension.r.src_queue = 0;

	ret = tb_mq_cmd_tx_rx(&tb->ipc_tx, &tb->ipc_rx, &bu, sizeof(bu), &reply, sizeof(reply));
	if (ret < 0) {
		fprintf(stderr, "error: can't set up route %s -> %s\n", src_comp_info->name,
			sink_comp_info->name);
		return ret;
	}

	if (reply.primary.r.status != IPC4_SUCCESS) {
		fprintf(stderr, "route %s -> %s ID set up failed with status %d\n",
			src_comp_info->name, sink_comp_info->name, reply.primary.r.status);
		return -EINVAL;
	}

	tplg_debug("route %s -> %s freed\n", src_comp_info->name, sink_comp_info->name);

	return 0;
}
