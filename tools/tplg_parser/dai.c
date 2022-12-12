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
#include <ipc/topology.h>
#include <ipc/stream.h>
#include <ipc/dai.h>
#include <sof/common.h>
#include <sof/lib/uuid.h>
#include <sof/ipc/topology.h>
#include <tplg_parser/topology.h>
#include <tplg_parser/tokens.h>

static const struct sof_topology_token dai_tokens[] = {
	{SOF_TKN_DAI_TYPE, SND_SOC_TPLG_TUPLE_TYPE_STRING, get_token_dai_type,
		offsetof(struct sof_ipc_comp_dai, type), 0},
	{SOF_TKN_DAI_INDEX, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_comp_dai, dai_index), 0},
	{SOF_TKN_DAI_DIRECTION, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_comp_dai, direction), 0},
};

/* load dai component */
int tplg_create_dai(struct tplg_context *ctx, struct sof_ipc_comp_dai *comp_dai)
{
	struct snd_soc_tplg_vendor_array *array = &ctx->widget->priv.array[0];
	size_t total_array_size = 0;
	int size = ctx->widget->priv.size;
	int comp_id = ctx->comp_id;
	int ret;

	/* configure comp_dai */
	comp_dai->comp.hdr.size = sizeof(*comp_dai);
	comp_dai->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	comp_dai->comp.id = comp_id;
	comp_dai->comp.type = SOF_COMP_DAI;
	comp_dai->comp.pipeline_id = ctx->pipeline_id;
	comp_dai->config.hdr.size = sizeof(comp_dai->config);

	/* read vendor tokens */
	while (total_array_size < size) {

		/* check for array size mismatch */
		if (!is_valid_priv_size(total_array_size, size, array)) {
			fprintf(stderr, "error: load dai array size mismatch\n");
			return -EINVAL;
		}

		ret = sof_parse_tokens(comp_dai, dai_tokens,
				       ARRAY_SIZE(dai_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse dai tokens failed %d\n",
				size);
			return -EINVAL;
		}

		/* parse comp tokens */
		ret = sof_parse_tokens(&comp_dai->config, comp_tokens,
				       ARRAY_SIZE(comp_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse filewrite tokens %d\n",
				size);
			return -EINVAL;
		}
		total_array_size += array->size;
		array = MOVE_POINTER_BY_BYTES(array, array->size);
	}

	return 0;
}

