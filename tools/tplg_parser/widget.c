// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

/* Topology parser */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <ipc/topology.h>
#include <ipc/stream.h>
#include <ipc/dai.h>
#include <sof/common.h>
#include <tplg_parser/topology.h>
#include <sof/lib/uuid.h>
#include <sof/ipc/topology.h>

/* load dapm widget */
int load_widget(struct tplg_context *ctx)
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
		if (tplg_register_pga(ctx) < 0) {
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
		if (tplg_register_buffer(ctx) < 0) {
			fprintf(stderr, "error: load buffer\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_SCHEDULER:
		/* find comp id for scheduling comp */
		if (dev_type == FUZZER_DEV)
			ctx->sched_id = find_widget(temp_comp_list, comp_id, ctx->widget->sname);

		if (tplg_register_pipeline(ctx) < 0) {
			fprintf(stderr, "error: load pipeline\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_SRC:
		if (tplg_register_src(ctx) < 0) {
			fprintf(stderr, "error: load src\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_ASRC:
		if (tplg_register_asrc(ctx) < 0) {
			fprintf(stderr, "error: load src\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_MIXER:
		if (tplg_register_mixer(ctx) < 0) {
			fprintf(stderr, "error: load mixer\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case SND_SOC_TPLG_DAPM_EFFECT:
		if (load_process(ctx) < 0) {
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
		ret = tplg_create_controls(ctx->widget->num_kcontrols, ctx->file);
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
