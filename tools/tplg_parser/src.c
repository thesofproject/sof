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

/* SRC */
static const struct sof_topology_token src_tokens[] = {
	{SOF_TKN_SRC_RATE_IN, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_comp_src, source_rate), 0},
	{SOF_TKN_SRC_RATE_OUT, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_comp_src, sink_rate), 0},
};

/* load src dapm widget */
int tplg_create_src(struct tplg_context *ctx,
		  struct sof_ipc_comp_src *src, size_t max_comp_size)
{
	struct snd_soc_tplg_vendor_array *array = NULL;
	size_t total_array_size = 0, read_size;
	FILE *file = ctx->file;
	int size = ctx->widget->priv.size;
	int comp_id = ctx->comp_id;
	unsigned char uuid[UUID_SIZE];
	int ret;

	if (max_comp_size < sizeof(struct sof_ipc_comp_src) + UUID_SIZE)
		return -EINVAL;

	/* allocate memory for vendor tuple array */
	array = (struct snd_soc_tplg_vendor_array *)malloc(size);
	if (!array) {
		fprintf(stderr, "error: mem alloc for src vendor array\n");
		return -errno;
	}

	/* read vendor tokens */
	while (total_array_size < size) {
		read_size = sizeof(struct snd_soc_tplg_vendor_array);
		ret = fread(array, read_size, 1, file);
		if (ret != 1) {
			free(MOVE_POINTER_BY_BYTES(array, -total_array_size));
			return -EINVAL;
		}

		/* check for array size mismatch */
		if (!is_valid_priv_size(total_array_size, size, array)) {
			fprintf(stderr, "error: load src array size mismatch\n");
			free(MOVE_POINTER_BY_BYTES(array, -total_array_size));
			return -EINVAL;
		}

		ret = tplg_read_array(array, file);
		if (ret) {
			fprintf(stderr, "error: read array fail\n");
			free(MOVE_POINTER_BY_BYTES(array, -total_array_size));
			return ret;
		}

		/* parse comp tokens */
		ret = sof_parse_tokens(&src->config, comp_tokens,
				       ARRAY_SIZE(comp_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse src comp_tokens %d\n",
				size);
			free(MOVE_POINTER_BY_BYTES(array, -total_array_size));
			return -EINVAL;
		}

		/* parse src tokens */
		ret = sof_parse_tokens(src, src_tokens,
				       ARRAY_SIZE(src_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse src tokens %d\n", size);
			free(MOVE_POINTER_BY_BYTES(array, -total_array_size));
			return -EINVAL;
		}

		/* parse uuid token */
		ret = sof_parse_tokens(uuid, comp_ext_tokens,
				       ARRAY_SIZE(comp_ext_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse src uuid token %d\n", size);
			free(array);
			return -EINVAL;
		}

		total_array_size += array->size;

		/* read next array */
		array = MOVE_POINTER_BY_BYTES(array, array->size);
	}

	/* point to the start of array so it gets freed properly */
	array = MOVE_POINTER_BY_BYTES(array, -total_array_size);

	/* configure src */
	src->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	src->comp.id = comp_id;
	src->comp.hdr.size = sizeof(struct sof_ipc_comp_src) + UUID_SIZE;
	src->comp.type = SOF_COMP_SRC;
	src->comp.pipeline_id = ctx->pipeline_id;
	src->comp.ext_data_length = UUID_SIZE;
	src->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	memcpy(src + 1, &uuid, UUID_SIZE);

	free(array);
	return 0;
}

/* load src dapm widget */
int tplg_new_src(struct tplg_context *ctx, struct sof_ipc_comp *comp, size_t comp_size,
		struct snd_soc_tplg_ctl_hdr *rctl, size_t max_ctl_size)
{
	struct sof_ipc_comp_src *src = (struct sof_ipc_comp_src *)comp;
	int ret;

	ret = tplg_create_src(ctx, src, comp_size);
	if (ret < 0)
		return ret;

	if (tplg_create_controls(ctx->widget->num_kcontrols, ctx->file, rctl, max_ctl_size) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	return ret;
}
