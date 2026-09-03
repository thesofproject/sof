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

/* PCM */
static const struct sof_topology_token pcm_tokens[] = {
	{SOF_TKN_PCM_DMAC_CONFIG, SND_SOC_TPLG_TUPLE_TYPE_WORD,
	 tplg_token_get_uint32_t,
	 offsetof(struct sof_ipc_comp_host, dmac_config), 0},
};

/* PCM - IPC3 */
static const struct sof_topology_token_group pcm_ipc3_tokens[] = {
	{pcm_tokens, ARRAY_SIZE(pcm_tokens),
		offsetof(struct sof_ipc_comp_host, comp)},
	{comp_tokens, ARRAY_SIZE(comp_tokens),
		offsetof(struct sof_ipc_comp_host, config)},
};

static int pcm_ipc3_build(struct tplg_context *ctx, void *_pcm)
{
	struct sof_ipc_comp_host *host = _pcm;
	int comp_id = ctx->comp_id;

	/* configure host comp IPC message */
	host->comp.hdr.size = sizeof(*host);
	host->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	host->comp.id = comp_id;
	host->comp.type = SOF_COMP_HOST;
	host->comp.pipeline_id = ctx->pipeline_id;
	host->direction = ctx->dir;
	host->config.hdr.size = sizeof(host->config);

	return 0;
}

/* PCM - IPC4 */
static const struct sof_topology_token pcm4_tokens[] = {
	/* TODO */
};

static const struct sof_topology_token_group pcm_ipc4_tokens[] = {
	{pcm4_tokens, ARRAY_SIZE(pcm4_tokens)},
};

static int pcm_ipc4_build(struct tplg_context *ctx, void *pcm)
{
	/* TODO */
	return 0;
}

static const struct sof_topology_module_desc pcm_ipc[] = {
	{3, pcm_ipc3_tokens, ARRAY_SIZE(pcm_ipc3_tokens),
		pcm_ipc3_build, sizeof(struct sof_ipc_comp_host)},
	{4, pcm_ipc4_tokens, ARRAY_SIZE(pcm_ipc4_tokens), pcm_ipc4_build},
};

int tplg_new_pcm(struct tplg_context *ctx, void *host, size_t host_size)
{
	return tplg_create_object(ctx, pcm_ipc, ARRAY_SIZE(pcm_ipc),
				"pcm", host, host_size);
}

