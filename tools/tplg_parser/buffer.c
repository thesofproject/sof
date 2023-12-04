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
#include <ipc4/gateway.h>
#include <sof/common.h>
#include <sof/lib/uuid.h>
#include <sof/ipc/topology.h>
#include <tplg_parser/topology.h>
#include <tplg_parser/tokens.h>

#include "copier/copier.h"

/* Buffers */
static const struct sof_topology_token buffer_tokens[] = {
	{SOF_TKN_BUF_SIZE, SND_SOC_TPLG_TUPLE_TYPE_WORD, tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_buffer, size), 0},
	{SOF_TKN_BUF_CAPS, SND_SOC_TPLG_TUPLE_TYPE_WORD, tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_buffer, caps), 0},
	{SOF_TKN_BUF_FLAGS, SND_SOC_TPLG_TUPLE_TYPE_WORD, tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_buffer, flags), 0},
};

static const struct sof_topology_token buffer_comp_tokens[] = {
	{SOF_TKN_COMP_CORE_ID, SND_SOC_TPLG_TUPLE_TYPE_WORD, tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_comp, core), 0},
};


/* Buffer - IPC3 */
static const struct sof_topology_token_group buffer_ipc3_tokens[] = {
	{buffer_comp_tokens, ARRAY_SIZE(buffer_comp_tokens),
		offsetof(struct sof_ipc_buffer, comp)},
	{buffer_tokens, ARRAY_SIZE(buffer_tokens),
		0},
};

static int buffer_ipc3_build(struct tplg_context *ctx, void *_buffer)
{
	struct sof_ipc_buffer *buffer = _buffer;
	int comp_id = ctx->comp_id;

	/* configure buffer */
	buffer->comp.core = 0;
	buffer->comp.id = comp_id;
	buffer->comp.pipeline_id = ctx->pipeline_id;
	buffer->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_BUFFER_NEW;
	buffer->comp.type = SOF_COMP_BUFFER;
	buffer->comp.hdr.size = sizeof(struct sof_ipc_buffer);

	return 0;
}

static const struct sof_topology_token buffer4_tokens[] = {
};

static const struct sof_topology_token_group buffer_ipc4_tokens[] = {
	{buffer4_tokens, ARRAY_SIZE(buffer4_tokens)},
};

static int buffer_ipc4_build(struct tplg_context *ctx, void *_copier)
{
	struct ipc4_copier_module_cfg *copier = _copier;

	copier->gtw_cfg.node_id.dw = IPC4_INVALID_NODE_ID;

	return tplg_parse_widget_audio_formats(ctx);
}

static const struct sof_topology_module_desc buffer_ipc[] = {
	{3, buffer_ipc3_tokens, ARRAY_SIZE(buffer_ipc3_tokens),
		buffer_ipc3_build, sizeof(struct sof_ipc_buffer)},
	{4, buffer_ipc4_tokens, ARRAY_SIZE(buffer_ipc4_tokens), buffer_ipc4_build},
};

int tplg_new_buffer(struct tplg_context *ctx, void *buffer, size_t buffer_size,
		    struct snd_soc_tplg_ctl_hdr *rctl, size_t buffer_ctl_size)
{
	int ret;

	ret = tplg_create_object(ctx, buffer_ipc, ARRAY_SIZE(buffer_ipc),
				 "buffer", buffer, buffer_size);

	return ret;
}
