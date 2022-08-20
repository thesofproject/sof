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
	 get_token_uint32_t,
	 offsetof(struct sof_ipc_comp_host, dmac_config), 0},
};

int tplg_create_pcm(struct tplg_context *ctx, int dir,
		  struct sof_ipc_comp_host *host)
{
	struct snd_soc_tplg_vendor_array *array = NULL;
	size_t total_array_size = 0, read_size;
	FILE *file = ctx->file;
	int size = ctx->widget->priv.size;
	int comp_id = ctx->comp_id;
	int ret;

	/* configure host comp IPC message */
	host->comp.hdr.size = sizeof(*host);
	host->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	host->comp.id = comp_id;
	host->comp.type = SOF_COMP_HOST;
	host->comp.pipeline_id = ctx->pipeline_id;
	host->direction = dir;
	host->config.hdr.size = sizeof(host->config);

	/* allocate memory for vendor tuple array */
	array = (struct snd_soc_tplg_vendor_array *)malloc(size);
	if (!array) {
		fprintf(stderr, "error: mem alloc\n");
		return -errno;
	}

	/* read vendor tokens */
	while (total_array_size < size) {
		read_size = sizeof(struct snd_soc_tplg_vendor_array);
		ret = fread(array, read_size, 1, file);
		if (ret != 1)
			return -EINVAL;

		/* check for array size mismatch */
		if (!is_valid_priv_size(total_array_size, size, array)) {
			fprintf(stderr, "error: load pcm array size mismatch\n");
			free(array);
			return -EINVAL;
		}

		ret = tplg_read_array(array, file);
		if (ret) {
			fprintf(stderr, "error: read array fail\n");
			free(array);
			return ret;
		}

		/* parse comp tokens */
		ret = sof_parse_tokens(&host->config, comp_tokens,
				       ARRAY_SIZE(comp_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse comp tokens %d\n",
				size);
			free(array);
			return -EINVAL;
		}

		/* parse pcm tokens */
		ret = sof_parse_tokens(host, pcm_tokens,
				       ARRAY_SIZE(comp_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse pcm tokens %d\n", size);
			free(array);
			return -EINVAL;
		}

		total_array_size += array->size;
	}

	free(array);
	return 0;
}

