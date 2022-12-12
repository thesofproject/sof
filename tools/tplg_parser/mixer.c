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
#include <string.h>
#include <ipc/topology.h>
#include <sof/lib/uuid.h>
#include <sof/ipc/topology.h>
#include <tplg_parser/topology.h>
#include <tplg_parser/tokens.h>

/* load mixer dapm widget */
int tplg_create_mixer(struct tplg_context *ctx,
		    struct sof_ipc_comp_mixer *mixer, size_t max_comp_size)
{
	struct snd_soc_tplg_vendor_array *array = &ctx->widget->priv.array[0];
	size_t total_array_size = 0;
	int size = ctx->widget->priv.size;
	int comp_id = ctx->comp_id;
	char uuid[UUID_SIZE];
	int ret;

	if (max_comp_size < sizeof(struct sof_ipc_comp_mixer) + UUID_SIZE)
		return -EINVAL;

	/* read vendor tokens */
	while (total_array_size < size) {

		/* check for array size mismatch */
		if (!is_valid_priv_size(total_array_size, size, array)) {
			fprintf(stderr, "error: load mixer array size mismatch\n");
			return -EINVAL;
		}

		/* parse comp tokens */
		ret = sof_parse_tokens(&mixer->config, comp_tokens,
				       ARRAY_SIZE(comp_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse src comp_tokens %d\n",
				size);
			return -EINVAL;
		}
		/* parse uuid token */
		ret = sof_parse_tokens(uuid, comp_ext_tokens,
				       ARRAY_SIZE(comp_ext_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse mixer uuid token %d\n", size);
			return -EINVAL;
		}

		total_array_size += array->size;

		/* read next array */
		array = MOVE_POINTER_BY_BYTES(array, array->size);
	}

	/* configure mixer */
	mixer->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	mixer->comp.id = comp_id;
	mixer->comp.hdr.size = sizeof(struct sof_ipc_comp_mixer) + UUID_SIZE;
	mixer->comp.type = SOF_COMP_MIXER;
	mixer->comp.pipeline_id = ctx->pipeline_id;
	mixer->comp.ext_data_length = UUID_SIZE;
	mixer->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	memcpy(mixer + 1, &uuid, UUID_SIZE);

	return 0;
}

int tplg_new_mixer(struct tplg_context *ctx, struct sof_ipc_comp *comp, size_t max_comp_size,
		struct snd_soc_tplg_ctl_hdr *rctl, size_t max_ctl_size)
{
	struct sof_ipc_comp_mixer *mixer = (struct sof_ipc_comp_mixer *)comp;
	int ret;

	ret = tplg_create_mixer(ctx, mixer, max_comp_size);
	if (ret < 0)
		return ret;

	if (tplg_create_controls(ctx, ctx->widget->num_kcontrols, rctl, max_ctl_size) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	return ret;
}
