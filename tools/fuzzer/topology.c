// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

/* Topology loader to set up components and pipeline */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <ipc/topology.h>
#include <ipc/stream.h>
#include <sof/common.h>
#include <tplg_parser/topology.h>
#include <tplg_parser/tokens.h>

#include "../fuzzer/fuzzer.h"

struct ipc;

static const struct sof_dai_types sof_dais[] = {
	{"SSP", SOF_DAI_INTEL_SSP},
	{"HDA", SOF_DAI_INTEL_HDA},
	{"DMIC", SOF_DAI_INTEL_DMIC},
};

/* find dai type */
enum sof_ipc_dai_type find_dai(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sof_dais); i++) {
		if (strcmp(name, sof_dais[i].name) == 0)
			return sof_dais[i].type;
	}

	return SOF_DAI_INTEL_NONE;
}

void register_comp(int comp_type, struct sof_ipc_comp_ext *comp_ext) {}

int ipc_pipeline_complete(struct ipc *ipc, uint32_t comp_id)
{
	return 0;
}

int ipc_comp_connect(struct ipc *ipc, ipc_pipe_comp_connect *connect)
{
	return 0;
}

int ipc_buffer_new(struct ipc *ipc, const struct sof_ipc_buffer *desc)
{
	return 0;
}

int ipc_pipeline_new(struct ipc *ipc, ipc_pipe_new *pipeline)
{
	return 0;
}

int ipc_comp_new(struct ipc *ipc, ipc_comp *new)
{
	return 0;
}

int find_widget(struct comp_info *temp_comp_list, int count, char *name)
{
	int i;

	for (i = 0; i < count; i++) {
		if (!strcmp(temp_comp_list[i].name, name))
			return temp_comp_list[i].id;
	}

	return -EINVAL;
}

static int fuzzer_complete_pipeline(struct fuzz *fuzzer, uint32_t comp_id)
{
	struct sof_ipc_pipe_ready ready;
	struct sof_ipc_reply r;
	int ret;

	fprintf(stdout, "tplg: complete pipeline id %d\n", comp_id);

	ready.hdr.size = sizeof(ready);
	ready.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_PIPE_COMPLETE;
	ready.comp_id = comp_id;

	/* configure fuzzer msg */
	fuzzer->msg.header = ready.hdr.cmd;
	memcpy(fuzzer->msg.msg_data, &ready, ready.hdr.size);
	fuzzer->msg.msg_size = sizeof(ready);
	fuzzer->msg.reply_size = sizeof(r);

	ret = fuzzer_send_msg(fuzzer);
	if (ret < 0)
		return ret;

	return 1;
}

/* load pipeline graph DAPM widget*/
static int fuzzer_load_graph(void *dev, struct comp_info *temp_comp_list,
		      int count, int num_comps, int pipeline_id)
{
	struct sof_ipc_pipe_comp_connect connection;
	struct fuzz *fuzzer = (struct fuzz *)dev;
	struct sof_ipc_reply r;
	char pipeline_string[DEBUG_MSG_LEN];
	int ret = 0;
	int i;

	for (i = 0; i < count; i++) {
		ret = tplg_create_graph(num_comps, pipeline_id, temp_comp_list,
				      pipeline_string, &connection,
				      fuzzer->tplg_file, i, count);
		if (ret < 0)
			return ret;

		/* configure fuzzer msg */
		fuzzer->msg.header = connection.hdr.cmd;
		memcpy(fuzzer->msg.msg_data, &connection, connection.hdr.size);
		fuzzer->msg.msg_size = sizeof(connection);
		fuzzer->msg.reply_size = sizeof(r);

		ret = fuzzer_send_msg(fuzzer);
		if (ret < 0)
			fprintf(stderr, "error: message tx failed\n");
	}

	return ret;
}

/* load buffer DAPM widget */
static int fuzzer_load_buffer(struct tplg_context *ctx)
{
	struct sof_ipc_buffer buffer;
	struct fuzz *fuzzer = ctx->fuzzer;
	struct sof_ipc_comp_reply r;
	int ret;

	ret = tplg_create_buffer(ctx, &buffer);
	if (ret < 0)
		return ret;

	/* configure fuzzer msg */
	fuzzer->msg.header = buffer.comp.hdr.cmd;
	memcpy(fuzzer->msg.msg_data, &buffer, buffer.comp.hdr.size);
	fuzzer->msg.msg_size = sizeof(buffer);
	fuzzer->msg.reply_size = sizeof(r);

	/* load volume component */
	ret = fuzzer_send_msg(fuzzer);
	if (ret < 0)
		fprintf(stderr, "error: message tx failed\n");

	return 0;
}

/* load pcm component */
static int fuzzer_load_pcm(struct tplg_context *ctx, int dir)
{
	struct fuzz *fuzzer = ctx->fuzzer;
	struct sof_ipc_comp_host host;
	struct sof_ipc_comp_reply r;
	int ret;

	ret = tplg_create_pcm(ctx, dir, &host);
	if (ret < 0)
		return ret;

	/* configure fuzzer msg */
	fuzzer->msg.header = host.comp.hdr.cmd;
	memcpy(fuzzer->msg.msg_data, &host, host.comp.hdr.size);
	fuzzer->msg.msg_size = sizeof(host);
	fuzzer->msg.reply_size = sizeof(r);

	/* load volume component */
	ret = fuzzer_send_msg(fuzzer);
	if (ret < 0)
		fprintf(stderr, "error: message tx failed\n");
	return 0;
}

int load_aif_in_out(struct tplg_context *ctx, int dir)
{
	return fuzzer_load_pcm(ctx, dir);
}

/* load dai component */
static int fuzzer_load_dai(struct tplg_context *ctx)
{
	struct fuzz *fuzzer = ctx->fuzzer;
	struct sof_ipc_comp_dai comp_dai;
	struct sof_ipc_comp_reply r;
	int ret;

	ret = tplg_create_dai(ctx, &comp_dai);
	if (ret < 0)
		return ret;

	/* configure fuzzer msg */
	fuzzer->msg.header = comp_dai.comp.hdr.cmd;
	memcpy(fuzzer->msg.msg_data, &comp_dai, comp_dai.comp.hdr.size);
	fuzzer->msg.msg_size = sizeof(comp_dai);
	fuzzer->msg.reply_size = sizeof(r);

	/* load volume component */
	ret = fuzzer_send_msg(fuzzer);
	if (ret < 0)
		fprintf(stderr, "error: message tx failed\n");

	return 0;
}

int load_dai_in_out(struct tplg_context *ctx, int dir)
{
	return fuzzer_load_dai(ctx);
}

/* load pda dapm widget */
static int fuzzer_load_pga(struct tplg_context *ctx)
{
	struct fuzz *fuzzer = ctx->fuzzer;
	struct sof_ipc_comp_volume volume;
	struct sof_ipc_comp_reply r;
	int ret = 0;

	ret = tplg_create_pga(ctx, &volume, sizeof(volume));
	if (ret < 0)
		return ret;

	/* configure fuzzer msg */
	fuzzer->msg.header = volume.comp.hdr.cmd;
	memcpy(fuzzer->msg.msg_data, &volume, volume.comp.hdr.size);
	fuzzer->msg.msg_size = sizeof(volume);
	fuzzer->msg.reply_size = sizeof(r);

	ret = fuzzer_send_msg(fuzzer);
	if (ret < 0)
		fprintf(stderr, "error: message tx failed\n");

	return 0;
}

/* load scheduler dapm widget */
static int fuzzer_load_pipeline(struct tplg_context *ctx)
{
	struct sof_ipc_pipe_new pipeline;
	struct fuzz *fuzzer = ctx->fuzzer;
	struct sof_ipc_comp_reply r;
	int ret;

	ret = tplg_create_pipeline(ctx, &pipeline);
	if (ret < 0)
		return ret;

	pipeline.sched_id = ctx->sched_id;

	/* configure fuzzer msg */
	fuzzer->msg.header = pipeline.hdr.cmd;
	memcpy(fuzzer->msg.msg_data, &pipeline, pipeline.hdr.size);
	fuzzer->msg.msg_size = sizeof(pipeline);
	fuzzer->msg.reply_size = sizeof(r);

	/* load volume component */
	ret = fuzzer_send_msg(fuzzer);
	if (ret < 0)
		fprintf(stderr, "error: message tx failed\n");

	return 0;
}

/* load src dapm widget */
static int fuzzer_load_src(struct tplg_context *ctx)
{
	struct fuzz *fuzzer = ctx->fuzzer;
	struct sof_ipc_comp_src src = {0};
	struct sof_ipc_comp_reply r;
	int ret = 0;

	ret = tplg_create_src(ctx, &src, sizeof(src));
	if (ret < 0)
		return ret;

	/* configure fuzzer msg */
	fuzzer->msg.header = src.comp.hdr.cmd;
	memcpy(fuzzer->msg.msg_data, &src, src.comp.hdr.size);
	fuzzer->msg.msg_size = sizeof(src);
	fuzzer->msg.reply_size = sizeof(r);

	/* load volume component */
	ret = fuzzer_send_msg(fuzzer);
	if (ret < 0)
		fprintf(stderr, "error: message tx failed\n");

	return ret;
}

/* load asrc dapm widget */
static int fuzzer_load_asrc(struct tplg_context *ctx)
{
	struct fuzz *fuzzer = ctx->fuzzer;
	struct sof_ipc_comp_asrc asrc = {0};
	struct sof_ipc_comp_reply r;
	int ret = 0;

	ret = tplg_create_asrc(ctx, &asrc, sizeof(asrc));
	if (ret < 0)
		return ret;

	/* configure fuzzer msg */
	fuzzer->msg.header = asrc.comp.hdr.cmd;
	memcpy(fuzzer->msg.msg_data, &asrc, asrc.comp.hdr.size);
	fuzzer->msg.msg_size = sizeof(asrc);
	fuzzer->msg.reply_size = sizeof(r);

	/* load asrc component */
	ret = fuzzer_send_msg(fuzzer);
	if (ret < 0)
		fprintf(stderr, "error: message tx failed\n");

	return ret;
}

/* load mixer dapm widget */
static int fuzzer_load_mixer(struct tplg_context *ctx)
{
	struct fuzz *fuzzer = ctx->fuzzer;
	struct sof_ipc_comp_mixer mixer = {0};
	struct sof_ipc_comp_reply r;
	int ret = 0;

	ret = tplg_create_mixer(ctx, &mixer, sizeof(mixer));
	if (ret < 0)
		return ret;

	/* configure fuzzer msg */
	fuzzer->msg.header = mixer.comp.hdr.cmd;
	memcpy(fuzzer->msg.msg_data, &mixer, mixer.comp.hdr.size);
	fuzzer->msg.msg_size = sizeof(mixer);
	fuzzer->msg.reply_size = sizeof(r);

	/* load volume component */
	ret = fuzzer_send_msg(fuzzer);
	if (ret < 0)
		fprintf(stderr, "error: message tx failed\n");

	return ret;
}

/* load effect dapm widget */
static int fuzzer_load_process(struct tplg_context *ctx)
{
	return -EINVAL; /* Not implemented */
}

/* load dapm widget */
static int fuzzer_load_widget(struct tplg_context *ctx)
{
	struct comp_info *temp_comp_list = ctx->info;
	int comp_index = ctx->info_index;
	int comp_id = ctx->comp_id;
	int ret = 0;
	int dev_type = ctx->dev_type;

	if (!temp_comp_list) {
		fprintf(stderr, "load_widget: temp_comp_list argument NULL\n");
		return -EINVAL;
	}

	/* allocate memory for widget */
	ctx->widget_size = sizeof(struct snd_soc_tplg_dapm_widget);
	ctx->widget = malloc(ctx->widget_size);
	if (!ctx->widget) {
		fprintf(stderr, "error: mem alloc\n");
		return -errno;
	}

	/* read widget data */
	ret = fread(ctx->widget, ctx->widget_size, 1, ctx->file);
	if (ret != 1) {
		ret = -EINVAL;
		goto exit;
	}

	/*
	 * create a list with all widget info
	 * containing mapping between component names and ids
	 * which will be used for setting up component connections
	 */
	temp_comp_list[comp_index].id = comp_id;
	temp_comp_list[comp_index].name = strdup(ctx->widget->name);
	temp_comp_list[comp_index].type = ctx->widget->id;
	temp_comp_list[comp_index].pipeline_id = ctx->pipeline_id;

	printf("debug: loading comp_id %d: widget %s id %d\n",
	       comp_id, ctx->widget->name, ctx->widget->id);

	/* load widget based on type */
	switch (ctx->widget->id) {

	/* load pga widget */
	case SND_SOC_TPLG_DAPM_PGA:
		if (fuzzer_load_pga(ctx) < 0) {
			fprintf(stderr, "error: load pga\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_AIF_IN:
		if (load_aif_in_out(ctx, SOF_IPC_STREAM_PLAYBACK) < 0) {
			fprintf(stderr, "error: load AIF IN failed\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_AIF_OUT:
		if (load_aif_in_out(ctx, SOF_IPC_STREAM_CAPTURE) < 0) {
			fprintf(stderr, "error: load AIF OUT failed\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_DAI_IN:
		if (load_dai_in_out(ctx, SOF_IPC_STREAM_PLAYBACK) < 0) {
			fprintf(stderr, "error: load filewrite\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_DAI_OUT:
		if (load_dai_in_out(ctx, SOF_IPC_STREAM_CAPTURE) < 0) {
			fprintf(stderr, "error: load filewrite\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_BUFFER:
		if (fuzzer_load_buffer(ctx) < 0) {
			fprintf(stderr, "error: load buffer\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_SCHEDULER:
		/* find comp id for scheduling comp */
		if (dev_type == FUZZER_DEV)
			ctx->sched_id = find_widget(temp_comp_list, comp_id, ctx->widget->sname);

		if (fuzzer_load_pipeline(ctx) < 0) {
			fprintf(stderr, "error: load pipeline\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_SRC:
		if (fuzzer_load_src(ctx) < 0) {
			fprintf(stderr, "error: load src\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_ASRC:
		if (fuzzer_load_asrc(ctx) < 0) {
			fprintf(stderr, "error: load src\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_MIXER:
		if (fuzzer_load_mixer(ctx) < 0) {
			fprintf(stderr, "error: load mixer\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_EFFECT:
		if (fuzzer_load_process(ctx) < 0) {
			fprintf(stderr, "error: load effect\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	/* unsupported widgets */
	default:
		if (fseek(ctx->file, ctx->widget->priv.size, SEEK_CUR)) {
			fprintf(stderr, "error: fseek unsupported widget\n");
			ret = -errno;
			goto exit;
		}

		printf("info: Widget type not supported %d\n", ctx->widget->id);
		ret = tplg_create_controls(ctx->widget->num_kcontrols, ctx->file, NULL, 0);
		if (ret < 0) {
			fprintf(stderr, "error: loading controls\n");
			goto exit;
		}
		ret = 0;
		break;
	}

	ret = 1;

exit:
	/* free allocated widget data */
	free(ctx->widget);
	return ret;
}

/* parse topology file and set up pipeline */
int fuzzer_parse_topology(struct tplg_context *ctx)
{
	struct snd_soc_tplg_hdr *hdr;
	struct fuzz *fuzzer = ctx->fuzzer;
	struct comp_info *comp_list_realloc = NULL;
	char message[DEBUG_MSG_LEN];
	int num_comps = 0;
	int i, ret = 0;
	size_t file_size, size;

	/* open topology file */
	ctx->file = fopen(ctx->tplg_file, "rb");
	if (!ctx->file) {
		fprintf(stderr, "error: opening file %s\n", ctx->tplg_file);
		return -errno;
	}
	fuzzer->tplg_file = ctx->file;

	/* file size */
	if (fseek(ctx->file, 0, SEEK_END)) {
		fprintf(stderr, "error: seek to end of topology\n");
		fclose(ctx->file);
		return -errno;
	}
	file_size = ftell(ctx->file);
	if (fseek(ctx->file, 0, SEEK_SET)) {
		fprintf(stderr, "error: seek to beginning of topology\n");
		fclose(ctx->file);
		return -errno;
	}

	/* allocate memory */
	size = sizeof(struct snd_soc_tplg_hdr);
	hdr = (struct snd_soc_tplg_hdr *)malloc(size);
	if (!hdr) {
		fprintf(stderr, "error: mem alloc\n");
		return -EINVAL;
	}

	fprintf(stdout, "debug: %s", "topology parsing start\n");

	while (1) {
		/* read topology header */
		ret = fread(hdr, sizeof(struct snd_soc_tplg_hdr), 1,
			    fuzzer->tplg_file);
		if (ret != 1)
			return -EINVAL;

		sprintf(message, "type: %x, size: 0x%x count: %d index: %d\n",
			hdr->type, hdr->payload_size, hdr->count, hdr->index);
		fprintf(stdout, "debug %s", message);

		/* parse header and load the next block based on type */
		switch (hdr->type) {
		/* load dapm widget */
		case SND_SOC_TPLG_TYPE_DAPM_WIDGET:
			sprintf(message, "number of DAPM widgets %d\n",
				hdr->count);
			fprintf(stdout, "debug %s\n", message);

			ctx->info_elems += hdr->count;
			size = sizeof(struct comp_info) * ctx->info_elems;
			comp_list_realloc = (struct comp_info *)
					 realloc(ctx->info, size);

			if (!comp_list_realloc && size) {
				fprintf(stderr, "error: mem realloc\n");
				return -ENOMEM;
			}
			ctx->info = comp_list_realloc;

			for (i = (ctx->info_elems - hdr->count); i < ctx->info_elems; i++)
				ctx->info[i].name = NULL;

			for (ctx->info_index = (ctx->info_elems - hdr->count);
			     ctx->info_index < ctx->info_elems;
			     ctx->info_index++) {
				ret = fuzzer_load_widget(ctx);
				if (ret < 0) {
					printf("error: loading widget\n");
					goto finish;
				} else if (ret > 0)
					ctx->comp_id++;
			}
			break;

		/* set up component connections from pipeline graph */
		case SND_SOC_TPLG_TYPE_DAPM_GRAPH:
			if (fuzzer_load_graph(fuzzer, comp_list_realloc, hdr->count,
				       num_comps, hdr->index) < 0) {
				fprintf(stderr, "error: pipeline graph\n");
				return -EINVAL;
			}
			if (ftell(fuzzer->tplg_file) == file_size)
				goto finish;
			break;
		default:
			fseek(fuzzer->tplg_file, hdr->payload_size, SEEK_CUR);
			if (ftell(fuzzer->tplg_file) == file_size)
				goto finish;
			break;
		}
	}
finish:
	/* pipeline complete after pipeline connections are established */
	for (i = 0; i < num_comps; i++)
		if (comp_list_realloc[i].type == SND_SOC_TPLG_DAPM_SCHEDULER)
			fuzzer_complete_pipeline(fuzzer, comp_list_realloc[i].id);

	fprintf(stdout, "debug: %s", "topology parsing end\n");

	/* free all data */
	free(hdr);

	for (i = 0; i < num_comps; i++)
		free(comp_list_realloc[i].name);

	free(comp_list_realloc);
	fclose(fuzzer->tplg_file);
	return 0;
}
