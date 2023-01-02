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

/* scheduling */
static const struct sof_topology_token sched_tokens[] = {
	{SOF_TKN_SCHED_PERIOD, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_pipe_new, period), 0},
	{SOF_TKN_SCHED_PRIORITY, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_pipe_new, priority), 0},
	{SOF_TKN_SCHED_MIPS, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_pipe_new, period_mips), 0},
	{SOF_TKN_SCHED_CORE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_pipe_new, core), 0},
	{SOF_TKN_SCHED_FRAMES, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_pipe_new, frames_per_sched), 0},
	{SOF_TKN_SCHED_TIME_DOMAIN, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_pipe_new, time_domain), 0},
};

/* Pipeline - IPC3 */
static const struct sof_topology_token_group pipeline_ipc3_tokens[] = {
	{sched_tokens, ARRAY_SIZE(sched_tokens),
		0},
};

static int pipeline_ipc3_build(struct tplg_context *ctx, void *_pipeline)
{
	struct sof_ipc_pipe_new *pipeline = _pipeline;
	int comp_id = ctx->comp_id;

	/* configure pipeline */
	pipeline->comp_id = comp_id;
	pipeline->pipeline_id = ctx->pipeline_id;
	pipeline->hdr.size = sizeof(*pipeline);
	pipeline->hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_PIPE_NEW;

	return 0;
}

/* Pipeline - IPC4 */
static const struct sof_topology_token pipeline4_tokens[] = {
	/* TODO */
};

static const struct sof_topology_token_group pipeline_ipc4_tokens[] = {
	{pipeline4_tokens, ARRAY_SIZE(pipeline4_tokens)},
};

static int pipeline_ipc4_build(struct tplg_context *ctx, void *pipeline)
{
	/* TODO */
	return 0;
}

static const struct sof_topology_module_desc pipeline_ipc[] = {
	{3, pipeline_ipc3_tokens, ARRAY_SIZE(pipeline_ipc3_tokens),
		pipeline_ipc3_build, sizeof(struct sof_ipc_pipe_new)},
	{4, pipeline_ipc4_tokens, ARRAY_SIZE(pipeline_ipc4_tokens), pipeline_ipc4_build},
};

/* load pipeline dapm widget */
int tplg_new_pipeline(struct tplg_context *ctx, void *pipeline,
		      size_t pipeline_size, struct snd_soc_tplg_ctl_hdr *rctl)
{
	int ret;

	ret = tplg_create_object(ctx, pipeline_ipc, ARRAY_SIZE(pipeline_ipc),
				 "pipeline", pipeline, pipeline_size);
	return ret;
}
