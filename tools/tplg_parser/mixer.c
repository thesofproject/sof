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


/* MIXER - IPC3 */
static const struct sof_topology_token_group mixer_ipc3_tokens[] = {
	{comp_tokens, ARRAY_SIZE(comp_tokens),
		offsetof(struct sof_ipc_comp_mixer, config)},
	{comp_ext_tokens, ARRAY_SIZE(comp_ext_tokens),
		sizeof(struct sof_ipc_comp_mixer)},
};

static int mixer_ipc3_build(struct tplg_context *ctx, void *_mixer)
{
	struct sof_ipc_comp_mixer *mixer = _mixer;
	int comp_id = ctx->comp_id;

	/* configure mixer */
	mixer->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	mixer->comp.id = comp_id;
	mixer->comp.hdr.size = sizeof(struct sof_ipc_comp_mixer) + UUID_SIZE;
	mixer->comp.type = SOF_COMP_MIXER;
	mixer->comp.pipeline_id = ctx->pipeline_id;
	mixer->comp.ext_data_length = UUID_SIZE;
	mixer->config.hdr.size = sizeof(struct sof_ipc_comp_config);

	return 0;
}

/* MIXER - IPC4 */
static const struct sof_topology_token mixer4_tokens[] = {
	/* TODO */
};

static const struct sof_topology_token_group mixer_ipc4_tokens[] = {
	{mixer4_tokens, ARRAY_SIZE(mixer4_tokens)},
};

static int mixer_ipc4_build(struct tplg_context *ctx, void *mixer)
{
	/* TODO */
	return 0;
}

static const struct sof_topology_module_desc mixer_ipc[] = {
	{3, mixer_ipc3_tokens, ARRAY_SIZE(mixer_ipc3_tokens),
		mixer_ipc3_build, sizeof(struct sof_ipc_comp_mixer) + UUID_SIZE},
	{4, mixer_ipc4_tokens, ARRAY_SIZE(mixer_ipc4_tokens), mixer_ipc4_build},
};

int tplg_new_mixer(struct tplg_context *ctx, void *mixer, size_t mixer_size,
		   struct snd_soc_tplg_ctl_hdr *rctl, size_t max_ctl_size)
{
	int ret;

	ret = tplg_create_object(ctx, mixer_ipc, ARRAY_SIZE(mixer_ipc),
				 "mixer", mixer, mixer_size);
	if (ret < 0)
		return ret;

	if (tplg_create_controls(ctx, ctx->widget->num_kcontrols,
				 rctl, max_ctl_size, mixer) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	return ret;
}
