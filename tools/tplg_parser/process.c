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
#include <kernel/header.h>
#include <tplg_parser/topology.h>
#include <tplg_parser/tokens.h>

static const struct sof_topology_token process_tokens[] = {
	{SOF_TKN_PROCESS_TYPE, SND_SOC_TPLG_TUPLE_TYPE_STRING,
		get_token_process_type,
		offsetof(struct sof_ipc_comp_process, type), 0},
};

/* load asrc dapm widget */
int tplg_create_process(struct tplg_context *ctx,
		      struct sof_ipc_comp_process *process,
		      struct sof_ipc_comp_ext *comp_ext)
{
	struct snd_soc_tplg_vendor_array *array = NULL;
	size_t total_array_size = 0;
	size_t read_size;
	FILE *file = ctx->file;
	int size = ctx->widget->priv.size;
	int comp_id = ctx->comp_id;
	int ret;

	/* allocate memory for vendor tuple array */
	array = (struct snd_soc_tplg_vendor_array *)malloc(size);
	if (!array) {
		fprintf(stderr, "error: mem alloc for asrc vendor array\n");
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
			fprintf(stderr, "error: load process array size mismatch\n");
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
		ret = sof_parse_tokens(&process->config, comp_tokens,
				       ARRAY_SIZE(comp_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse process comp_tokens %d\n",
				size);
			free(MOVE_POINTER_BY_BYTES(array, -total_array_size));
			return -EINVAL;
		}

		/* parse process tokens */
		ret = sof_parse_tokens(process, process_tokens,
				       ARRAY_SIZE(process_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse process tokens %d\n",
				size);
			free(MOVE_POINTER_BY_BYTES(array, -total_array_size));
			return -EINVAL;
		}

		/* parse uuid tokes */
		ret = sof_parse_tokens(comp_ext, comp_ext_tokens,
				       ARRAY_SIZE(comp_ext_tokens), array,
				       array->size);

		if (ret != 0) {
			fprintf(stderr, "error: parse comp extended tokens %d\n",
				size);
			free(MOVE_POINTER_BY_BYTES(array, -total_array_size));
			return -EINVAL;
		}

		total_array_size += array->size;

		/* read next array */
		array = MOVE_POINTER_BY_BYTES(array, array->size);
	}

	/* point to the start of array so it gets freed properly */
	array = MOVE_POINTER_BY_BYTES(array, -total_array_size);

	/* configure asrc */
	process->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	process->comp.id = comp_id;
	process->comp.hdr.size = sizeof(struct sof_ipc_comp_process) + UUID_SIZE;
	process->comp.type = tplg_get_process_type(process->type);
	process->comp.pipeline_id = ctx->pipeline_id;
	process->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	process->comp.ext_data_length = UUID_SIZE;
	memcpy(process + 1, comp_ext, UUID_SIZE);

	free(array);
	return 0;
}

int tplg_process_init_data(struct sof_ipc_comp_process **process_ipc,
			     struct sof_ipc_comp_process *process)
{
	*process_ipc = malloc(sizeof(struct sof_ipc_comp_process) + UUID_SIZE);
	if (!(*process_ipc)) {
		fprintf(stderr, "error: Failed to allocate IPC\n");
		return -errno;
	}

	/* Copy header */
	memcpy(*process_ipc, process, sizeof(struct sof_ipc_comp_process) + UUID_SIZE);
	(*process_ipc)->size = 0;
	return 0;
}

int tplg_process_append_data(struct sof_ipc_comp_process **process_ipc,
			       struct sof_ipc_comp_process *process,
			       struct snd_soc_tplg_ctl_hdr *ctl,
			       char *priv_data)
{
	int ipc_size;
	int size = 0;
	struct snd_soc_tplg_bytes_control *bytes_ctl;

	if (*process_ipc) {
		fprintf(stderr, "error: Only single private data append to IPC is supported\n");
		return -EINVAL;
	}

	/* Size is process IPC plus private data minus ABI header */
	ipc_size = sizeof(struct sof_ipc_comp_process) + UUID_SIZE;
	if (ctl->ops.info == SND_SOC_TPLG_CTL_BYTES) {
		bytes_ctl = (struct snd_soc_tplg_bytes_control *)ctl;
		size = bytes_ctl->priv.size - sizeof(struct sof_abi_hdr);
		ipc_size += size;
	}
	*process_ipc = malloc(ipc_size);
	if (!(*process_ipc)) {
		fprintf(stderr, "error: Failed to allocate IPC\n");
		return -errno;
	}

	/* Copy header */
	memcpy(*process_ipc, process, sizeof(struct sof_ipc_comp_process) + UUID_SIZE);

	/* Copy configuration data, need to strip ABI header*/
	memcpy((char *)*process_ipc + sizeof(struct sof_ipc_comp_process) + UUID_SIZE,
	       priv_data + sizeof(struct sof_abi_hdr), size);
	(*process_ipc)->size = size;
	return 0;
}

/* load process dapm widget */
int tplg_new_process(struct tplg_context *ctx, struct sof_ipc_comp *comp, size_t comp_size,
		struct snd_soc_tplg_ctl_hdr *rctl, size_t max_ctl_size)
{
	struct sof_ipc_comp_process *process = (struct sof_ipc_comp_process *)comp;
	struct snd_soc_tplg_dapm_widget *widget = ctx->widget;
	struct snd_soc_tplg_ctl_hdr *ctl = NULL;
	struct sof_ipc_comp_process *process_ipc = NULL;
	struct sof_ipc_comp_ext comp_ext;
	char *priv_data = NULL;
	int ret;
	int i;

	ret = tplg_create_process(ctx, process, &comp_ext);
	if (ret < 0)
		return ret;

	/* Get control into ctl and priv_data */
	for (i = 0; i < widget->num_kcontrols; i++) {
		ret = tplg_create_single_control(&ctl, &priv_data, ctx->file);

		if (ret < 0) {
			fprintf(stderr, "error: failed control load\n");
			return ret;
		}

		/* Merge process and priv_data into process_ipc */
		if (priv_data)
			ret = tplg_process_append_data(&process_ipc, process, ctl, priv_data);

		free(ctl);
		free(priv_data);
		if (ret) {
			fprintf(stderr, "error: private data append failed\n");
			free(process_ipc);
			return ret;
		}
	}

	/* Default IPC without appended data */
	if (!process_ipc) {
		ret = tplg_process_init_data(&process_ipc, process);
		if (ret)
			return ret;
	}

	return ret;
}
