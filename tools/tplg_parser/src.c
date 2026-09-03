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
#include <src/src_ipc.h>

/* SRC - IPC3 */
static const struct sof_topology_token src3_tokens[] = {
	{SOF_TKN_SRC_RATE_IN, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_comp_src, source_rate), 0},
	{SOF_TKN_SRC_RATE_OUT, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_comp_src, sink_rate), 0},
};

static const struct sof_topology_token_group src_ipc3_tokens[] = {
	{src3_tokens, ARRAY_SIZE(src3_tokens)},
	{comp_tokens, ARRAY_SIZE(comp_tokens),
		offsetof(struct sof_ipc_comp_src, config)},
	{comp_ext_tokens, ARRAY_SIZE(comp_ext_tokens),
		sizeof(struct sof_ipc_comp_src)},
};

static int src_ipc3_build(struct tplg_context *ctx, void *_src)
{
	struct sof_ipc_comp_src *src = _src;
	int comp_id = ctx->comp_id;

	/* configure src */
	src->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	src->comp.id = comp_id;
	src->comp.hdr.size = sizeof(struct sof_ipc_comp_src) + UUID_SIZE;
	src->comp.type = SOF_COMP_SRC;
	src->comp.pipeline_id = ctx->pipeline_id;
	src->comp.ext_data_length = UUID_SIZE;
	src->config.hdr.size = sizeof(struct sof_ipc_comp_config);

	return 0;
}

/* ASRC - IPC4 */
static const struct sof_topology_token src4_tokens[] = {
	{SOF_TKN_SRC_RATE_OUT, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		tplg_token_get_uint32_t,
		offsetof(struct ipc4_config_src, sink_rate), 0},
};

static const struct sof_topology_token_group src_ipc4_tokens[] = {
	{src4_tokens, ARRAY_SIZE(src4_tokens)},
};

static int src_ipc4_build(struct tplg_context *ctx, void *src)
{
	/* TODO */
	return 0;
}

static const struct sof_topology_module_desc src_ipc[] = {
	{3, src_ipc3_tokens, ARRAY_SIZE(src_ipc3_tokens),
		src_ipc3_build, sizeof(struct sof_ipc_comp_src) + UUID_SIZE},
	{4, src_ipc4_tokens, ARRAY_SIZE(src_ipc4_tokens), src_ipc4_build},
};

/* load src dapm widget */
int tplg_new_src(struct tplg_context *ctx, void *src, size_t src_size,
		 struct snd_soc_tplg_ctl_hdr *rctl, size_t max_ctl_size)
{
	int ret;

	ret = tplg_create_object(ctx, src_ipc, ARRAY_SIZE(src_ipc),
				 "src", src, src_size);
	if (ret < 0)
		return ret;

	if (tplg_create_controls(ctx, ctx->widget->num_kcontrols,
				 rctl, max_ctl_size, src) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	return ret;
}
