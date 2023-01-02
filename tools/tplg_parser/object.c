// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
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

/* create object from descriptors */
int tplg_create_object(struct tplg_context *ctx,
		       const struct sof_topology_module_desc *desc, int num_desc,
		       const char *name, void *object, size_t max_object_size)
{
	struct snd_soc_tplg_vendor_array *array = &ctx->widget->priv.array[0];
	size_t total_array_size = 0;
	const struct sof_topology_module_desc *ipc = NULL;
	int ret, i;
	int size = ctx->widget->priv.size;

	for (i = 0; i < num_desc; i++) {
		if (desc[i].abi_major != ctx->ipc_major)
			continue;
		/* match */
		ipc = &desc[i];
		break;
	}
	if (!ipc) {
		printf("error: %s no support for IPC major %d\n", __func__,
		       ctx->ipc_major);
		return -EINVAL;
	}

	if (max_object_size < ipc->min_size) {
		printf("error: %s not enough space, have %zu need %d\n",
		       __func__, max_object_size, ipc->min_size);
		return -EINVAL;
	}

	memset(object, 0, max_object_size);

	/* read vendor tokens */
	while (total_array_size < size) {
		/* check for array size mismatch */
		if (!tplg_is_valid_priv_size(total_array_size, size, array)) {
			fprintf(stderr, "error: load %s array size mismatch\n", name);
			return -EINVAL;
		}

		for (i = 0; i < ipc->num_groups; i++) {
			const struct sof_topology_token *tokens = ipc->grp[i].tokens;
			int num_tokens = ipc->grp[i].num_tokens;
			int offset = ipc->grp[i].grp_offset;

			ret = sof_parse_tokens(object + offset, tokens,
					       num_tokens, array,
					       array->size);
			if (ret != 0) {
				fprintf(stderr, "error: parse %s comp_tokens %d\n",
					name, size);
				return -EINVAL;
			}
		}

		total_array_size += array->size;

		/* read next array */
		array = MOVE_POINTER_BY_BYTES(array, array->size);
	}

	ret = ipc->builder(ctx, object);
	if (ret < 0)
		fprintf(stderr, "error: builder for %s failed\n", name);

	return ret;
}
