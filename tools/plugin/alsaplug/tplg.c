// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>

/* Topology loader to set up components and pipeline */

#include <stdio.h>
#include <sys/poll.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <assert.h>
#include <errno.h>
#include <dlfcn.h>

#include <ipc4/error_status.h>

#include <tplg_parser/topology.h>
#include <tplg_parser/tokens.h>

#include <alsa/asoundlib.h>
#include <alsa/control_external.h>
#include <alsa/pcm_external.h>

#include "plugin.h"

#define FILE_READ 0
#define FILE_WRITE 1

#define MAX_TPLG_OBJECT_SIZE	4096

//#include <sound/asound.h>

/* temporary - current MAXLEN is not define in UAPI header - fix pending */
#ifndef SNDRV_CTL_ELEM_ID_NAME_MAXLEN
#define SNDRV_CTL_ELEM_ID_NAME_MAXLEN	44
#endif
#include <alsa/sound/asoc.h>

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

static int plug_parse_ipc4_comp_tokens(snd_sof_plug_t *plug, struct ipc4_base_module_cfg *base_cfg)
{
	struct tplg_context *ctx = &plug->tplg;
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

static void plug_setup_widget_ipc_msg(struct tplg_comp_info *comp_info)
{
	struct ipc4_module_init_instance *module_init = &comp_info->module_init;

	module_init->primary.r.type = SOF_IPC4_MOD_INIT_INSTANCE;
	module_init->primary.r.module_id = comp_info->module_id;
	module_init->primary.r.instance_id = comp_info->instance_id;
	module_init->primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_MODULE_MSG;
	module_init->primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
}

static int plug_aif_in_out(snd_sof_plug_t *plug, int dir)
{
	struct tplg_context *ctx = &plug->tplg;
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	int ret;

	ret = tplg_parse_widget_audio_formats(ctx);
	if (ret < 0)
		return ret;

	comp_info->ipc_payload =  calloc(sizeof(struct ipc4_base_module_cfg), 1);
	if (!comp_info->ipc_payload)
		return -ENOMEM;

	comp_info->ipc_size = sizeof(struct ipc4_base_module_cfg);

	if (dir == SOF_IPC_STREAM_PLAYBACK) {
		comp_info->module_id = 0x96;
		plug_setup_widget_ipc_msg(comp_info);
	} else {
		comp_info->module_id = 0x98;
		plug_setup_widget_ipc_msg(comp_info);
	}

	return 0;
}

static int plug_dai_in_out(snd_sof_plug_t *plug, int dir)
{
	struct tplg_context *ctx = &plug->tplg;
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	int ret;

	ret = tplg_parse_widget_audio_formats(ctx);
	if (ret < 0)
		return ret;

	comp_info->ipc_payload =  calloc(sizeof(struct ipc4_base_module_cfg), 1);
	if (!comp_info->ipc_payload)
		return -ENOMEM;

	comp_info->ipc_size = sizeof(struct ipc4_base_module_cfg);

	if (dir == SOF_IPC_STREAM_PLAYBACK) {
		comp_info->module_id = 0x97;
		plug_setup_widget_ipc_msg(comp_info);
	} else {
		comp_info->module_id = 0x99;
		plug_setup_widget_ipc_msg(comp_info);
	}

	return 0;
}

static int plug_new_src_ipc(snd_sof_plug_t *plug)
{
	struct tplg_context *ctx = &plug->tplg;
	char tplg_object[MAX_TPLG_OBJECT_SIZE] = {0};
	struct sof_ipc_comp_src *src =
		(struct sof_ipc_comp_src *)tplg_object;
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	int ret;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl)
		return -ENOMEM;

	ret = tplg_new_src(ctx, &src->comp, MAX_TPLG_OBJECT_SIZE,
			   tplg_ctl, ctx->hdr->payload_size);
	if (ret < 0) {
		SNDERR("error: failed to create src\n");
		goto out;
	}

out:
	free(tplg_ctl);
	return ret;
}

static int plug_new_asrc_ipc(snd_sof_plug_t *plug)
{
	struct tplg_context *ctx = &plug->tplg;
	char tplg_object[MAX_TPLG_OBJECT_SIZE] = {0};
	struct sof_ipc_comp_asrc *asrc =
		(struct sof_ipc_comp_asrc *)tplg_object;
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	int ret;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl)
		return -ENOMEM;

	ret = tplg_new_asrc(ctx, &asrc->comp, MAX_TPLG_OBJECT_SIZE,
			    tplg_ctl, ctx->hdr->payload_size);
	if (ret < 0) {
		SNDERR("error: failed to create PGA\n");
		goto out;
	}

out:
	free(tplg_ctl);
	return ret;
}

static int plug_new_mixer(snd_sof_plug_t *plug)
{
	struct tplg_context *ctx = &plug->tplg;
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	char tplg_object[MAX_TPLG_OBJECT_SIZE] = {0};
	struct sof_ipc_comp_mixer *mixer =
		(struct sof_ipc_comp_mixer *)tplg_object;
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	int ret;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl)
		return -ENOMEM;

	comp_info->instance_id = plug->instance_ids[SND_SOC_TPLG_DAPM_MIXER]++;
	comp_info->ipc_size = sizeof(struct ipc4_base_module_cfg);
	comp_info->ipc_payload = calloc(comp_info->ipc_size, 1);
	if (!comp_info->ipc_payload)
		return -ENOMEM;

	ret = tplg_new_mixer(ctx, &mixer->comp, MAX_TPLG_OBJECT_SIZE,
			     tplg_ctl, ctx->hdr->payload_size);
	if (ret < 0) {
		SNDERR("error: failed to create mixer\n");
		goto out;
	}

	if (strstr(comp_info->name, "mixin")) {
		comp_info->module_id = 0x2;
		plug_setup_widget_ipc_msg(comp_info);
	} else {
		comp_info->module_id = 0x3;
		plug_setup_widget_ipc_msg(comp_info);
	}
out:
	free(tplg_ctl);
	return ret;
}

static int plug_new_pga(snd_sof_plug_t *plug)
{
	struct tplg_context *ctx = &plug->tplg;
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
	comp_info->instance_id = plug->instance_ids[SND_SOC_TPLG_DAPM_PGA]++;
	comp_info->module_id = 0x6;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl) {
		free(comp_info->ipc_payload);
		return -ENOMEM;
	}

	ret = tplg_new_pga(ctx, &volume, sizeof(struct ipc4_peak_volume_config),
			   tplg_ctl, ctx->hdr->payload_size);
	if (ret < 0) {
		SNDERR("%s: failed to create PGA\n", __func__);
		goto out;
	}

	/* copy volume data to ipc_payload */
	memcpy(comp_info->ipc_payload + sizeof(struct ipc4_base_module_cfg),
	       &volume, sizeof(struct ipc4_peak_volume_config));

	/* skip kcontrols for now */
	if (tplg_create_controls(ctx, ctx->widget->num_kcontrols,
				 tplg_ctl, ctx->hdr->payload_size, &volume) < 0) {
		SNDERR("error: loading controls\n");
		goto out;
	}

	plug_setup_widget_ipc_msg(comp_info);

	free(tplg_ctl);

	return ret;

out:
	free(tplg_ctl);
	free(comp_info->ipc_payload);

	return ret;
}

static int plug_new_process(snd_sof_plug_t *plug)
{
	struct tplg_context *ctx = &plug->tplg;
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	struct sof_ipc_comp_process *process;
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	int ret;

	process = calloc(MAX_TPLG_OBJECT_SIZE, 1);
	if (!process)
		return -ENOMEM;

	comp_info->ipc_payload = process;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl) {
		free(process);
		return -ENOMEM;
	}

	ret = tplg_new_process(ctx, process, MAX_TPLG_OBJECT_SIZE,
			       tplg_ctl, ctx->hdr->payload_size);
	if (ret < 0) {
		SNDERR("error: failed to create PGA\n");
		goto out;
	}
out:
	free(tplg_ctl);
	return ret;
}

static int plug_new_pipeline(snd_sof_plug_t *plug)
{
	struct tplg_pipeline_info *pipe_info;
	struct sof_ipc_pipe_new pipeline = {0};
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	struct tplg_context *ctx = &plug->tplg;
	int ret;

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
		SNDERR("error: failed to create pipeline\n");
		free(pipe_info->name);
		free(pipe_info);
		goto out;
	}

	list_item_append(&pipe_info->item, &plug->pipeline_list);
	tplg_debug("loading pipeline %s\n", pipe_info->name);
out:
	free(tplg_ctl);
	return ret;
}

static int plug_new_buffer(snd_sof_plug_t *plug)
{
	struct ipc4_copier_module_cfg *copier = calloc(sizeof(struct ipc4_copier_module_cfg), 1);
	struct tplg_context *ctx = &plug->tplg;
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	int ret;

	if (!copier)
		return -ENOMEM;

	comp_info->ipc_payload = copier;

	ret = tplg_new_buffer(ctx, copier, sizeof(copier), NULL, 0);
	if (ret < 0) {
		SNDERR("error: failed to create pipeline\n");
		free(copier);
	}

	return ret;
}

/* Insert new comp info into the list of widgets */
static inline int plug_insert_comp(snd_sof_plug_t *plug)
{
	struct tplg_context *ctx = &plug->tplg;
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
		goto sname_err;
	}

	comp_info->id = comp_id;
	comp_info->type = ctx->widget->id;
	comp_info->pipeline_id = ctx->pipeline_id;
	ctx->current_comp_info = comp_info;

	ret = plug_parse_ipc4_comp_tokens(plug, &comp_info->basecfg);
	if (ret < 0)
		goto sname_err;

	list_item_append(&comp_info->item, &plug->widget_list);

	tplg_debug("debug: loading comp_id %d: widget %s type %d size %d at offset %ld is_pages %d\n",
		   comp_id, ctx->widget->name, ctx->widget->id, ctx->widget->size,
		   ctx->tplg_offset, comp_info->basecfg.is_pages);

	return 0;
sname_err:
	free(comp_info->name);
err:
	free(comp_info);
	return ret;
}

/* load dapm widget */
static int plug_load_widget(snd_sof_plug_t *plug)
{
	struct tplg_context *ctx = &plug->tplg;
	int ret = 0;

	/* get next widget */
	ctx->widget = tplg_get_widget(ctx);
	ctx->widget_size = ctx->widget->size;

	/* insert widget into mapping */
	ret = plug_insert_comp(plug);
	if (ret < 0) {
		SNDERR("plug_load_widget: invalid widget index\n");
		return ret;
	}

	/* load widget based on type */
	switch (ctx->widget->id) {
	/* load pga widget */
	case SND_SOC_TPLG_DAPM_PGA:
		if (plug_new_pga(plug) < 0) {
			SNDERR("error: load pga\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_AIF_IN:
		if (plug_aif_in_out(plug, SOF_IPC_STREAM_PLAYBACK) < 0) {
			SNDERR("error: load AIF IN failed\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_AIF_OUT:
		if (plug_aif_in_out(plug, SOF_IPC_STREAM_CAPTURE) < 0) {
			SNDERR("error: load AIF OUT failed\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_DAI_IN:
		if (plug_dai_in_out(plug, SOF_IPC_STREAM_PLAYBACK) < 0) {
			SNDERR("error: load filewrite\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_DAI_OUT:
		if (plug_dai_in_out(plug, SOF_IPC_STREAM_CAPTURE) < 0) {
			SNDERR("error: load filewrite\n");
			ret = -EINVAL;
			goto exit;
		}
		break;

	case SND_SOC_TPLG_DAPM_BUFFER:
		if (plug_new_buffer(plug) < 0) {
			SNDERR("error: load pipeline\n");
			ret = -EINVAL;
			goto exit;
		}
		break;

	case SND_SOC_TPLG_DAPM_SCHEDULER:
		if (plug_new_pipeline(plug) < 0) {
			SNDERR("error: load pipeline\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_SRC:
		if (plug_new_src_ipc(plug) < 0) {
			SNDERR("error: load src\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_ASRC:
		if (plug_new_asrc_ipc(plug) < 0) {
			SNDERR("error: load asrc\n");
			ret = -EINVAL;
			goto exit;
		}
		break;

	case SND_SOC_TPLG_DAPM_MIXER:
		if (plug_new_mixer(plug) < 0) {
			SNDERR("error: load mixer\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_EFFECT:
		if (plug_new_process(plug) < 0) {
			SNDERR("error: load effect\n");
			ret = -EINVAL;
			goto exit;
		}
		break;

	/* unsupported widgets */
	default:
		tplg_debug("info: Widget %s id %d unsupported and skipped: size %d priv size %d\n",
			   ctx->widget->name, ctx->widget->id,
			   ctx->widget->size, ctx->widget->priv.size);

#if 0
		if (fseek(ctx->file, ctx->widget->priv.size, SEEK_CUR)) {
			SNDERR("error: fseek unsupported widget\n");
			ret = -errno;
			goto exit;
		}
		ret = tplg_create_controls(ctx->widget->num_kcontrols, ctx->file, NULL, 0);
		if (ret < 0) {
			SNDERR("error: loading controls\n");
			//goto exit;
		}
#endif
		ret = 0;
		break;
	}

	ret = 1;

exit:
	return ret;
}

static int plug_register_graph(snd_sof_plug_t *plug, int count)
{
	struct tplg_context *ctx = &plug->tplg;
	int ret = 0;
	int i;

	for (i = 0; i < count; i++) {
		ret = tplg_parse_graph(ctx, &plug->widget_list, &plug->route_list);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static int plug_parse_pcm(snd_sof_plug_t *plug, int count)
{
	struct tplg_context *ctx = &plug->tplg;
	int ret, i;

	for (i = 0; i < count; i++) {
		ret = tplg_parse_pcm(ctx, &plug->widget_list, &plug->pcm_list);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static void
plug_pipeline_update_resource_usage(snd_sof_plug_t *plug, struct tplg_comp_info *comp_info)
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

static int plug_is_single_format(struct sof_ipc4_pin_format *fmts, int num_formats)
{
	struct sof_ipc4_pin_format *fmt = &fmts[0];
	uint32_t _rate, _channels, _valid_bits;
	int i;

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

static int plug_match_audio_format(snd_sof_plug_t *plug, struct tplg_comp_info *comp_info,
				   struct plug_config *config)
{
	struct sof_ipc4_available_audio_format *available_fmt = &comp_info->available_fmt;
	struct ipc4_base_module_cfg *base_cfg = &comp_info->basecfg;
	struct sof_ipc4_pin_format *fmt;
	int config_valid_bits;
	int i;

	switch (config->format) {
	case SND_PCM_FORMAT_S16_LE:
		config_valid_bits = 16;
		break;
	case SND_PCM_FORMAT_S32_LE:
		config_valid_bits = 32;
		break;
	case SND_PCM_FORMAT_S24_LE:
		config_valid_bits = 24;
		break;
	default:
		break;
	}

	if (plug_is_single_format(available_fmt->input_pin_fmts,
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
		SNDERR("Cannot find matching format for rate %d channels %d valid_bits %d for %s\n",
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
	base_cfg->ibs = fmt->buffer_size;

	/*
	 * FIXME: is this correct? Choose ALSA period size for obs so that the buffer sizes
	 * will set accodingly. Need to get channel count and format from output format
	 */
	base_cfg->obs = plug->period_size * 2 * 2;

	return 0;
}

static int plug_set_up_widget_base_config(snd_sof_plug_t *plug, struct tplg_comp_info *comp_info)
{
	struct plug_cmdline_item *cmd_item = &plug->cmdline[0];
	struct plug_config *config;
	bool config_found = false;
	int ret, i;

	for (i < 0; i < plug->num_configs; i++) {
		config = &plug->config[i];

		if (!strcmp(config->name, cmd_item->config_name)) {
			config_found = true;
			break;
		}
	}

	if (!config_found) {
		SNDERR("unsupported config requested %s\n", cmd_item->config_name);
		return -ENOTSUP;
	}

	/* match audio formats and populate base config */
	ret = plug_match_audio_format(plug, comp_info, config);
	if (ret < 0)
		return ret;

	/* copy the basecfg into the ipc payload */
	memcpy(comp_info->ipc_payload, &comp_info->basecfg, sizeof(struct ipc4_base_module_cfg));

	return 0;
}

/* parse topology file and set up pipeline */
int plug_parse_topology(snd_sof_plug_t *plug)

{
	struct tplg_context *ctx = &plug->tplg;
	struct snd_soc_tplg_hdr *hdr;
	struct list_item *item;
	char pipeline_string[256] = {0};
	int i;
	int ret = 0;
	FILE *file;
	size_t size;

	tplg_debug("parsing topology file %s\n", ctx->tplg_file);

	/* TODO: ctl callback */
//	ctx->ctl_arg = sp;
//	ctx->ctl_cb = pipe_kcontrol_cb_new;

	/* open topology file */
	file = fopen(ctx->tplg_file, "rb");
	if (!file) {
		SNDERR("error: can't open topology %s : %s\n", ctx->tplg_file, strerror(errno));
		return -errno;
	}

	/* file size */
	if (fseek(file, 0, SEEK_END)) {
		SNDERR("error: can't seek to end of topology: %s\n", strerror(errno));
		fclose(file);
		return -errno;
	}
	ctx->tplg_size = ftell(file);
	if (fseek(file, 0, SEEK_SET)) {
		SNDERR("error: can't seek to beginning of topology: %s\n", strerror(errno));
		fclose(file);
		return -errno;
	}

	/* load whole topology into memory */
	ctx->tplg_base = calloc(ctx->tplg_size, 1);
	if (!ctx->tplg_base) {
		SNDERR("error: can't alloc buffer for topology %zu bytes\n", ctx->tplg_size);
		fclose(file);
		return -ENOMEM;
	}
	ret = fread(ctx->tplg_base, ctx->tplg_size, 1, file);
	if (ret != 1) {
		SNDERR("error: can't read topology: %s\n",
		       strerror(errno));
		fclose(file);
		return -errno;
	}
	fclose(file);

	/* initialize widget, route, pipeline and pcm lists */
	list_init(&plug->widget_list);
	list_init(&plug->route_list);
	list_init(&plug->pcm_list);
	list_init(&plug->pipeline_list);

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
				ret = plug_load_widget(plug);
				if (ret < 0) {
					SNDERR("error: loading widget\n");
					return ret;
				}
				ctx->comp_id++;
			}
			break;
		/* set up component connections from pipeline graph */
		case SND_SOC_TPLG_TYPE_DAPM_GRAPH:
			if (plug_register_graph(plug, hdr->count) < 0) {
				SNDERR("error: pipeline graph\n");
				return -EINVAL;
			}
			break;
		/* parse PCM info */
		case SND_SOC_TPLG_TYPE_PCM:
			ret = plug_parse_pcm(plug, hdr->count);
			if (ret < 0)
				goto out;
			break;
		default:
			tplg_debug("%s %d\n", __func__, __LINE__);
			tplg_skip_hdr_payload(ctx);
			break;
		}
	}

	/* assign pipeline to every widget in the widget list */
	list_for_item(item, &plug->widget_list) {
		struct tplg_comp_info *comp_info = container_of(item, struct tplg_comp_info, item);
		struct list_item *pipe_item;

		list_for_item(pipe_item, &plug->pipeline_list) {
			struct tplg_pipeline_info *pipe_info;

			pipe_info = container_of(pipe_item, struct tplg_pipeline_info, item);
			if (pipe_info->id == comp_info->pipeline_id) {
				comp_info->pipe_info = pipe_info;
				break;
			}
		}

		if (!comp_info->pipe_info) {
			SNDERR("Error assigning pipeline for %s\n", comp_info->name);
			return -EINVAL;
		}
	}
out:
	return ret;
}

static int plug_set_up_widget_ipc(snd_sof_plug_t *plug, struct tplg_comp_info *comp_info)
{
	struct ipc4_module_init_instance *module_init = &comp_info->module_init;
	struct ipc4_message_reply reply;
	void *msg;
	int size, ret;

	module_init->extension.r.param_block_size = comp_info->ipc_size >> 2;
	module_init->extension.r.ppl_instance_id = comp_info->pipe_info->instance_id;

	size = sizeof(*module_init) + comp_info->ipc_size;
	msg = calloc(size, 1);
	if (!msg)
		return -ENOMEM;

	memcpy(msg, module_init, sizeof(*module_init));
	memcpy(msg + sizeof(*module_init), comp_info->ipc_payload, comp_info->ipc_size);

	ret = plug_mq_cmd_tx_rx(&plug->ipc_tx, &plug->ipc_rx,
				msg, size, &reply, sizeof(reply));
	free(msg);
	if (ret < 0) {
		SNDERR("error: can't set up widget %s\n", comp_info->name);
		return ret;
	}

	if (reply.primary.r.status != IPC4_SUCCESS) {
		SNDERR("widget %s set up failed with status %d\n",
		       comp_info->name, reply.primary.r.status);
		return -EINVAL;
	}
	return 0;
}

static int plug_set_up_pipeline(snd_sof_plug_t *plug, struct tplg_pipeline_info *pipe_info)
{
	struct ipc4_pipeline_create msg;
	struct ipc4_message_reply reply;
	int ret;

	msg.primary.r.type = SOF_IPC4_GLB_CREATE_PIPELINE;
	msg.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
	msg.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	pipe_info->instance_id = plug->instance_ids[SND_SOC_TPLG_DAPM_SCHEDULER]++;
	msg.primary.r.instance_id = pipe_info->instance_id;
	msg.primary.r.ppl_mem_size = pipe_info->mem_usage;

	ret = plug_mq_cmd_tx_rx(&plug->ipc_tx, &plug->ipc_rx,
				&msg, sizeof(msg), &reply, sizeof(reply));
	if (ret < 0) {
		SNDERR("error: can't set up pipeline %s\n", pipe_info->name);
		return ret;
	}

	if (reply.primary.r.status != IPC4_SUCCESS) {
		SNDERR("pipeline %s instance ID %d set up failed with status %d\n",
		       pipe_info->name, pipe_info->instance_id, reply.primary.r.status);
		return -EINVAL;
	}

	tplg_debug("pipeline %s instance_id %d mem_usage %d set up\n", pipe_info->name,
		   pipe_info->instance_id, pipe_info->mem_usage);

	return 0;
}

static int plug_prepare_widget(snd_sof_plug_t *plug, struct tplg_pcm_info *pcm_info,
			       struct tplg_comp_info *comp_info, int dir)
{
	struct tplg_pipeline_list *pipeline_list;
	int ret, i;

	if (dir)
		pipeline_list = &pcm_info->capture_pipeline_list;
	else
		pipeline_list = &pcm_info->playback_pipeline_list;

	/* populate base config */
	ret = plug_set_up_widget_base_config(plug, comp_info);
	if (ret < 0)
		return ret;

	plug_pipeline_update_resource_usage(plug, comp_info);

	/* add pipeline to pcm pipeline_list if needed */
	for (i = 0; i < pipeline_list->count; i++) {
		struct tplg_pipeline_info *pipe_info = pipeline_list->pipelines[i];

		if (pipe_info == comp_info->pipe_info)
			break;
	}

	if (i == pipeline_list->count) {
		pipeline_list->pipelines[pipeline_list->count] = comp_info->pipe_info;
		pipeline_list->count++;
	}

	tplg_debug("widget %s prepared\n", comp_info->name);
	return 0;
}

static int plug_prepare_widgets(snd_sof_plug_t *plug, struct tplg_pcm_info *pcm_info,
				struct tplg_comp_info *starting_comp_info,
				struct tplg_comp_info *current_comp_info)
{
	struct list_item *item;
	int ret;

	/* for playback */
	list_for_item(item, &plug->route_list) {
		struct tplg_route_info *route_info = container_of(item, struct tplg_route_info,
								  item);

		if (route_info->source != current_comp_info)
			continue;

		/* set up source widget if it is the starting widget */
		if (starting_comp_info == current_comp_info) {
			ret = plug_prepare_widget(plug, pcm_info, current_comp_info, 0);
			if (ret < 0)
				return ret;
		}

		/* set up the sink widget */
		ret = plug_prepare_widget(plug, pcm_info, route_info->sink, 0);
		if (ret < 0)
			return ret;

		/* and then continue down the path */
		if (route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_IN ||
		    route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_OUT) {
			ret = plug_prepare_widgets(plug, pcm_info, starting_comp_info,
						   route_info->sink);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int plug_prepare_widgets_capture(snd_sof_plug_t *plug, struct tplg_pcm_info *pcm_info,
					struct tplg_comp_info *starting_comp_info,
					struct tplg_comp_info *current_comp_info)
{
	struct list_item *item;
	int ret;

	/* for playback */
	list_for_item(item, &plug->route_list) {
		struct tplg_route_info *route_info = container_of(item, struct tplg_route_info,
								  item);

		if (route_info->sink != current_comp_info)
			continue;

		/* set up sink widget if it is the starting widget */
		if (starting_comp_info == current_comp_info) {
			ret = plug_prepare_widget(plug, pcm_info, current_comp_info, 1);
			if (ret < 0)
				return ret;
		}

		/* set up the source widget */
		ret = plug_prepare_widget(plug, pcm_info, route_info->source, 1);
		if (ret < 0)
			return ret;

		/* and then continue up the path */
		if (route_info->source->type != SND_SOC_TPLG_DAPM_DAI_IN &&
		    route_info->source->type != SND_SOC_TPLG_DAPM_DAI_OUT) {
			ret = plug_prepare_widgets(plug, pcm_info, starting_comp_info,
						   route_info->source);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int plug_set_up_route(snd_sof_plug_t *plug, struct tplg_route_info *route_info)
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

	ret = plug_mq_cmd_tx_rx(&plug->ipc_tx, &plug->ipc_rx,
				&bu, sizeof(bu), &reply, sizeof(reply));
	if (ret < 0) {
		SNDERR("error: can't set up route %s -> %s\n", src_comp_info->name,
		       sink_comp_info->name);
		return ret;
	}

	if (reply.primary.r.status != IPC4_SUCCESS) {
		SNDERR("route %s -> %s ID set up failed with status %d\n",
		       src_comp_info->name, sink_comp_info->name, reply.primary.r.status);
		return -EINVAL;
	}

	tplg_debug("route %s -> %s set up\n", src_comp_info->name, sink_comp_info->name);

	return 0;
}

static int plug_set_up_widget(snd_sof_plug_t *plug, struct tplg_comp_info *comp_info)
{
	struct tplg_pipeline_info *pipe_info = comp_info->pipe_info;
	int ret;

	pipe_info->usage_count++;

	/* first set up pipeline if needed, only done once for the first pipeline widget */
	if (pipe_info->usage_count == 1) {
		ret = plug_set_up_pipeline(plug, pipe_info);
		if (ret < 0) {
			pipe_info->usage_count--;
			return ret;
		}
	}

	/* now set up the widget */
	ret = plug_set_up_widget_ipc(plug, comp_info);
	if (ret < 0)
		return ret;

	tplg_debug("widget %s set up\n", comp_info->name);

	return 0;
}

static int plug_set_up_widgets(snd_sof_plug_t *plug, struct tplg_comp_info *starting_comp_info,
			       struct tplg_comp_info *current_comp_info)
{
	struct list_item *item;
	int ret;

	/* for playback */
	list_for_item(item, &plug->route_list) {
		struct tplg_route_info *route_info = container_of(item, struct tplg_route_info,
								  item);

		if (route_info->source != current_comp_info)
			continue;

		/* set up source widget if it is the starting widget */
		if (starting_comp_info == current_comp_info) {
			ret = plug_set_up_widget(plug, current_comp_info);
			if (ret < 0)
				return ret;
		}

		/* set up the sink widget */
		ret = plug_set_up_widget(plug, route_info->sink);
		if (ret < 0)
			return ret;

		/* source and sink widgets are up, so set up route now */
		ret = plug_set_up_route(plug, route_info);
		if (ret < 0)
			return ret;

		/* and then continue down the path */
		if (route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_IN ||
		    route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_OUT) {
			ret = plug_set_up_widgets(plug, starting_comp_info, route_info->sink);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int plug_set_up_widgets_capture(snd_sof_plug_t *plug,
				       struct tplg_comp_info *starting_comp_info,
				       struct tplg_comp_info *current_comp_info)
{
	struct list_item *item;
	int ret;

	/* for playback */
	list_for_item(item, &plug->route_list) {
		struct tplg_route_info *route_info = container_of(item, struct tplg_route_info,
								  item);

		if (route_info->sink != current_comp_info)
			continue;

		/* set up source widget if it is the starting widget */
		if (starting_comp_info == current_comp_info) {
			ret = plug_set_up_widget(plug, current_comp_info);
			if (ret < 0)
				return ret;
		}

		/* set up the sink widget */
		ret = plug_set_up_widget(plug, route_info->source);
		if (ret < 0)
			return ret;

		/* source and sink widgets are up, so set up route now */
		ret = plug_set_up_route(plug, route_info);
		if (ret < 0)
			return ret;

		/* and then continue down the path */
		if (route_info->source->type != SND_SOC_TPLG_DAPM_DAI_IN &&
		    route_info->source->type != SND_SOC_TPLG_DAPM_DAI_OUT) {
			ret = plug_set_up_widgets(plug, starting_comp_info, route_info->source);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

int plug_set_up_pipelines(snd_sof_plug_t *plug, int dir)
{
	struct tplg_comp_info *host = NULL;
	struct tplg_pcm_info *pcm_info;
	struct list_item *item;
	int ret;

	list_for_item(item, &plug->pcm_list) {
		pcm_info = container_of(item, struct tplg_pcm_info, item);

		if (pcm_info->id == plug->pcm_id) {
			if (dir)
				host = pcm_info->capture_host;
			else
				host = pcm_info->playback_host;
			break;
		}
	}

	if (!host) {
		SNDERR("No host component found for PCM ID: %d\n", plug->pcm_id);
		return -EINVAL;
	}

	plug->pcm_info = pcm_info;

	if (dir) {
		ret = plug_prepare_widgets_capture(plug, pcm_info, host, host);
		if (ret < 0)
			return ret;

		ret = plug_set_up_widgets_capture(plug, host, host);
		if (ret < 0)
			return ret;

		tplg_debug("Setting up capture pipelines complete\n");

		return 0;
	}

	ret = plug_prepare_widgets(plug, pcm_info, host, host);
	if (ret < 0)
		return ret;

	ret = plug_set_up_widgets(plug, host, host);
	if (ret < 0)
		return ret;

	tplg_debug("Setting up playback pipelines complete\n");

	return 0;
}

static int plug_delete_pipeline(snd_sof_plug_t *plug, struct tplg_pipeline_info *pipe_info)
{
	struct ipc4_pipeline_delete msg;
	struct ipc4_message_reply reply;
	int ret;

	msg.primary.r.type = SOF_IPC4_GLB_DELETE_PIPELINE;
	msg.primary.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
	msg.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	msg.primary.r.instance_id = pipe_info->instance_id;

	ret = plug_mq_cmd_tx_rx(&plug->ipc_tx, &plug->ipc_rx,
				&msg, sizeof(msg), &reply, sizeof(reply));
	if (ret < 0) {
		SNDERR("error: can't delete pipeline %s\n", pipe_info->name);
		return ret;
	}

	if (reply.primary.r.status != IPC4_SUCCESS) {
		SNDERR("pipeline %s instance ID %d delete failed with status %d\n",
		       pipe_info->name, pipe_info->instance_id, reply.primary.r.status);
		return -EINVAL;
	}

	tplg_debug("pipeline %s instance_id %d freed\n", pipe_info->name,
		   pipe_info->instance_id);

	return 0;
}

static int plug_free_route(snd_sof_plug_t *plug, struct tplg_route_info *route_info)
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

	ret = plug_mq_cmd_tx_rx(&plug->ipc_tx, &plug->ipc_rx,
				&bu, sizeof(bu), &reply, sizeof(reply));
	if (ret < 0) {
		SNDERR("error: can't set up route %s -> %s\n", src_comp_info->name,
		       sink_comp_info->name);
		return ret;
	}

	if (reply.primary.r.status != IPC4_SUCCESS) {
		SNDERR("route %s -> %s ID set up failed with status %d\n",
		       src_comp_info->name, sink_comp_info->name, reply.primary.r.status);
		return -EINVAL;
	}

	tplg_debug("route %s -> %s freed\n", src_comp_info->name, sink_comp_info->name);

	return 0;
}

static int plug_free_widgets(snd_sof_plug_t *plug, struct tplg_comp_info *starting_comp_info,
			     struct tplg_comp_info *current_comp_info)
{
	struct list_item *item;
	int ret;

	/* for playback */
	list_for_item(item, &plug->route_list) {
		struct tplg_route_info *route_info = container_of(item, struct tplg_route_info,
								  item);

		if (route_info->source != current_comp_info)
			continue;

		/* Widgets will be freed when the pipeline is deleted, so just unbind modules */
		ret = plug_free_route(plug, route_info);
		if (ret < 0)
			return ret;

		/* and then continue down the path */
		if (route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_IN ||
		    route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_OUT) {
			ret = plug_free_widgets(plug, starting_comp_info, route_info->sink);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int plug_free_widgets_capture(snd_sof_plug_t *plug,
				     struct tplg_comp_info *starting_comp_info,
				     struct tplg_comp_info *current_comp_info)
{
	struct list_item *item;
	int ret;

	/* for playback */
	list_for_item(item, &plug->route_list) {
		struct tplg_route_info *route_info = container_of(item, struct tplg_route_info,
								  item);

		if (route_info->sink != current_comp_info)
			continue;

		/* Widgets will be freed when the pipeline is deleted, so just unbind modules */
		ret = plug_free_route(plug, route_info);
		if (ret < 0)
			return ret;

		/* and then continue down the path */
		if (route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_IN &&
		    route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_OUT) {
			ret = plug_free_widgets(plug, starting_comp_info, route_info->source);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

int plug_free_pipelines(snd_sof_plug_t *plug, struct tplg_pipeline_list *pipeline_list, int dir)
{
	struct tplg_comp_info *host = NULL;
	struct tplg_pcm_info *pcm_info;
	struct list_item *item;
	int ret, i;

	list_for_item(item, &plug->pcm_list) {
		pcm_info = container_of(item, struct tplg_pcm_info, item);

		if (pcm_info->id == plug->pcm_id) {
			host = pcm_info->playback_host; /* FIXME */
			break;
		}
	}

	if (!host) {
		SNDERR("No host component found for PCM ID: %d\n", plug->pcm_id);
		return -EINVAL;
	}

	if (dir) {
	} else {
		ret = plug_free_widgets(plug, host, host);
		if (ret < 0) {
			SNDERR("failed to free widgets for PCM %d\n", plug->pcm_id);
			return ret;
		}
	}

	for (i = 0; i < pipeline_list->count; i++) {
		struct tplg_pipeline_info *pipe_info = pipeline_list->pipelines[i];

		ret = plug_delete_pipeline(plug, pipe_info);
		if (ret < 0)
			return ret;
	}

	plug->instance_ids[SND_SOC_TPLG_DAPM_SCHEDULER] = 0;
	return 0;
}

void plug_free_topology(snd_sof_plug_t *plug)
{
	struct list_item *item, *_item;

	list_for_item_safe(item, _item, &plug->pcm_list) {
		struct tplg_pcm_info *pcm_info = container_of(item, struct tplg_pcm_info, item);

		free(pcm_info->name);
		free(pcm_info);
	}

	list_for_item_safe(item, _item, &plug->widget_list) {
		struct tplg_comp_info *comp_info = container_of(item, struct tplg_comp_info, item);

		free(comp_info->name);
		free(comp_info->stream_name);
		free(comp_info->ipc_payload);
		free(comp_info);
	}

	list_for_item_safe(item, _item, &plug->route_list) {
		struct tplg_route_info *route_info = container_of(item, struct tplg_route_info,
								  item);

		free(route_info);
	}

	list_for_item_safe(item, _item, &plug->pipeline_list) {
		struct tplg_pipeline_info *pipe_info = container_of(item, struct tplg_pipeline_info,
								    item);

		free(pipe_info->name);
		free(pipe_info);
	}

	tplg_debug("freed all pipelines, widgets, routes and pcms\n");
}
