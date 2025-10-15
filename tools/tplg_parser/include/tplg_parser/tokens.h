// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

/* Topology parser */

#ifndef _COMMON_TPLG_TOKENS_H
#define _COMMON_TPLG_TOKENS_H

#include <ipc/topology.h>
#include <sof/ipc/topology.h>
#include <tplg_parser/topology.h>

/* temporary - current MAXLEN is not define in UAPI header - fix pending */
#ifndef SNDRV_CTL_ELEM_ID_NAME_MAXLEN
#define SNDRV_CTL_ELEM_ID_NAME_MAXLEN	44
#endif

#include <linux/types.h>
#include <alsa/sound/asoc.h>

struct sof_topology_token {
	uint32_t token;
	uint32_t type;
	int (*get_token)(void *elem, void *object, uint32_t offset,
			 uint32_t size);
	uint32_t offset;
	uint32_t size;
};

static const struct sof_topology_token tone_tokens[] = {
};

/* Generic components */
static const struct sof_topology_token comp_tokens[] = {
	{SOF_TKN_COMP_PERIOD_SINK_COUNT,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_comp_config, periods_sink), 0},
	{SOF_TKN_COMP_PERIOD_SOURCE_COUNT,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_comp_config, periods_source), 0},
	{SOF_TKN_COMP_FORMAT,
		SND_SOC_TPLG_TUPLE_TYPE_STRING, tplg_token_get_comp_format,
		offsetof(struct sof_ipc_comp_config, frame_fmt), 0},
};

/* Component extended tokens */
static const struct sof_topology_token comp_ext_tokens[] = {
	{SOF_TKN_COMP_UUID, SND_SOC_TPLG_TUPLE_TYPE_UUID, tplg_token_get_uuid, 0, 0},

};

struct sof_topology_token_group {
	const struct sof_topology_token *tokens;
	const int num_tokens;
	const int grp_offset;
};

struct sof_topology_module_desc {
	const int abi_major;
	const struct sof_topology_token_group *grp;
	const int num_groups;
	int (*builder)(struct tplg_context *ctx, void *data);
	const int min_size;
};

#endif
