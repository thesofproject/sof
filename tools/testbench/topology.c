// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018-2024 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>

/* Topology loader to set up components and pipeline */

#include <sof/audio/component.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/topology.h>
#include <rtos/string.h>
#include <sof/common.h>
#include <sof/lib/uuid.h>
#include <tplg_parser/tokens.h>
#include <tplg_parser/topology.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "testbench/common_test.h"
#include "testbench/file.h"
#include "testbench/topology.h"
#include "testbench/topology_ipc4.h"

static int tb_new_src(struct testbench_prm *tb)
{
	struct tplg_context *ctx = &tb->tplg;
	char tplg_object[MAX_TPLG_OBJECT_SIZE] = {0};
	struct sof_ipc_comp_src *src = (struct sof_ipc_comp_src *)tplg_object;
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	int ret;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl)
		return -ENOMEM;

	ret = tplg_new_src(ctx, &src->comp, MAX_TPLG_OBJECT_SIZE, tplg_ctl, ctx->hdr->payload_size);
	if (ret < 0) {
		fprintf(stderr, "error: failed to create SRC\n");
		goto out;
	}

out:
	free(tplg_ctl);
	return ret;
}

static int tb_new_asrc(struct testbench_prm *tb)
{
	struct tplg_context *ctx = &tb->tplg;
	char tplg_object[MAX_TPLG_OBJECT_SIZE] = {0};
	struct sof_ipc_comp_asrc *asrc = (struct sof_ipc_comp_asrc *)tplg_object;
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	int ret;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl)
		return -ENOMEM;

	ret = tplg_new_asrc(ctx, &asrc->comp, MAX_TPLG_OBJECT_SIZE,
			    tplg_ctl, ctx->hdr->payload_size);
	if (ret < 0) {
		fprintf(stderr, "error: failed to create ASRC\n");
		goto out;
	}

out:
	free(tplg_ctl);
	return ret;
}

static int tb_new_mixer(struct testbench_prm *tb)
{
	struct tplg_context *ctx = &tb->tplg;
	struct tplg_comp_info *comp_info = ctx->current_comp_info;
	char tplg_object[MAX_TPLG_OBJECT_SIZE] = {0};
	struct sof_ipc_comp_mixer *mixer = (struct sof_ipc_comp_mixer *)tplg_object;
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	int ret;

	tplg_ctl = calloc(ctx->hdr->payload_size, 1);
	if (!tplg_ctl)
		return -ENOMEM;

	comp_info->instance_id = tb->instance_ids[SND_SOC_TPLG_DAPM_MIXER]++;
	comp_info->ipc_size = sizeof(struct ipc4_base_module_cfg);
	comp_info->ipc_payload = calloc(comp_info->ipc_size, 1);
	if (!comp_info->ipc_payload)
		return -ENOMEM;

	ret = tplg_new_mixer(ctx, &mixer->comp, MAX_TPLG_OBJECT_SIZE,
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

static int tb_new_pipeline(struct testbench_prm *tb)
{
	struct tplg_pipeline_info *pipe_info;
	struct sof_ipc_pipe_new pipeline = {0};
	struct snd_soc_tplg_ctl_hdr *tplg_ctl;
	struct tplg_context *ctx = &tb->tplg;
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
		fprintf(stderr, "error: failed to create pipeline\n");
		free(pipe_info->name);
		free(pipe_info);
		goto out;
	}

	list_item_append(&pipe_info->item, &tb->pipeline_list);
	tplg_debug("loading pipeline %s\n", pipe_info->name);
out:
	free(tplg_ctl);
	return ret;
}

static int tb_new_buffer(struct testbench_prm *tb)
{
	struct ipc4_copier_module_cfg *copier = calloc(sizeof(struct ipc4_copier_module_cfg), 1);
	struct tplg_context *ctx = &tb->tplg;
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

static int tb_register_graph(struct testbench_prm *tb, int count)
{
	struct tplg_context *ctx = &tb->tplg;
	int ret = 0;
	int i;

	for (i = 0; i < count; i++) {
		ret = tplg_parse_graph(ctx, &tb->widget_list, &tb->route_list);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static int tb_parse_pcm(struct testbench_prm *tb, int count)
{
	struct tplg_context *ctx = &tb->tplg;
	int ret, i;

	for (i = 0; i < count; i++) {
		ret = tplg_parse_pcm(ctx, &tb->widget_list, &tb->pcm_list);
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
		goto sname_err;
	}

	comp_info->id = comp_id;
	comp_info->type = ctx->widget->id;
	comp_info->pipeline_id = ctx->pipeline_id;
	ctx->current_comp_info = comp_info;

	// TODO IPC3
	if (ctx->ipc_major == 4) {
		ret = tb_parse_ipc4_comp_tokens(tp, &comp_info->basecfg);
		if (ret < 0)
			goto sname_err;
	}

	list_item_append(&comp_info->item, &tp->widget_list);

	printf("debug: loading comp_id %d: widget %s type %d size %d at offset %ld is_pages %d\n",
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
static int tb_load_widget(struct testbench_prm *tb)
{
	struct tplg_context *ctx = &tb->tplg;
	int ret = 0;

	/* get next widget */
	ctx->widget = tplg_get_widget(ctx);
	ctx->widget_size = ctx->widget->size;

	/* insert widget into mapping */
	ret = tb_insert_comp(tb);
	if (ret < 0) {
		fprintf(stderr, "tb_load_widget: invalid widget index\n");
		return ret;
	}

	/* load widget based on type */
	switch (tb->tplg.widget->id) {
	/* load pga widget */
	case SND_SOC_TPLG_DAPM_PGA:
		if (tb_new_pga(tb) < 0) {
			fprintf(stderr, "error: load pga\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_AIF_IN:
		if (tb_new_aif_in_out(tb, SOF_IPC_STREAM_PLAYBACK) < 0) {
			fprintf(stderr, "error: load AIF IN failed\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_AIF_OUT:
		if (tb_new_aif_in_out(tb, SOF_IPC_STREAM_CAPTURE) < 0) {
			fprintf(stderr, "error: load AIF OUT failed\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_DAI_IN:
		if (tb_new_dai_in_out(tb, SOF_IPC_STREAM_PLAYBACK) < 0) {
			fprintf(stderr, "error: load filewrite\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_DAI_OUT:
		if (tb_new_dai_in_out(tb, SOF_IPC_STREAM_CAPTURE) < 0) {
			fprintf(stderr, "error: load filewrite\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_BUFFER:
		if (tb_new_buffer(tb) < 0) {
			fprintf(stderr, "error: load buffer\n");
			ret = -EINVAL;
			goto exit;
		}
		break;

	case SND_SOC_TPLG_DAPM_SCHEDULER:
		if (tb_new_pipeline(tb) < 0) {
			fprintf(stderr, "error: load pipeline\n");
			ret = -EINVAL;
			goto exit;
		}
		break;

	case SND_SOC_TPLG_DAPM_SRC:
		if (tb_new_src(tb) < 0) {
			fprintf(stderr, "error: load src\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_ASRC:
		if (tb_new_asrc(tb) < 0) {
			fprintf(stderr, "error: load src\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_MIXER:
		if (tb_new_mixer(tb) < 0) {
			fprintf(stderr, "error: load mixer\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_EFFECT:
		if (tb_new_process(tb) < 0) {
			fprintf(stderr, "error: load effect\n");
			ret = -EINVAL;
			goto exit;
		}
		break;

	/* unsupported widgets */
	default:
		printf("info: Widget %s id %d unsupported and skipped: size %d priv size %d\n",
		       ctx->widget->name, ctx->widget->id,
		       ctx->widget->size, ctx->widget->priv.size);
		break;
	}

	ret = 1;

exit:
	return ret;
}

/* parse topology file and set up pipeline */
int tb_parse_topology(struct testbench_prm *tb)

{
	struct tplg_context *ctx = &tb->tplg;
	struct snd_soc_tplg_hdr *hdr;
	struct list_item *item;
	int i;
	int ret = 0;
	FILE *file;

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
	list_init(&tb->widget_list);
	list_init(&tb->route_list);
	list_init(&tb->pcm_list);
	list_init(&tb->pipeline_list);

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
				ret = tb_load_widget(tb);
				if (ret < 0) {
					fprintf(stderr, "error: loading widget\n");
					return ret;
				}
				ctx->comp_id++;
			}
			break;

		/* set up component connections from pipeline graph */
		case SND_SOC_TPLG_TYPE_DAPM_GRAPH:
			if (tb_register_graph(tb, hdr->count) < 0) {
				fprintf(stderr, "error: pipeline graph\n");
				ret = -EINVAL;
				goto out;
			}
			break;

		case SND_SOC_TPLG_TYPE_PCM:
			ret = tb_parse_pcm(tb, hdr->count);
			if (ret < 0)
				goto out;
			break;

		default:
			tplg_skip_hdr_payload(ctx);
			break;
		}
	}

	/* assign pipeline to every widget in the widget list */
	list_for_item(item, &tb->widget_list) {
		struct tplg_comp_info *comp_info = container_of(item, struct tplg_comp_info, item);
		struct list_item *pipe_item;

		list_for_item(pipe_item, &tb->pipeline_list) {
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

out:
	/* free all data */
	free(ctx->tplg_base);
	return ret;
}

static int tb_prepare_widget(struct testbench_prm *tb, struct tplg_pcm_info *pcm_info,
			     struct tplg_comp_info *comp_info, int dir)
{
	struct tplg_pipeline_list *pipeline_list;
	int ret, i;

	if (dir)
		pipeline_list = &pcm_info->capture_pipeline_list;
	else
		pipeline_list = &pcm_info->playback_pipeline_list;

	/* populate base config */
	ret = tb_set_up_widget_base_config(tb, comp_info);
	if (ret < 0)
		return ret;

	tb_pipeline_update_resource_usage(tb, comp_info);

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

static int tb_prepare_widgets(struct testbench_prm *tb, struct tplg_pcm_info *pcm_info,
			      struct tplg_comp_info *starting_comp_info,
			      struct tplg_comp_info *current_comp_info)
{
	struct list_item *item;
	int ret;

	/* for playback */
	list_for_item(item, &tb->route_list) {
		struct tplg_route_info *route_info = container_of(item, struct tplg_route_info,
								  item);

		if (route_info->source != current_comp_info)
			continue;

		/* set up source widget if it is the starting widget */
		if (starting_comp_info == current_comp_info) {
			ret = tb_prepare_widget(tb, pcm_info, current_comp_info, 0);
			if (ret < 0)
				return ret;
		}

		/* set up the sink widget */
		ret = tb_prepare_widget(tb, pcm_info, route_info->sink, 0);
		if (ret < 0)
			return ret;

		/* and then continue down the path */
		if (route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_IN ||
		    route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_OUT) {
			ret = tb_prepare_widgets(tb, pcm_info, starting_comp_info,
						 route_info->sink);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int tb_prepare_widgets_capture(struct testbench_prm *tb, struct tplg_pcm_info *pcm_info,
				      struct tplg_comp_info *starting_comp_info,
				      struct tplg_comp_info *current_comp_info)
{
	struct list_item *item;
	int ret;

	/* for capture */
	list_for_item(item, &tb->route_list) {
		struct tplg_route_info *route_info = container_of(item, struct tplg_route_info,
								  item);

		if (route_info->sink != current_comp_info)
			continue;

		/* set up sink widget if it is the starting widget */
		if (starting_comp_info == current_comp_info) {
			ret = tb_prepare_widget(tb, pcm_info, current_comp_info, 1);
			if (ret < 0)
				return ret;
		}

		/* set up the source widget */
		ret = tb_prepare_widget(tb, pcm_info, route_info->source, 1);
		if (ret < 0)
			return ret;

		/* and then continue up the path */
		if (route_info->source->type != SND_SOC_TPLG_DAPM_DAI_IN &&
		    route_info->source->type != SND_SOC_TPLG_DAPM_DAI_OUT) {
			ret = tb_prepare_widgets_capture(tb, pcm_info, starting_comp_info,
							 route_info->source);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}


static int tb_set_up_widget(struct testbench_prm *tb, struct tplg_comp_info *comp_info)
{
	struct tplg_pipeline_info *pipe_info = comp_info->pipe_info;
	int ret;

	pipe_info->usage_count++;

	/* first set up pipeline if needed, only done once for the first pipeline widget */
	if (pipe_info->usage_count == 1) {
		ret = tb_set_up_pipeline(tb, pipe_info);
		if (ret < 0) {
			pipe_info->usage_count--;
			return ret;
		}
	}

	/* now set up the widget */
	ret = tb_set_up_widget_ipc(tb, comp_info);
	if (ret < 0)
		return ret;

	tplg_debug("widget %s set up\n", comp_info->name);

	return 0;
}

static int tb_set_up_widgets(struct testbench_prm *tb, struct tplg_comp_info *starting_comp_info,
			     struct tplg_comp_info *current_comp_info)
{
	struct list_item *item;
	int ret;

	/* for playback */
	list_for_item(item, &tb->route_list) {
		struct tplg_route_info *route_info = container_of(item, struct tplg_route_info,
								  item);

		if (route_info->source != current_comp_info)
			continue;

		/* set up source widget if it is the starting widget */
		if (starting_comp_info == current_comp_info) {
			ret = tb_set_up_widget(tb, current_comp_info);
			if (ret < 0)
				return ret;
		}

		/* set up the sink widget */
		ret = tb_set_up_widget(tb, route_info->sink);
		if (ret < 0)
			return ret;

		/* source and sink widgets are up, so set up route now */
		ret = tb_set_up_route(tb, route_info);
		if (ret < 0)
			return ret;

		/* and then continue down the path */
		if (route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_IN ||
		    route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_OUT) {
			ret = tb_set_up_widgets(tb, starting_comp_info, route_info->sink);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int tb_set_up_widgets_capture(struct testbench_prm *tb,
				     struct tplg_comp_info *starting_comp_info,
				     struct tplg_comp_info *current_comp_info)
{
	struct list_item *item;
	int ret;

	/* for playback */
	list_for_item(item, &tb->route_list) {
		struct tplg_route_info *route_info = container_of(item, struct tplg_route_info,
								  item);

		if (route_info->sink != current_comp_info)
			continue;

		/* set up source widget if it is the starting widget */
		if (starting_comp_info == current_comp_info) {
			ret = tb_set_up_widget(tb, current_comp_info);
			if (ret < 0)
				return ret;
		}

		/* set up the sink widget */
		ret = tb_set_up_widget(tb, route_info->source);
		if (ret < 0)
			return ret;

		/* source and sink widgets are up, so set up route now */
		ret = tb_set_up_route(tb, route_info);
		if (ret < 0)
			return ret;

		/* and then continue down the path */
		if (route_info->source->type != SND_SOC_TPLG_DAPM_DAI_IN &&
		    route_info->source->type != SND_SOC_TPLG_DAPM_DAI_OUT) {
			ret = tb_set_up_widgets_capture(tb, starting_comp_info, route_info->source);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

int tb_set_up_pipelines(struct testbench_prm *tb, int dir)
{
	struct tplg_comp_info *host = NULL;
	struct tplg_pcm_info *pcm_info;
	struct list_item *item;
	int ret;

	// TODO tb->pcm_id is not defined?
	list_for_item(item, &tb->pcm_list) {
		pcm_info = container_of(item, struct tplg_pcm_info, item);

		if (pcm_info->id == tb->pcm_id) {
			if (dir)
				host = pcm_info->capture_host;
			else
				host = pcm_info->playback_host;
			break;
		}
	}

	if (!host) {
		fprintf(stderr, "No host component found for PCM ID: %d\n", tb->pcm_id);
		return -EINVAL;
	}

	if (!tb_is_pipeline_enabled(tb, host->pipeline_id))
		return 0;

	tb->pcm_info = pcm_info; //  TODO must be an array

	if (dir) {
		ret = tb_prepare_widgets_capture(tb, pcm_info, host, host);
		if (ret < 0)
			return ret;

		ret = tb_set_up_widgets_capture(tb, host, host);
		if (ret < 0)
			return ret;

		tplg_debug("Setting up capture pipelines complete\n");

		return 0;
	}

	ret = tb_prepare_widgets(tb, pcm_info, host, host);
	if (ret < 0)
		return ret;

	ret = tb_set_up_widgets(tb, host, host);
	if (ret < 0)
		return ret;

	tplg_debug("Setting up playback pipelines complete\n");

	return 0;
}

static int tb_free_widgets(struct testbench_prm *tb, struct tplg_comp_info *starting_comp_info,
			   struct tplg_comp_info *current_comp_info)
{
	struct tplg_route_info *route_info;
	struct list_item *item;
	int ret;

	/* for playback */
	list_for_item(item, &tb->route_list) {
		route_info = container_of(item, struct tplg_route_info, item);
		if (route_info->source != current_comp_info)
			continue;

		/* Widgets will be freed when the pipeline is deleted, so just unbind modules */
		ret = tb_free_route(tb, route_info);
		if (ret < 0)
			return ret;

		/* and then continue down the path */
		if (route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_IN ||
		    route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_OUT) {
			ret = tb_free_widgets(tb, starting_comp_info, route_info->sink);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int tb_free_widgets_capture(struct testbench_prm *tb,
				   struct tplg_comp_info *starting_comp_info,
				   struct tplg_comp_info *current_comp_info)
{
	struct tplg_route_info *route_info;
	struct list_item *item;
	int ret;

	/* for playback */
	list_for_item(item, &tb->route_list) {
		route_info = container_of(item, struct tplg_route_info, item);
		if (route_info->sink != current_comp_info)
			continue;

		/* Widgets will be freed when the pipeline is deleted, so just unbind modules */
		ret = tb_free_route(tb, route_info);
		if (ret < 0)
			return ret;

		/* and then continue down the path */
		if (route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_IN &&
		    route_info->sink->type != SND_SOC_TPLG_DAPM_DAI_OUT) {
			ret = tb_free_widgets_capture(tb, starting_comp_info, route_info->source);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

int tb_free_pipelines(struct testbench_prm *tb, int dir)
{
	struct tplg_pipeline_list *pipeline_list;
	struct tplg_pcm_info *pcm_info;
	struct list_item *item;
	struct tplg_comp_info *host = NULL;
	int ret, i;

	list_for_item(item, &tb->pcm_list) {
		pcm_info = container_of(item, struct tplg_pcm_info, item);
		if (dir)
			host = pcm_info->capture_host;
		else
			host = pcm_info->playback_host;

		if (!host || !tb_is_pipeline_enabled(tb, host->pipeline_id))
			continue;

		if (dir) {
			pipeline_list = &tb->pcm_info->capture_pipeline_list;
			ret = tb_free_widgets_capture(tb, host, host);
			if (ret < 0) {
				fprintf(stderr, "failed to free widgets for capture PCM\n");
				return ret;
			}
		} else {
			pipeline_list = &tb->pcm_info->playback_pipeline_list;
			ret = tb_free_widgets(tb, host, host);
			if (ret < 0) {
				fprintf(stderr, "failed to free widgets for playback PCM\n");
				return ret;
			}
		}
		for (i = 0; i < pipeline_list->count; i++) {
			struct tplg_pipeline_info *pipe_info = pipeline_list->pipelines[i];

			ret = tb_delete_pipeline(tb, pipe_info);
			if (ret < 0)
				return ret;
		}
	}

	tb->instance_ids[SND_SOC_TPLG_DAPM_SCHEDULER] = 0;
	return 0;
}

int tb_free_all_pipelines(struct testbench_prm *tb)
{
	debug_print("freeing playback direction\n");
	tb_free_pipelines(tb, SOF_IPC_STREAM_PLAYBACK);

	debug_print("freeing capture direction\n");
	tb_free_pipelines(tb, SOF_IPC_STREAM_CAPTURE);
	return 0;
}

void tb_free_topology(struct testbench_prm *tb)
{
	struct tplg_pcm_info *pcm_info;
	struct tplg_comp_info *comp_info;
	struct tplg_route_info *route_info;
	struct tplg_pipeline_info *pipe_info;
	struct list_item *item, *_item;

	list_for_item_safe(item, _item, &tb->pcm_list) {
		pcm_info = container_of(item, struct tplg_pcm_info, item);
		free(pcm_info->name);
		free(pcm_info);
	}

	list_for_item_safe(item, _item, &tb->widget_list) {
		comp_info = container_of(item, struct tplg_comp_info, item);
		free(comp_info->name);
		free(comp_info->stream_name);
		free(comp_info->ipc_payload);
		free(comp_info);
	}

	list_for_item_safe(item, _item, &tb->route_list) {
		route_info = container_of(item, struct tplg_route_info, item);
		free(route_info);
	}

	list_for_item_safe(item, _item, &tb->pipeline_list) {
		pipe_info = container_of(item, struct tplg_pipeline_info, item);
		free(pipe_info->name);
		free(pipe_info);
	}

	tplg_debug("freed all pipelines, widgets, routes and pcms\n");
}
