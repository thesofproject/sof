// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018-2024 Intel Corporation,
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//	   Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#if CONFIG_IPC_MAJOR_4

#include <ipc4/header.h>
#include <kernel/header.h>
#include <tplg_parser/tokens.h>
#include <tplg_parser/topology.h>
#include <module/ipc4/base-config.h>
#include <stdio.h>
#include <stdlib.h>

#include "testbench/utils.h"
#include "testbench/topology_ipc4.h"
#include "testbench/file.h"
#include "testbench/file_ipc4.h"
#include "testbench/trace.h"
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
	char mailbox[TB_IPC4_MAX_MSG_SIZE];
	struct ipc4_message_reply *reply_msg = reply;

	if (len > TB_IPC4_MAX_MSG_SIZE || rlen > TB_IPC4_MAX_MSG_SIZE) {
		fprintf(stderr, "ipc: message too big len=%zu rlen=%zu\n", len, rlen);
		return -EINVAL;
	}

	memset(mailbox, 0, TB_IPC4_MAX_MSG_SIZE);
	memcpy(mailbox, msg, len);
	tb_ipc_message(mailbox, len);
	memcpy(reply, mailbox, rlen);
	if (reply_msg->primary.r.status != IPC4_SUCCESS)
		return -EINVAL;

	return 0;
}

static int tb_parse_ipc4_comp_tokens(struct testbench_prm *tp,
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

static void tb_setup_widget_ipc_msg(struct tplg_comp_info *comp_info)
{
	struct ipc4_module_init_instance *module_init = &comp_info->module_init;

	module_init->primary.r.type = SOF_IPC4_MOD_INIT_INSTANCE;
	module_init->primary.r.module_id = comp_info->module_id;
	module_init->primary.r.instance_id = comp_info->instance_id;
	module_init->primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_MODULE_MSG;
	module_init->primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
}

int tb_set_up_widget_ipc(struct testbench_prm *tp, struct tplg_comp_info *comp_info)
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
	ret = tb_mq_cmd_tx_rx(&tp->ipc_tx, &tp->ipc_rx, msg, size, &reply, sizeof(reply));

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

int tb_set_up_route(struct testbench_prm *tp, struct tplg_route_info *route_info)
{
	struct tplg_comp_info *src_comp_info = route_info->source;
	struct tplg_comp_info *sink_comp_info = route_info->sink;
	struct ipc4_module_bind_unbind bu = {{0}};
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

	ret = tb_mq_cmd_tx_rx(&tp->ipc_tx, &tp->ipc_rx, &bu, sizeof(bu), &reply, sizeof(reply));
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

int tb_set_up_pipeline(struct testbench_prm *tp, struct tplg_pipeline_info *pipe_info)
{
	struct ipc4_pipeline_create msg = {.primary.dat = 0, .extension.dat = 0};
	struct ipc4_message_reply reply;
	int ret;

	msg.primary.r.type = SOF_IPC4_GLB_CREATE_PIPELINE;
	msg.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
	msg.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	pipe_info->instance_id = tp->instance_ids[SND_SOC_TPLG_DAPM_SCHEDULER]++;
	msg.primary.r.instance_id = pipe_info->instance_id;
	msg.primary.r.ppl_mem_size = pipe_info->mem_usage;
	ret = tb_mq_cmd_tx_rx(&tp->ipc_tx, &tp->ipc_rx, &msg, sizeof(msg), &reply, sizeof(reply));
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

void tb_pipeline_update_resource_usage(struct testbench_prm *tp,
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

int tb_match_audio_format(struct testbench_prm *tp, struct tplg_comp_info *comp_info,
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
		return -EINVAL;
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
	base_cfg->ibs = tp->period_size * 2;
	base_cfg->obs = tp->period_size * 2;

	return 0;
}

static int tb_set_pin_formats(struct tplg_comp_info *comp_info,
			      struct ipc4_base_module_cfg *base_cfg,
			      struct ipc4_base_module_cfg_ext *base_cfg_ext,
			      enum tb_pin_type pin_type)
{
	struct sof_ipc4_pin_format *formats_array;
	struct sof_ipc4_pin_format *pin_format_item;
	struct sof_ipc4_pin_format *pin_format;
	struct sof_ipc4_pin_format *base_cfg_ext_pin_formats =
		(struct sof_ipc4_pin_format *)base_cfg_ext->pin_formats;
	int format_list_count;
	int num_pins;
	int i, j;
	int pin_format_offset = 0;

	switch (pin_type) {
	case TB_PIN_TYPE_INPUT:
		num_pins = base_cfg_ext->nb_input_pins;
		formats_array = comp_info->available_fmt.input_pin_fmts;
		format_list_count = comp_info->available_fmt.num_input_formats;
		break;
	case TB_PIN_TYPE_OUTPUT:
		num_pins = base_cfg_ext->nb_output_pins;
		pin_format_offset = base_cfg_ext->nb_input_pins;
		formats_array = comp_info->available_fmt.output_pin_fmts;
		format_list_count = comp_info->available_fmt.num_output_formats;
		break;
	default:
		fprintf(stderr, "Error: Illegal pin type %d for %s\n", pin_type, comp_info->name);
	}

	for (i = pin_format_offset; i < num_pins + pin_format_offset; i++) {
		pin_format = &base_cfg_ext_pin_formats[i];

		if (i == pin_format_offset) {
			if (pin_type == TB_PIN_TYPE_INPUT) {
				pin_format->buffer_size = base_cfg->ibs;
				/* Note: Copy "struct ipc4_audio_format" to
				 * "struct sof_ipc4_pin_format". They are same
				 * but separate definitions. The last member fmt_cfg
				 * is defined with bitfields for channels_count, valid_bit_depth,
				 * s_type, reservedin ipc4_audio_format.
				 */
				memcpy(&pin_format->audio_fmt, &base_cfg->audio_fmt,
				       sizeof(pin_format->audio_fmt));
			} else {
				pin_format->buffer_size = base_cfg->obs;
				/* TODO: Using workaround, need to find out how to do this. This
				 * is copied kernel function sof_ipc4_process_set_pin_formats()
				 * from process->output_format. It's set in
				 * sof_ipc4_prepare_process_module().
				 */
				memcpy(&pin_format->audio_fmt, &base_cfg->audio_fmt,
				       sizeof(pin_format->audio_fmt));
			}
			continue;
		}

		for (j = 0; j < format_list_count; j++) {
			pin_format_item = &formats_array[j];
			if (pin_format_item->pin_index == i - pin_format_offset) {
				*pin_format = *pin_format_item;
				break;
			}
		}

		if (j == format_list_count) {
			fprintf(stderr, "%s pin %d format not found for %s\n",
				(pin_type == TB_PIN_TYPE_INPUT) ? "input" : "output",
				i - pin_format_offset, comp_info->name);
			return -EINVAL;
		}
	}

	return 0;
}

int tb_set_up_widget_base_config(struct testbench_prm *tp, struct tplg_comp_info *comp_info)
{
	char *config_name = tp->config[0].name;
	struct ipc4_base_module_cfg *base_cfg;
	struct ipc4_base_module_cfg_ext *base_cfg_ext;
	struct tb_config *config;
	struct tplg_pins_info *pins = &comp_info->pins_info;
	bool config_found = false;
	int ret, i;

	for (i = 0; i < tp->num_configs; i++) {
		config = &tp->config[i];

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
	ret = tb_match_audio_format(tp, comp_info, config);
	if (ret < 0)
		return ret;

	/* copy the basecfg into the ipc payload */
	memcpy(comp_info->ipc_payload, &comp_info->basecfg, sizeof(struct ipc4_base_module_cfg));

	/* Copy ext config for process type */
	if (comp_info->module_id == TB_PROCESS_MODULE_ID) {
		base_cfg = comp_info->ipc_payload;
		base_cfg_ext = comp_info->ipc_payload + sizeof(*base_cfg);
		base_cfg_ext->nb_input_pins = pins->num_input_pins;
		base_cfg_ext->nb_output_pins = pins->num_output_pins;
		ret = tb_set_pin_formats(comp_info, base_cfg, base_cfg_ext, TB_PIN_TYPE_INPUT);
		if (ret)
			return ret;

		ret = tb_set_pin_formats(comp_info, base_cfg, base_cfg_ext, TB_PIN_TYPE_OUTPUT);
		if (ret)
			return ret;
	}

	return 0;
}

static int tb_pipeline_set_state(struct testbench_prm *tp, int state,
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

int tb_pipelines_set_state(struct testbench_prm *tp, int state, int dir)
{
	struct ipc4_pipeline_set_state pipe_state = {{ 0 }};
	struct tplg_pipeline_list *pipeline_list;
	struct tplg_pipeline_info *pipe_info;
	int ret;
	int i;

	if (dir == SOF_IPC_STREAM_CAPTURE)
		pipeline_list = &tp->pcm_info->capture_pipeline_list;
	else
		pipeline_list = &tp->pcm_info->playback_pipeline_list;

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
			ret = tb_pipeline_set_state(tp, state, &pipe_state, pipe_info,
						    &tp->ipc_tx, &tp->ipc_rx);
			if (ret < 0)
				return ret;
		}

		return 0;
	}

	for (i = 0; i < pipeline_list->count; i++) {
		pipe_info = pipeline_list->pipelines[i];
		ret = tb_pipeline_set_state(tp, state, &pipe_state, pipe_info,
					    &tp->ipc_tx, &tp->ipc_rx);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/*
 * Topology widgets
 */

static int tb_new_src(struct testbench_prm *tp)
{
	struct ipc4_config_src src;
	struct tplg_context *ctx = &tp->tplg;
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	size_t uuid_offset;
	int ret;

	ret = tplg_parse_widget_audio_formats(ctx);
	if (ret < 0)
		return ret;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl)
		return -ENOMEM;

	comp_info->ipc_size = sizeof(struct ipc4_config_src);
	uuid_offset = comp_info->ipc_size;
	comp_info->ipc_size += sizeof(struct sof_uuid);
	comp_info->ipc_payload = calloc(comp_info->ipc_size, 1);
	if (!comp_info->ipc_payload)
		return -ENOMEM;

	comp_info->instance_id = tp->instance_ids[SND_SOC_TPLG_DAPM_EFFECT]++;
	comp_info->module_id = TB_SRC_MODULE_ID;

	ret = tplg_new_src(ctx, &src, sizeof(struct ipc4_config_src),
			   tplg_ctl, ctx->hdr->payload_size);
	if (ret < 0) {
		fprintf(stderr, "error: failed to create SRC\n");
		goto out;
	}

	/* copy volume data to ipc_payload */
	memcpy(comp_info->ipc_payload, &src, sizeof(struct ipc4_config_src));

	/* copy uuid to the end of the payload */
	memcpy(comp_info->ipc_payload + uuid_offset, &comp_info->uuid, sizeof(struct sof_uuid));

	tb_setup_widget_ipc_msg(comp_info);

out:
	free(tplg_ctl);
	return ret;
}

static int tb_new_asrc(struct testbench_prm *tp)
{
	struct ipc4_asrc_module_cfg asrc;
	struct tplg_context *ctx = &tp->tplg;
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	size_t uuid_offset;
	int ret;

	ret = tplg_parse_widget_audio_formats(ctx);
	if (ret < 0)
		return ret;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl)
		return -ENOMEM;

	comp_info->ipc_size = sizeof(struct ipc4_asrc_module_cfg);
	uuid_offset = comp_info->ipc_size;
	comp_info->ipc_size += sizeof(struct sof_uuid);
	comp_info->ipc_payload = calloc(comp_info->ipc_size, 1);
	if (!comp_info->ipc_payload)
		return -ENOMEM;

	comp_info->instance_id = tp->instance_ids[SND_SOC_TPLG_DAPM_EFFECT]++;
	comp_info->module_id = TB_ASRC_MODULE_ID;

	ret = tplg_new_asrc(ctx, &asrc, sizeof(struct ipc4_asrc_module_cfg),
			    tplg_ctl, ctx->hdr->payload_size);
	if (ret < 0) {
		fprintf(stderr, "error: failed to create SRC\n");
		goto out;
	}

	/* copy volume data to ipc_payload */
	memcpy(comp_info->ipc_payload, &asrc, sizeof(struct ipc4_asrc_module_cfg));

	/* copy uuid to the end of the payload */
	memcpy(comp_info->ipc_payload + uuid_offset, &comp_info->uuid, sizeof(struct sof_uuid));

	tb_setup_widget_ipc_msg(comp_info);

out:
	free(tplg_ctl);
	return ret;
}

static int tb_new_mixer(struct testbench_prm *tp)
{
	struct tplg_context *ctx = &tp->tplg;
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	char tplg_object[TB_IPC4_MAX_TPLG_OBJECT_SIZE] = {0};
	struct sof_ipc_comp_mixer *mixer = (struct sof_ipc_comp_mixer *)tplg_object;
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	int ret;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl)
		return -ENOMEM;

	comp_info->instance_id = tp->instance_ids[SND_SOC_TPLG_DAPM_MIXER]++;
	comp_info->ipc_size = sizeof(struct ipc4_base_module_cfg);
	comp_info->ipc_payload = calloc(comp_info->ipc_size, 1);
	if (!comp_info->ipc_payload)
		return -ENOMEM;

	ret = tplg_new_mixer(ctx, &mixer->comp, TB_IPC4_MAX_TPLG_OBJECT_SIZE,
			     tplg_ctl, ctx->hdr->payload_size);
	if (ret < 0) {
		fprintf(stderr, "error: failed to create mixer\n");
		goto out;
	}

	if (strstr(comp_info->name, "mixin")) {
		comp_info->module_id = 0x2;
		tb_setup_widget_ipc_msg(comp_info);
	} else {
		comp_info->module_id = 0x3;
		tb_setup_widget_ipc_msg(comp_info);
	}
out:
	free(tplg_ctl);
	return ret;
}

static int tb_new_pipeline(struct testbench_prm *tp)
{
	struct tplg_pipeline_info *pipe_info;
	struct sof_ipc_pipe_new pipeline = {{0}};
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	struct tplg_context *ctx = &tp->tplg;
	int ret = 0;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl)
		return -ENOMEM;

	pipe_info = calloc(sizeof(struct tplg_pipeline_info), 1);
	if (!pipe_info) {
		ret = -ENOMEM;
		goto out;
	}

	pipe_info->name = strdup(ctx->widget->name);
	if (!pipe_info->name) {
		free(pipe_info);
		goto out;
	}

	pipe_info->id = ctx->pipeline_id;

	ret = tplg_new_pipeline(ctx, &pipeline, sizeof(pipeline), tplg_ctl);
	if (ret < 0) {
		fprintf(stderr, "error: failed to create pipeline\n");
		free(pipe_info->name);
		free(pipe_info);
		goto out;
	}

	list_item_append(&pipe_info->item, &tp->pipeline_list);
	tplg_debug("loading pipeline %s\n", pipe_info->name);
out:
	free(tplg_ctl);
	return ret;
}

static int tb_new_buffer(struct testbench_prm *tp)
{
	struct ipc4_copier_module_cfg *copier = calloc(sizeof(struct ipc4_copier_module_cfg), 1);
	struct tplg_context *ctx = &tp->tplg;
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	int ret;

	if (!copier)
		return -ENOMEM;

	comp_info->ipc_payload = copier;

	ret = tplg_new_buffer(ctx, copier, sizeof(copier), NULL, 0);
	if (ret < 0) {
		fprintf(stderr, "error: failed to create pipeline\n");
		free(copier);
	}

	return ret;
}

int tb_new_aif_in_out(struct testbench_prm *tp, int dir)
{
	struct tplg_context *ctx = &tp->tplg;
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	struct ipc4_file_module_cfg *file;
	int ret;

	ret = tplg_parse_widget_audio_formats(ctx);
	if (ret < 0)
		return ret;

	comp_info->ipc_size = sizeof(struct ipc4_file_module_cfg) + sizeof(struct sof_uuid);
	comp_info->ipc_payload =  calloc(comp_info->ipc_size, 1);
	if (!comp_info->ipc_payload)
		return -ENOMEM;

	if (dir == SOF_IPC_STREAM_PLAYBACK) {
		/* Set from testbench command line*/
		file = (struct ipc4_file_module_cfg *)comp_info->ipc_payload;
		file->config.mode = FILE_READ;
		file->config.rate = tp->fs_in;
		file->config.channels = tp->channels_in;
		file->config.frame_fmt = tp->frame_fmt;
		file->config.direction = dir;

		comp_info->instance_id = tp->instance_ids[SND_SOC_TPLG_DAPM_AIF_IN]++;
		comp_info->module_id = TB_FILE_OUT_AIF_MODULE_ID;
		if (tb_is_pipeline_enabled(tp, ctx->pipeline_id)) {
			if (tp->input_file_index >= tp->input_file_num) {
				fprintf(stderr, "error: not enough input files for aif\n");
				return -EINVAL;
			}
			file->config.fn = tp->input_file[tp->input_file_index];
			tp->fr[tp->input_file_index].id = comp_info->module_id;
			tp->fr[tp->input_file_index].instance_id = comp_info->instance_id;
			tp->fr[tp->input_file_index].pipeline_id = ctx->pipeline_id;
			tp->input_file_index++;
		}

		tb_setup_widget_ipc_msg(comp_info);
	} else {
		file = (struct ipc4_file_module_cfg *)comp_info->ipc_payload;
		file->config.mode = FILE_WRITE;
		file->config.rate = tp->fs_out;
		file->config.channels = tp->channels_out;
		file->config.frame_fmt = tp->frame_fmt;
		file->config.direction = dir;

		comp_info->instance_id = tp->instance_ids[SND_SOC_TPLG_DAPM_AIF_OUT]++;
		comp_info->module_id = TB_FILE_IN_AIF_MODULE_ID;
		if (tb_is_pipeline_enabled(tp, ctx->pipeline_id)) {
			if (tp->output_file_index >= tp->output_file_num) {
				fprintf(stderr, "error: not enough output files for aif\n");
				return -EINVAL;
			}
			file->config.fn = tp->output_file[tp->output_file_index];
			tp->fw[tp->output_file_index].id = comp_info->module_id;
			tp->fw[tp->output_file_index].instance_id = comp_info->instance_id;
			tp->fw[tp->output_file_index].pipeline_id = ctx->pipeline_id;
			tp->output_file_index++;
		}
	}

	tb_setup_widget_ipc_msg(comp_info);
	memcpy(comp_info->ipc_payload + sizeof(struct ipc4_file_module_cfg), &tb_file_uuid,
	       sizeof(struct sof_uuid));
	return 0;
}

int tb_new_dai_in_out(struct testbench_prm *tp, int dir)
{
	struct tplg_context *ctx = &tp->tplg;
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	struct ipc4_file_module_cfg *file;
	int ret;

	ret = tplg_parse_widget_audio_formats(ctx);
	if (ret < 0)
		return ret;

	comp_info->ipc_size = sizeof(struct ipc4_file_module_cfg) + sizeof(struct sof_uuid);
	comp_info->ipc_payload =  calloc(comp_info->ipc_size, 1);
	if (!comp_info->ipc_payload)
		return -ENOMEM;

	if (dir == SOF_IPC_STREAM_PLAYBACK) {
		/* Set from testbench command line*/
		file = (struct ipc4_file_module_cfg *)comp_info->ipc_payload;
		file->config.mode = FILE_WRITE;
		file->config.rate = tp->fs_out;
		file->config.channels = tp->channels_out;
		file->config.frame_fmt = tp->frame_fmt;
		file->config.direction = dir;

		comp_info->instance_id = tp->instance_ids[SND_SOC_TPLG_DAPM_DAI_OUT]++;
		comp_info->module_id = TB_FILE_OUT_DAI_MODULE_ID;
		if (tb_is_pipeline_enabled(tp, ctx->pipeline_id)) {
			if (tp->output_file_index >= tp->output_file_num) {
				fprintf(stderr, "error: not enough output files for dai\n");
				return -EINVAL;
			}
			file->config.fn = tp->output_file[tp->output_file_index];
			tp->fw[tp->output_file_index].id = comp_info->module_id;
			tp->fw[tp->output_file_index].instance_id = comp_info->instance_id;
			tp->fw[tp->output_file_index].pipeline_id = ctx->pipeline_id;
			tp->output_file_index++;
		}
		tb_setup_widget_ipc_msg(comp_info);
	} else {
		file = (struct ipc4_file_module_cfg *)comp_info->ipc_payload;
		file->config.mode = FILE_READ;
		file->config.rate = tp->fs_in;
		file->config.channels = tp->channels_in;
		file->config.frame_fmt = tp->frame_fmt;
		file->config.direction = dir;

		comp_info->instance_id = tp->instance_ids[SND_SOC_TPLG_DAPM_DAI_IN]++;
		comp_info->module_id = TB_FILE_IN_DAI_MODULE_ID;
		if (tb_is_pipeline_enabled(tp, ctx->pipeline_id)) {
			if (tp->input_file_index >= tp->input_file_num) {
				fprintf(stderr, "error: not enough input files for dai\n");
				return -EINVAL;
			}
			file->config.fn = tp->input_file[tp->input_file_index];
			tp->fr[tp->input_file_index].id = comp_info->module_id;
			tp->fr[tp->input_file_index].instance_id = comp_info->instance_id;
			tp->fr[tp->input_file_index].pipeline_id = ctx->pipeline_id;
			tp->input_file_index++;
		}
	}

	tb_setup_widget_ipc_msg(comp_info);
	memcpy(comp_info->ipc_payload + sizeof(struct ipc4_file_module_cfg), &tb_file_uuid,
	       sizeof(struct sof_uuid));
	return 0;
}

int tb_new_pga(struct testbench_prm *tp)
{
	struct tplg_context *ctx = &tp->tplg;
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	struct ipc4_peak_volume_config volume;
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	size_t uuid_offset;
	int ret;

	comp_info->ipc_size = sizeof(struct ipc4_peak_volume_config);
	comp_info->ipc_size += sizeof(struct ipc4_base_module_cfg);
	uuid_offset = comp_info->ipc_size;
	comp_info->ipc_size += sizeof(struct sof_uuid);
	comp_info->ipc_payload = calloc(comp_info->ipc_size, 1);
	if (!comp_info->ipc_payload)
		return -ENOMEM;

	/* FIXME: move this to when the widget is actually set up */
	comp_info->instance_id = tp->instance_ids[SND_SOC_TPLG_DAPM_PGA]++;
	comp_info->module_id = TB_PGA_MODULE_ID;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl) {
		ret = -ENOMEM;
		goto out;
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

	/* copy uuid to the end of the payload */
	memcpy(comp_info->ipc_payload + uuid_offset, &comp_info->uuid, sizeof(struct sof_uuid));

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

int tb_new_process(struct testbench_prm *tp)
{
	struct tplg_context *ctx = &tp->tplg;
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	struct tplg_pins_info *pins = &comp_info->pins_info;
	size_t ext_size;
	size_t uuid_offset;
	int ret;

	ret = tplg_parse_widget_audio_formats(ctx);
	if (ret < 0)
		return ret;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl)
		return -ENOMEM;

	/* Ext config size */
	ext_size = ipc4_calc_base_module_cfg_ext_size(pins->num_input_pins, pins->num_output_pins);

	/* use base config variant with uuid */
	comp_info->ipc_size = sizeof(struct ipc4_base_module_cfg);
	comp_info->ipc_size += ext_size;
	uuid_offset =  comp_info->ipc_size;
	comp_info->ipc_size += sizeof(struct sof_uuid);
	comp_info->ipc_payload = calloc(comp_info->ipc_size, 1);
	if (!comp_info->ipc_payload) {
		ret = ENOMEM;
		goto out;
	}

	/* FIXME: move this to when the widget is actually set up */
	comp_info->instance_id = tp->instance_ids[SND_SOC_TPLG_DAPM_EFFECT]++;
	comp_info->module_id = TB_PROCESS_MODULE_ID;

	/* set up kcontrols */
	ret = tplg_create_controls(ctx, ctx->widget->num_kcontrols, tplg_ctl,
				   ctx->hdr->payload_size, comp_info);
	if (ret < 0) {
		fprintf(stderr, "error: loading controls\n");
		goto out;
	}

	tb_setup_widget_ipc_msg(comp_info);

	/* copy uuid to the end of the payload */
	memcpy(comp_info->ipc_payload + uuid_offset, &comp_info->uuid, sizeof(struct sof_uuid));

	/* TODO: drop tplg_ctl to avoid memory leak. Need to store and handle this
	 * to support controls.
	 */
	free(tplg_ctl);
	return 0;

out:
	free(tplg_ctl);
	free(comp_info->ipc_payload);
	return ret;
}

static int tb_register_graph(struct testbench_prm *tp, int count)
{
	struct tplg_context *ctx = &tp->tplg;
	int ret = 0;
	int i;

	for (i = 0; i < count; i++) {
		ret = tplg_parse_graph(ctx, &tp->widget_list, &tp->route_list);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static int tb_parse_pcm(struct testbench_prm *tp, int count)
{
	struct tplg_context *ctx = &tp->tplg;
	int ret, i;

	for (i = 0; i < count; i++) {
		ret = tplg_parse_pcm(ctx, &tp->widget_list, &tp->pcm_list);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/*
 * create a list with all widget info
 * containing mapping between component names and ids
 * which will be used for setting up component connections
 */
static inline int tb_insert_comp(struct testbench_prm *tp)
{
	struct tplg_context *ctx = &tp->tplg;
	struct tplg_comp_info *comp_info;
	int comp_id = ctx->comp_id;
	int ret;

	if (ctx->widget->id == SND_SOC_TPLG_DAPM_SCHEDULER)
		return 0;

	comp_info = calloc(sizeof(struct tplg_comp_info), 1);
	if (!comp_info)
		return -ENOMEM;

	comp_info->name = strdup(ctx->widget->name);
	if (!comp_info->name) {
		ret = -ENOMEM;
		goto err;
	}

	comp_info->stream_name = strdup(ctx->widget->sname);
	if (!comp_info->stream_name) {
		ret = -ENOMEM;
		goto err;
	}

	comp_info->id = comp_id;
	comp_info->type = ctx->widget->id;
	comp_info->pipeline_id = ctx->pipeline_id;
	ctx->current_comp_info = comp_info;

	ret = tb_parse_ipc4_comp_tokens(tp, &comp_info->basecfg);
	if (ret < 0)
		goto err;

	list_item_append(&comp_info->item, &tp->widget_list);

	printf("debug: loading comp_id %d: widget %s type %d size %d at offset %ld is_pages %d\n",
	       comp_id, ctx->widget->name, ctx->widget->id, ctx->widget->size,
	       ctx->tplg_offset, comp_info->basecfg.is_pages);

	return 0;

err:
	free(comp_info->name);
	free(comp_info);
	return ret;
}

/* load dapm widget */
static int tb_load_widget(struct testbench_prm *tp)
{
	struct tplg_context *ctx = &tp->tplg;
	int ret = 0;

	/* get next widget */
	ctx->widget = tplg_get_widget(ctx);
	ctx->widget_size = ctx->widget->size;

	/* insert widget into mapping */
	ret = tb_insert_comp(tp);
	if (ret < 0) {
		fprintf(stderr, "tb_load_widget: invalid widget index\n");
		return ret;
	}

	/* load widget based on type */
	switch (tp->tplg.widget->id) {
	/* load pga widget */
	case SND_SOC_TPLG_DAPM_PGA:
		if (tb_new_pga(tp) < 0) {
			fprintf(stderr, "error: load pga\n");
			return -EINVAL;
		}
		break;
	case SND_SOC_TPLG_DAPM_AIF_IN:
		if (tb_new_aif_in_out(tp, SOF_IPC_STREAM_PLAYBACK) < 0) {
			fprintf(stderr, "error: load AIF IN failed\n");
			return -EINVAL;
		}
		break;
	case SND_SOC_TPLG_DAPM_AIF_OUT:
		if (tb_new_aif_in_out(tp, SOF_IPC_STREAM_CAPTURE) < 0) {
			fprintf(stderr, "error: load AIF OUT failed\n");
			return -EINVAL;
		}
		break;
	case SND_SOC_TPLG_DAPM_DAI_IN:
		if (tb_new_dai_in_out(tp, SOF_IPC_STREAM_PLAYBACK) < 0) {
			fprintf(stderr, "error: load filewrite\n");
			return -EINVAL;
		}
		break;
	case SND_SOC_TPLG_DAPM_DAI_OUT:
		if (tb_new_dai_in_out(tp, SOF_IPC_STREAM_CAPTURE) < 0) {
			fprintf(stderr, "error: load filewrite\n");
			return -EINVAL;
		}
		break;
	case SND_SOC_TPLG_DAPM_BUFFER:
		if (tb_new_buffer(tp) < 0) {
			fprintf(stderr, "error: load buffer\n");
			return -EINVAL;
		}
		break;

	case SND_SOC_TPLG_DAPM_SCHEDULER:
		if (tb_new_pipeline(tp) < 0) {
			fprintf(stderr, "error: load pipeline\n");
			return -EINVAL;
		}
		break;

	case SND_SOC_TPLG_DAPM_SRC:
		if (tb_new_src(tp) < 0) {
			fprintf(stderr, "error: load src\n");
			return -EINVAL;
		}
		break;
	case SND_SOC_TPLG_DAPM_ASRC:
		if (tb_new_asrc(tp) < 0) {
			fprintf(stderr, "error: load src\n");
			return -EINVAL;
		}
		break;
	case SND_SOC_TPLG_DAPM_MIXER:
		if (tb_new_mixer(tp) < 0) {
			fprintf(stderr, "error: load mixer\n");
			return -EINVAL;
		}
		break;
	case SND_SOC_TPLG_DAPM_EFFECT:
		if (tb_new_process(tp) < 0) {
			fprintf(stderr, "error: load effect\n");
			return -EINVAL;
		}
		break;

	/* unsupported widgets */
	default:
		if (tb_check_trace(LOG_LEVEL_DEBUG))
			printf("Widget %s id %d unsupported and skipped: size %d priv size %d\n",
			       ctx->widget->name, ctx->widget->id,
			       ctx->widget->size, ctx->widget->priv.size);
		break;
	}

	return 0;
}

#define SOF_IPC4_VOL_ZERO_DB	0x7fffffff
#define VOLUME_FWL		16
/*
 * Constants used in the computation of linear volume gain
 * from dB gain 20th root of 10 in Q1.16 fixed-point notation
 */
#define VOL_TWENTIETH_ROOT_OF_TEN	73533
/* 40th root of 10 in Q1.16 fixed-point notation*/
#define VOL_FORTIETH_ROOT_OF_TEN	69419

/* 0.5 dB step value in topology TLV */
#define VOL_HALF_DB_STEP	50

/*
 * Function to truncate an unsigned 64-bit number
 * by x bits and return 32-bit unsigned number. This
 * function also takes care of rounding while truncating
 */
static uint32_t vol_shift_64(uint64_t i, uint32_t x)
{
	if (x == 0)
		return (uint32_t)i;

	/* do not truncate more than 32 bits */
	if (x > 32)
		x = 32;

	return (uint32_t)(((i >> (x - 1)) + 1) >> 1);
}

/*
 * Function to compute a ** exp where,
 * a is a fractional number represented by a fixed-point integer with a fractional word length
 * of "fwl"
 * exp is an integer
 * fwl is the fractional word length
 * Return value is a fractional number represented by a fixed-point integer with a fractional
 * word length of "fwl"
 */
static uint32_t vol_pow32(uint32_t a, int exp, uint32_t fwl)
{
	int i, iter;
	uint32_t power = 1 << fwl;
	unsigned long long numerator;

	/* if exponent is 0, return 1 */
	if (exp == 0)
		return power;

	/* determine the number of iterations based on the exponent */
	if (exp < 0)
		iter = exp * -1;
	else
		iter = exp;

	/* multiply a "iter" times to compute power */
	for (i = 0; i < iter; i++) {
		/*
		 * Product of 2 Qx.fwl fixed-point numbers yields a Q2*x.2*fwl
		 * Truncate product back to fwl fractional bits with rounding
		 */
		power = vol_shift_64((uint64_t)power * a, fwl);
	}

	if (exp > 0) {
		/* if exp is positive, return the result */
		return power;
	}

	/* if exp is negative, return the multiplicative inverse */
	numerator = (uint64_t)1 << (fwl << 1);
	numerator /= power;

	return (uint32_t)numerator;
}

/*
 * Function to calculate volume gain from TLV data.
 * This function can only handle gain steps that are multiples of 0.5 dB
 */
static uint32_t vol_compute_gain(uint32_t value, struct snd_soc_tplg_tlv_dbscale *scale)
{
	int dB_gain;
	uint32_t linear_gain;
	int f_step;

	/* mute volume */
	if (value == 0 && scale->mute)
		return 0;

	/* compute dB gain from tlv. tlv_step in topology is multiplied by 100 */
	dB_gain = (int)scale->min / 100 + (value * scale->step) / 100;

	/* compute linear gain represented by fixed-point int with VOLUME_FWL fractional bits */
	linear_gain = vol_pow32(VOL_TWENTIETH_ROOT_OF_TEN, dB_gain, VOLUME_FWL);

	/* extract the fractional part of volume step */
	f_step = scale->step - (scale->step / 100);

	/* if volume step is an odd multiple of 0.5 dB */
	if (f_step == VOL_HALF_DB_STEP && (value & 1))
		linear_gain = vol_shift_64((uint64_t)linear_gain * VOL_FORTIETH_ROOT_OF_TEN,
					   VOLUME_FWL);

	return linear_gain;
}

/* helper function to add new kcontrols to the list of kcontrols */
static int tb_kcontrol_cb_new(struct snd_soc_tplg_ctl_hdr *tplg_ctl,
			      void *comp, void *arg, int index)
{
	struct tplg_comp_info *comp_info = comp;
	struct testbench_prm *tp = arg;
	struct tb_glb_state *glb = &tp->glb_ctx;
	struct tb_ctl *ctl;
	struct snd_soc_tplg_mixer_control *tplg_mixer;
	struct snd_soc_tplg_enum_control *tplg_enum;
	struct snd_soc_tplg_bytes_control *tplg_bytes;

	if (glb->num_ctls >= TB_MAX_CTLS) {
		fprintf(stderr, "Error: Too many controls already.\n");
		return -EINVAL;
	}

	fprintf(stderr, "Info: Found control type %d: %s\n", tplg_ctl->type, tplg_ctl->name);

	switch (tplg_ctl->ops.info) {
	case SND_SOC_TPLG_CTL_RANGE:
	case SND_SOC_TPLG_CTL_STROBE:
		fprintf(stderr, "Warning: Not supported ctl type %d\n", tplg_ctl->type);
		return 0;
	case SND_SOC_TPLG_CTL_VOLSW:
	case SND_SOC_TPLG_CTL_VOLSW_SX:
	case SND_SOC_TPLG_CTL_VOLSW_XR_SX:
		tplg_mixer = (struct snd_soc_tplg_mixer_control *)tplg_ctl;
		struct snd_soc_tplg_ctl_tlv *tlv;
		struct snd_soc_tplg_tlv_dbscale *scale;
		int i;

		glb->size += sizeof(struct tb_ctl);
		ctl = &glb->ctl[glb->num_ctls++];
		ctl->module_id = comp_info->module_id;
		ctl->instance_id = comp_info->instance_id;
		ctl->mixer_ctl = *tplg_mixer;
		ctl->index = index;
		ctl->type = tplg_ctl->type;
		tlv = &tplg_ctl->tlv;
		scale = &tlv->scale;

		/* populate the volume table */
		for (i = 0; i < tplg_mixer->max + 1 ; i++) {
			uint32_t val = vol_compute_gain(i, scale);

			/* Can be over Q1.31, need to saturate */
			uint64_t q31val = ((uint64_t)val) << 15;

			ctl->volume_table[i] = q31val > SOF_IPC4_VOL_ZERO_DB ?
							SOF_IPC4_VOL_ZERO_DB : q31val;
		}
		break;
	case SND_SOC_TPLG_CTL_ENUM:
	case SND_SOC_TPLG_CTL_ENUM_VALUE:
		tplg_enum = (struct snd_soc_tplg_enum_control *)tplg_ctl;
		glb->size += sizeof(struct tb_ctl);
		ctl = &glb->ctl[glb->num_ctls++];
		ctl->module_id = comp_info->module_id;
		ctl->instance_id = comp_info->instance_id;
		ctl->enum_ctl = *tplg_enum;
		ctl->index = index;
		ctl->type = tplg_ctl->type;
		break;
	case SND_SOC_TPLG_CTL_BYTES:
	{
		tplg_bytes = (struct snd_soc_tplg_bytes_control *)tplg_ctl;
		glb->size += sizeof(struct tb_ctl);
		ctl = &glb->ctl[glb->num_ctls++];
		ctl->module_id = comp_info->module_id;
		ctl->instance_id = comp_info->instance_id;
		ctl->bytes_ctl = *tplg_bytes;
		ctl->index = index;
		ctl->type = tplg_ctl->type;
		memcpy(ctl->data, tplg_bytes->priv.data, tplg_bytes->priv.size);
		break;
	}
	default:
		fprintf(stderr, "Error: Invalid ctl type %d\n", tplg_ctl->type);
		return -EINVAL;
	}

	return 0;
}

/* parse topology file and set up pipeline */
int tb_parse_topology(struct testbench_prm *tp)

{
	struct tplg_context *ctx = &tp->tplg;
	struct snd_soc_tplg_hdr *hdr;
	struct list_item *item;
	int i;
	int ret = 0;
	FILE *file;

	ctx->ipc_major = 4;
	ctx->ctl_arg = tp;
	ctx->ctl_cb = tb_kcontrol_cb_new;
	tp->glb_ctx.ctl = calloc(TB_MAX_CTLS, sizeof(struct tb_ctl));
	if (!tp->glb_ctx.ctl) {
		fprintf(stderr, "error: failed to allocate for controls.\n");
		return -ENOMEM;
	}

	/* open topology file */
	file = fopen(ctx->tplg_file, "rb");
	if (!file) {
		fprintf(stderr, "error: can't open topology %s : %s\n", ctx->tplg_file,
			strerror(errno));
		return -errno;
	}

	/* file size */
	if (fseek(file, 0, SEEK_END)) {
		fprintf(stderr, "error: can't seek to end of topology: %s\n",
			strerror(errno));
		fclose(file);
		return -errno;
	}
	ctx->tplg_size = ftell(file);
	if (fseek(file, 0, SEEK_SET)) {
		fprintf(stderr, "error: can't seek to beginning of topology: %s\n",
			strerror(errno));
		fclose(file);
		return -errno;
	}

	/* load whole topology into memory */
	ctx->tplg_base = calloc(ctx->tplg_size, 1);
	if (!ctx->tplg_base) {
		fprintf(stderr, "error: can't alloc buffer for topology %zu bytes\n",
			ctx->tplg_size);
		fclose(file);
		return -ENOMEM;
	}
	ret = fread(ctx->tplg_base, ctx->tplg_size, 1, file);
	if (ret != 1) {
		fprintf(stderr, "error: can't read topology: %s\n", strerror(errno));
		free(ctx->tplg_base);
		fclose(file);
		return -errno;
	}
	fclose(file);

	/* initialize widget, route, pipeline and pcm lists */
	list_init(&tp->widget_list);
	list_init(&tp->route_list);
	list_init(&tp->pcm_list);
	list_init(&tp->pipeline_list);

	while (ctx->tplg_offset < ctx->tplg_size) {
		/* read next topology header */
		hdr = tplg_get_hdr(ctx);

		tplg_debug("type: %x, size: 0x%x count: %d index: %d\n",
			   hdr->type, hdr->payload_size, hdr->count, hdr->index);

		ctx->hdr = hdr;

		/* parse header and load the next block based on type */
		switch (hdr->type) {
		/* load dapm widget */
		case SND_SOC_TPLG_TYPE_DAPM_WIDGET:

			tplg_debug("number of DAPM widgets %d\n", hdr->count);

			/* update max pipeline_id */
			ctx->pipeline_id = hdr->index;

			for (i = 0; i < hdr->count; i++) {
				ret = tb_load_widget(tp);
				if (ret < 0) {
					fprintf(stderr, "error: loading widget\n");
					goto out;
				}
				ctx->comp_id++;
			}
			break;

		/* set up component connections from pipeline graph */
		case SND_SOC_TPLG_TYPE_DAPM_GRAPH:
			ret = tb_register_graph(tp, hdr->count);
			if (ret < 0) {
				fprintf(stderr, "error: pipeline graph\n");
				goto out;
			}
			break;

		case SND_SOC_TPLG_TYPE_PCM:
			ret = tb_parse_pcm(tp, hdr->count);
			if (ret < 0) {
				fprintf(stderr, "error: parsing pcm\n");
				goto out;
			}
			break;

		default:
			tplg_skip_hdr_payload(ctx);
			break;
		}
	}

	/* assign pipeline to every widget in the widget list */
	list_for_item(item, &tp->widget_list) {
		struct tplg_comp_info *comp_info = container_of(item, struct tplg_comp_info, item);
		struct list_item *pipe_item;

		list_for_item(pipe_item, &tp->pipeline_list) {
			struct tplg_pipeline_info *pipe_info;

			pipe_info = container_of(pipe_item, struct tplg_pipeline_info, item);
			if (pipe_info->id == comp_info->pipeline_id) {
				comp_info->pipe_info = pipe_info;
				break;
			}
		}

		if (!comp_info->pipe_info) {
			fprintf(stderr, "warning: failed  assigning pipeline for %s\n",
				comp_info->name);
		}
	}

	return 0;

out:
	/* free all data */
	free(ctx->tplg_base);
	return ret;
}

/*
 * To run
 */

int tb_set_running_state(struct testbench_prm *tp)
{
	int ret;

	ret = tb_pipelines_set_state(tp, SOF_IPC4_PIPELINE_STATE_PAUSED, SOF_IPC_STREAM_PLAYBACK);
	if (ret) {
		fprintf(stderr, "error: failed to set state to paused\n");
		return ret;
	}

	ret = tb_pipelines_set_state(tp, SOF_IPC4_PIPELINE_STATE_PAUSED, SOF_IPC_STREAM_CAPTURE);
	if (ret) {
		fprintf(stderr, "error: failed to set state to paused\n");
		return ret;
	}

	tb_debug_print("pipelines are set to paused state\n");
	ret = tb_pipelines_set_state(tp, SOF_IPC4_PIPELINE_STATE_RUNNING, SOF_IPC_STREAM_PLAYBACK);
	if (ret) {
		fprintf(stderr, "error: failed to set state to running\n");
		return ret;
	}

	ret = tb_pipelines_set_state(tp, SOF_IPC4_PIPELINE_STATE_RUNNING, SOF_IPC_STREAM_CAPTURE);
	if (ret)
		fprintf(stderr, "error: failed to set state to running\n");

	tb_debug_print("pipelines are set to running state\n");
	return ret;
}

/*
 * To stop
 */

int tb_set_reset_state(struct testbench_prm *tp)
{
	int ret;

	ret = tb_pipelines_set_state(tp, SOF_IPC4_PIPELINE_STATE_PAUSED, SOF_IPC_STREAM_PLAYBACK);
	if (ret) {
		fprintf(stderr, "error: failed to set state to paused\n");
		return ret;
	}

	tb_schedule_pipeline_check_state(tp);

	ret = tb_pipelines_set_state(tp, SOF_IPC4_PIPELINE_STATE_RESET, SOF_IPC_STREAM_PLAYBACK);
	if (ret) {
		fprintf(stderr, "error: failed to set state to reset\n");
		return ret;
	}

	tb_schedule_pipeline_check_state(tp);

	ret = tb_pipelines_set_state(tp, SOF_IPC4_PIPELINE_STATE_PAUSED, SOF_IPC_STREAM_CAPTURE);
	if (ret) {
		fprintf(stderr, "error: failed to set state to paused\n");
		return ret;
	}

	tb_schedule_pipeline_check_state(tp);

	ret = tb_pipelines_set_state(tp, SOF_IPC4_PIPELINE_STATE_RESET, SOF_IPC_STREAM_CAPTURE);
	if (ret)
		fprintf(stderr, "error: failed to set state to reset\n");

	tb_schedule_pipeline_check_state(tp);
	return ret;
}

int tb_delete_pipeline(struct testbench_prm *tp, struct tplg_pipeline_info *pipe_info)
{
	struct ipc4_pipeline_delete msg = {{0}};
	struct ipc4_message_reply reply;
	int ret;

	msg.primary.r.type = SOF_IPC4_GLB_DELETE_PIPELINE;
	msg.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
	msg.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	msg.primary.r.instance_id = pipe_info->instance_id;

	ret = tb_mq_cmd_tx_rx(&tp->ipc_tx, &tp->ipc_rx, &msg, sizeof(msg), &reply, sizeof(reply));
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

int tb_free_route(struct testbench_prm *tp, struct tplg_route_info *route_info)
{
	struct tplg_comp_info *src_comp_info = route_info->source;
	struct tplg_comp_info *sink_comp_info = route_info->sink;
	struct ipc4_module_bind_unbind bu = {{0}};
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

	ret = tb_mq_cmd_tx_rx(&tp->ipc_tx, &tp->ipc_rx, &bu, sizeof(bu), &reply, sizeof(reply));
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

static void tb_ctl_ipc_message(struct ipc4_module_large_config *config, int param_id,
			       size_t offset_or_size, uint32_t module_id, uint32_t instance_id,
			       uint32_t type)
{
	config->primary.r.type = type;
	config->primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_MODULE_MSG;
	config->primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	config->primary.r.module_id = module_id;
	config->primary.r.instance_id = instance_id;

	config->extension.r.data_off_size = offset_or_size;
	config->extension.r.large_param_id = param_id;
}

int tb_send_bytes_data(struct tb_mq_desc *ipc_tx, struct tb_mq_desc *ipc_rx,
		       uint32_t module_id, uint32_t instance_id, struct sof_abi_hdr *abi)
{
	struct ipc4_module_large_config config = {{ 0 }};
	struct ipc4_message_reply reply;
	void *msg;
	int ret;
	size_t chunk_size;
	size_t msg_size;
	size_t msg_size_full = sizeof(config) + abi->size;
	size_t remaining = abi->size;
	size_t payload_limit = TB_IPC4_MAX_MSG_SIZE - sizeof(config);
	size_t offset = 0;

	/* allocate memory for IPC message */
	msg_size = MIN(msg_size_full, TB_IPC4_MAX_MSG_SIZE);
	msg = calloc(msg_size, 1);
	if (!msg)
		return -ENOMEM;

	config.extension.r.init_block = 1;

	/* configure the IPC message */
	tb_ctl_ipc_message(&config, abi->type, remaining, module_id, instance_id,
			   SOF_IPC4_MOD_LARGE_CONFIG_SET);

	do {
		if (remaining > payload_limit) {
			chunk_size = payload_limit;
		} else {
			chunk_size = remaining;
			config.extension.r.final_block = 1;
		}

		if (offset) {
			config.extension.r.init_block = 0;
			tb_ctl_ipc_message(&config, abi->type, offset, module_id, instance_id,
					   SOF_IPC4_MOD_LARGE_CONFIG_SET);
		}

		/* set the IPC message data */
		memcpy(msg, &config, sizeof(config));
		memcpy(msg + sizeof(config), (char *)abi->data + offset, chunk_size);

		/* send the message and check status */
		ret = tb_mq_cmd_tx_rx(ipc_tx, ipc_rx, msg, chunk_size + sizeof(config),
				      &reply, sizeof(reply));
		if (ret < 0) {
			fprintf(stderr, "Error: Failed to send IPC to set bytes data.\n");
			goto out;
		}

		if (reply.primary.r.status != IPC4_SUCCESS) {
			fprintf(stderr, "Error: Failed with status %d.\n", reply.primary.r.status);
			ret = -EINVAL;
			goto out;
		}

		offset += chunk_size;
		remaining -= chunk_size;
	} while (remaining);

out:
	free(msg);
	return ret;
}

#endif /* CONFIG_IPC_MAJOR_4 */
