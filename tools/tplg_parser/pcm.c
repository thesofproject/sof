// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

/* FE DAI or PCM parser */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <ipc/topology.h>
#include <sof/list.h>
#include <sof/ipc/topology.h>
#include <kernel/header.h>
#include <tplg_parser/topology.h>

/* parse and save the PCM information */
int tplg_parse_pcm(struct tplg_context *ctx, struct list_item *widget_list,
		   struct list_item *pcm_list)
{
	struct tplg_pcm_info *pcm_info;
	struct snd_soc_tplg_pcm *pcm;
	struct list_item *item;

	pcm_info = calloc(sizeof(struct tplg_pcm_info), 1);
	if (!pcm_info)
		return -ENOMEM;

	pcm = tplg_get_pcm(ctx);

	pcm_info->name = strdup(pcm->pcm_name);
	if (!pcm_info->name) {
		free(pcm_info);
		return -ENOMEM;
	}

	pcm_info->id = pcm->pcm_id;

	/* look up from the widget list and populate the PCM info */
	list_for_item(item, widget_list) {
		struct tplg_comp_info *comp_info = container_of(item, struct tplg_comp_info, item);

		if (!strcmp(pcm->caps[0].name, comp_info->stream_name) &&
		    (comp_info->type == SND_SOC_TPLG_DAPM_AIF_IN ||
		     comp_info->type == SND_SOC_TPLG_DAPM_AIF_OUT)) {
			pcm_info->playback_host = comp_info;
			tplg_debug("PCM: '%s' ID: %d Host name: %s\n", pcm_info->name,
				   pcm_info->id, comp_info->name);
		}
		if (!strcmp(pcm->caps[1].name, comp_info->stream_name) &&
		    (comp_info->type == SND_SOC_TPLG_DAPM_AIF_IN ||
		     comp_info->type == SND_SOC_TPLG_DAPM_AIF_OUT)) {
			pcm_info->capture_host = comp_info;
			tplg_debug("PCM: '%s' ID: %d Host name: %s\n", pcm_info->name,
				   pcm_info->id, comp_info->name);
		}
	}

	list_item_append(&pcm_info->item, pcm_list);

	return 0;
}
