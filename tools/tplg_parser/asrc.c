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

/* ASRC - IPC3 */
static const struct sof_topology_token asrc3_tokens[] = {
	{SOF_TKN_ASRC_RATE_IN, SND_SOC_TPLG_TUPLE_TYPE_WORD, tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_comp_asrc, source_rate), 0},
	{SOF_TKN_ASRC_RATE_OUT, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_comp_asrc, sink_rate), 0},
	{SOF_TKN_ASRC_ASYNCHRONOUS_MODE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_comp_asrc, asynchronous_mode), 0},
	{SOF_TKN_ASRC_OPERATION_MODE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_comp_asrc, operation_mode), 0},
};

static const struct sof_topology_token_group asrc_ipc3_tokens[] = {
	{asrc3_tokens, ARRAY_SIZE(asrc3_tokens)},
	{comp_tokens, ARRAY_SIZE(comp_tokens),
		offsetof(struct sof_ipc_comp_asrc, config)},
	{comp_ext_tokens, ARRAY_SIZE(comp_ext_tokens),
		sizeof(struct sof_ipc_comp_asrc)},
};

static int asrc_ipc3_build(struct tplg_context *ctx, void *_asrc)
{
	struct sof_ipc_comp_asrc *asrc = _asrc;
	int comp_id = ctx->comp_id;

	/* configure asrc */
	asrc->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	asrc->comp.id = comp_id;
	asrc->comp.hdr.size = sizeof(struct sof_ipc_comp_asrc) + UUID_SIZE;
	asrc->comp.type = SOF_COMP_ASRC;
	asrc->comp.pipeline_id = ctx->pipeline_id;
	asrc->comp.ext_data_length = UUID_SIZE;
	asrc->config.hdr.size = sizeof(struct sof_ipc_comp_config);

	return 0;
}

/* ASRC - IPC4 */
static const struct sof_topology_token asrc4_tokens[] = {
	{SOF_TKN_ASRC_RATE_OUT, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		tplg_token_get_uint32_t, offsetof(struct ipc4_asrc_module_cfg, out_freq), 0},
	{SOF_TKN_ASRC_OPERATION_MODE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		tplg_token_get_uint32_t, offsetof(struct ipc4_asrc_module_cfg, asrc_mode), 0},
};

static const struct sof_topology_token_group asrc_ipc4_tokens[] = {
	{asrc4_tokens, ARRAY_SIZE(asrc4_tokens)},
};

static int asrc_ipc4_build(struct tplg_context *ctx, void *asrc)
{
	/* TODO */
	return 0;
}

static const struct sof_topology_module_desc asrc_ipc[] = {
	{3, asrc_ipc3_tokens, ARRAY_SIZE(asrc_ipc3_tokens),
		asrc_ipc3_build, sizeof(struct sof_ipc_comp_asrc) + UUID_SIZE},
	{4, asrc_ipc4_tokens, ARRAY_SIZE(asrc_ipc4_tokens), asrc_ipc4_build},
};

/* load asrc dapm widget */
int tplg_new_asrc(struct tplg_context *ctx, void *asrc, size_t asrc_size,
		  struct snd_soc_tplg_ctl_hdr *rctl, size_t max_ctl_size)
{
	int ret;

	ret = tplg_create_object(ctx, asrc_ipc, ARRAY_SIZE(asrc_ipc),
				 "asrc", asrc, asrc_size);
	if (ret < 0)
		return ret;

	if (tplg_create_controls(ctx, ctx->widget->num_kcontrols,
				 rctl, max_ctl_size, asrc) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	return ret;
}
