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
#include <sof/common.h>
#include <sof/lib/uuid.h>
#include <sof/ipc/topology.h>
#include <tplg_parser/topology.h>
#include <tplg_parser/tokens.h>

/* ASRC */
static const struct sof_topology_token asrc_tokens[] = {
	{SOF_TKN_ASRC_RATE_IN, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_comp_asrc, source_rate), 0},
	{SOF_TKN_ASRC_RATE_OUT, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_comp_asrc, sink_rate), 0},
	{SOF_TKN_ASRC_ASYNCHRONOUS_MODE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_comp_asrc, asynchronous_mode), 0},
	{SOF_TKN_ASRC_OPERATION_MODE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_comp_asrc, operation_mode), 0},
};

/* load asrc dapm widget */
int tplg_create_asrc(struct tplg_context *ctx, struct sof_ipc_comp_asrc *asrc,
		     size_t max_comp_size)
{
	struct snd_soc_tplg_vendor_array *array = &ctx->widget->priv.array[0];
	size_t total_array_size = 0;
	int ret, comp_id = ctx->comp_id;
	int size = ctx->widget_size;
	char uuid[UUID_SIZE];

	if (max_comp_size < sizeof(struct sof_ipc_comp_asrc) + UUID_SIZE)
		return -EINVAL;

	/* read vendor tokens */
	while (total_array_size < size) {

		/* check for array size mismatch */
		if (!is_valid_priv_size(total_array_size, size, array)) {
			fprintf(stderr, "error: load asrc array size mismatch\n");
			return -EINVAL;
		}

		/* parse comp tokens */
		ret = sof_parse_tokens(&asrc->config, comp_tokens,
				       ARRAY_SIZE(comp_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse asrc comp_tokens %d\n",
				size);
			return -EINVAL;
		}

		/* parse asrc tokens */
		ret = sof_parse_tokens(asrc, asrc_tokens,
				       ARRAY_SIZE(asrc_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse asrc tokens %d\n", size);
			return -EINVAL;
		}

		/* parse uuid token */
		ret = sof_parse_tokens(uuid, comp_ext_tokens,
				       ARRAY_SIZE(comp_ext_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse asrc uuid token %d\n", size);
			return -EINVAL;
		}

		total_array_size += array->size;

		/* read next array */
		array = MOVE_POINTER_BY_BYTES(array, array->size);
	}

	/* configure asrc */
	asrc->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	asrc->comp.id = comp_id;
	asrc->comp.hdr.size = sizeof(struct sof_ipc_comp_asrc) + UUID_SIZE;
	asrc->comp.type = SOF_COMP_ASRC;
	asrc->comp.pipeline_id = ctx->pipeline_id;
	asrc->comp.ext_data_length = UUID_SIZE;
	asrc->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	memcpy(asrc + 1, &uuid, UUID_SIZE);

	return 0;
}

/* load asrc dapm widget */
int tplg_new_asrc(struct tplg_context *ctx, struct sof_ipc_comp *comp, size_t comp_size,
		struct snd_soc_tplg_ctl_hdr *rctl, size_t max_ctl_size)
{
	struct sof_ipc_comp_asrc *asrc = (struct sof_ipc_comp_asrc *)comp;
	int ret;

	ret = tplg_create_asrc(ctx, asrc, comp_size);
	if (ret < 0)
		return ret;

	if (tplg_create_controls(ctx, ctx->widget->num_kcontrols, rctl, max_ctl_size) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	return ret;
}
