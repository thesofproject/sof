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

/* Buffers */
static const struct sof_topology_token buffer_tokens[] = {
	{SOF_TKN_BUF_SIZE, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_buffer, size), 0},
	{SOF_TKN_BUF_CAPS, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_buffer, caps), 0},
};

static const struct sof_topology_token buffer_comp_tokens[] = {
	{SOF_TKN_COMP_CORE_ID, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_comp, core), 0},
};


/* load buffer DAPM widget */
int tplg_create_buffer(struct tplg_context *ctx,
		     struct sof_ipc_buffer *buffer)
{
	struct snd_soc_tplg_vendor_array *array = &ctx->widget->priv.array[0];
	size_t parsed_size = 0;
	int size = ctx->widget->priv.size;
	int comp_id = ctx->comp_id;
	int ret;

	/* configure buffer */
	buffer->comp.core = 0;
	buffer->comp.id = comp_id;
	buffer->comp.pipeline_id = ctx->pipeline_id;
	buffer->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_BUFFER_NEW;
	buffer->comp.type = SOF_COMP_BUFFER;
	buffer->comp.hdr.size = sizeof(struct sof_ipc_buffer);

	/* read vendor tokens */
	while (parsed_size < size) {

		/* check for array size mismatch */
		if (!is_valid_priv_size(parsed_size, size, array)) {
			fprintf(stderr, "error: load buffer array size mismatch\n");
			return -EINVAL;
		}

		/* parse buffer comp tokens */
		ret = sof_parse_tokens(&buffer->comp, buffer_comp_tokens,
				       ARRAY_SIZE(buffer_comp_tokens), array,
				       array->size);
		if (ret) {
			fprintf(stderr, "error: parse buffer comp tokens %d\n",
				size);
			return -EINVAL;
		}

		/* parse buffer tokens */
		ret = sof_parse_tokens(buffer, buffer_tokens,
				       ARRAY_SIZE(buffer_tokens), array,
				       array->size);
		if (ret) {
			fprintf(stderr, "error: parse buffer tokens %d\n",
				size);
			return -EINVAL;
		}

		parsed_size += array->size;
		array = MOVE_POINTER_BY_BYTES(array, array->size);
	}

	return 0;
}

/* load pipeline dapm widget */
int tplg_new_buffer(struct tplg_context *ctx, struct sof_ipc_buffer *buffer,
		struct snd_soc_tplg_ctl_hdr *rctl)
{
	int ret;

	ret = tplg_create_buffer(ctx, buffer);
	if (ret < 0)
		return ret;

	if (tplg_create_controls(ctx, ctx->widget->num_kcontrols, rctl, 0) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	return ret;
}
