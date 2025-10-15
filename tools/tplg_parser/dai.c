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

struct sof_dai_types {
	const char *name;
	enum sof_ipc_dai_type type;
};

const struct sof_dai_types sof_dais[] = {
	{"SSP", SOF_DAI_INTEL_SSP},
	{"HDA", SOF_DAI_INTEL_HDA},
	{"DMIC", SOF_DAI_INTEL_DMIC},
};

/* find dai type */
static enum sof_ipc_dai_type find_dai(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sof_dais); i++) {
		if (strcmp(name, sof_dais[i].name) == 0)
			return sof_dais[i].type;
	}

	return SOF_DAI_INTEL_NONE;
}

static int get_token_dai_type(void *elem, void *object, uint32_t offset, uint32_t size)
{
	struct snd_soc_tplg_vendor_string_elem *velem = elem;
	uint32_t *val = (uint32_t *)((uint8_t *)object + offset);

	*val = find_dai(velem->string);
	return 0;
}

static const struct sof_topology_token dai_tokens[] = {
	{SOF_TKN_DAI_TYPE, SND_SOC_TPLG_TUPLE_TYPE_STRING, get_token_dai_type,
		offsetof(struct sof_ipc_comp_dai, type), 0},
	{SOF_TKN_DAI_INDEX, SND_SOC_TPLG_TUPLE_TYPE_WORD, tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_comp_dai, dai_index), 0},
	{SOF_TKN_DAI_DIRECTION, SND_SOC_TPLG_TUPLE_TYPE_WORD, tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_comp_dai, direction), 0},
};

/* DAI - IPC3 */
static const struct sof_topology_token_group dai_ipc3_tokens[] = {
	{comp_tokens, ARRAY_SIZE(comp_tokens),
		offsetof(struct sof_ipc_comp_dai, config)},
	{dai_tokens, ARRAY_SIZE(dai_tokens),
		0},
};

static int dai_ipc3_build(struct tplg_context *ctx, void *_dai)
{
	struct sof_ipc_comp_dai *dai = _dai;
	int comp_id = ctx->comp_id;

	/* configure dai */
	dai->comp.hdr.size = sizeof(*dai);
	dai->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	dai->comp.id = comp_id;
	dai->comp.type = SOF_COMP_DAI;
	dai->comp.pipeline_id = ctx->pipeline_id;
	dai->config.hdr.size = sizeof(dai->config);

	return 0;
}

/* DAI - IPC4 */
static const struct sof_topology_token dai4_tokens[] = {
	/* TODO */
};

static const struct sof_topology_token_group dai_ipc4_tokens[] = {
	{dai4_tokens, ARRAY_SIZE(dai4_tokens)},
};

static int dai_ipc4_build(struct tplg_context *ctx, void *dai)
{
	/* TODO */
	return 0;
}

static const struct sof_topology_module_desc dai_ipc[] = {
	{3, dai_ipc3_tokens, ARRAY_SIZE(dai_ipc3_tokens),
		dai_ipc3_build, sizeof(struct sof_ipc_comp_dai)},
	{4, dai_ipc4_tokens, ARRAY_SIZE(dai_ipc4_tokens), dai_ipc4_build},
};

int tplg_new_dai(struct tplg_context *ctx, void *dai, size_t dai_size,
		 struct snd_soc_tplg_ctl_hdr *rctl, size_t max_ctl_size)
{
	int ret;

	ret = tplg_create_object(ctx, dai_ipc, ARRAY_SIZE(dai_ipc),
				 "dai", dai, dai_size);
	return ret;
}

