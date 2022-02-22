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
#include <math.h>
#include <ipc/topology.h>
#include <ipc/stream.h>
#include <ipc/dai.h>
#include <sof/common.h>
#include <tplg_parser/topology.h>
#include <sof/lib/uuid.h>
#include <sof/ipc/topology.h>
#include <kernel/header.h>

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
		  struct sof_ipc_comp_src *src)
{
	struct snd_soc_tplg_vendor_array *array = NULL;
	size_t total_array_size = 0, read_size;
	FILE *file = ctx->file;
	int size = ctx->widget->priv.size;
	int comp_id = ctx->comp_id;
	int ret;

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

		total_array_size += array->size;

		/* read next array */
		array = MOVE_POINTER_BY_BYTES(array, array->size);
	}

	/* point to the start of array so it gets freed properly */
	array = MOVE_POINTER_BY_BYTES(array, -total_array_size);

	/* configure src */
	src->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	src->comp.id = comp_id;
	src->comp.hdr.size = sizeof(struct sof_ipc_comp_src);
	src->comp.type = SOF_COMP_SRC;
	src->comp.pipeline_id = ctx->pipeline_id;
	src->config.hdr.size = sizeof(struct sof_ipc_comp_config);

	free(array);
	return 0;
}


/* load src dapm widget */
int tplg_register_src(struct tplg_context *ctx)
{
	struct sof *sof = ctx->sof;
	struct sof_ipc_comp_src src = {0};
	FILE *file = ctx->file;
	int ret = 0;

	ret = tplg_create_src(ctx, &src);
	if (ret < 0)
		return ret;

	if (tplg_create_controls(ctx->widget->num_kcontrols, file) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	/* set testbench input and output sample rate from topology */
	if (!ctx->fs_out) {
		ctx->fs_out = src.sink_rate;

		if (!ctx->fs_in)
			ctx->fs_in = src.source_rate;
		else
			src.source_rate = ctx->fs_in;
	} else {
		src.sink_rate = ctx->fs_out;
	}

	/* load src component */
	register_comp(src.comp.type, NULL);
	if (ipc_comp_new(sof->ipc, ipc_to_comp_new(&src)) < 0) {
		fprintf(stderr, "error: new src comp\n");
		return -EINVAL;
	}

	return ret;
}

/* ASRC */
static const struct sof_topology_token asrc_tokens[] = {
	{SOF_TKN_ASRC_RATE_IN, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_comp_asrc, source_rate), 0},
	{SOF_TKN_ASRC_RATE_OUT, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_comp_asrc, sink_rate), 0},
	{SOF_TKN_ASRC_ASYNCHRONOUS_MODE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_comp_asrc, asynchronous_mode), 0},
	{SOF_TKN_ASRC_OPERATION_MODE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_comp_asrc, operation_mode), 0},
};

/* load asrc dapm widget */
int tplg_create_asrc(struct tplg_context *ctx, struct sof_ipc_comp_asrc *asrc)
{
	struct snd_soc_tplg_vendor_array *array = NULL;
	size_t total_array_size = 0, read_size, size = ctx->widget_size;
	FILE *file = ctx->file;
	int ret, comp_id = ctx->comp_id;

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
			fprintf(stderr, "error: load asrc array size mismatch\n");
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
		ret = sof_parse_tokens(&asrc->config, comp_tokens,
				       ARRAY_SIZE(comp_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse asrc comp_tokens %zu\n",
				size);
			free(MOVE_POINTER_BY_BYTES(array, -total_array_size));
			return -EINVAL;
		}

		/* parse asrc tokens */
		ret = sof_parse_tokens(asrc, asrc_tokens,
				       ARRAY_SIZE(asrc_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse asrc tokens %zu\n", size);
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
	asrc->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	asrc->comp.id = comp_id;
	asrc->comp.hdr.size = sizeof(struct sof_ipc_comp_asrc);
	asrc->comp.type = SOF_COMP_ASRC;
	asrc->comp.pipeline_id = ctx->pipeline_id;
	asrc->config.hdr.size = sizeof(struct sof_ipc_comp_config);

	free(array);
	return 0;
}

/* load asrc dapm widget */
int tplg_register_asrc(struct tplg_context *ctx)
{
	struct snd_soc_tplg_dapm_widget *widget = ctx->widget;
	struct sof *sof = ctx->sof;
	struct sof_ipc_comp_asrc asrc = {0};
	int ret = 0;

	ret = tplg_create_asrc(ctx, &asrc);
	if (ret < 0)
		return ret;

	if (tplg_create_controls(widget->num_kcontrols, ctx->file) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	/* set testbench input and output sample rate from topology */
	if (!ctx->fs_out) {
		ctx->fs_out = asrc.sink_rate;

		if (!ctx->fs_in)
			ctx->fs_in = asrc.source_rate;
		else
			asrc.source_rate = ctx->fs_in;
	} else {
		asrc.sink_rate = ctx->fs_out;
	}

	/* load asrc component */
	register_comp(asrc.comp.type, NULL);
	if (ipc_comp_new(sof->ipc, ipc_to_comp_new(&asrc)) < 0) {
		fprintf(stderr, "error: new asrc comp\n");
		return -EINVAL;
	}

	return ret;
}

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
	process->comp.hdr.size = sizeof(struct sof_ipc_comp_asrc);
	process->comp.type = tplg_get_process_type(process->type);
	process->comp.pipeline_id = ctx->pipeline_id;
	process->config.hdr.size = sizeof(struct sof_ipc_comp_config);

	free(array);
	return 0;
}

static int process_init_data(struct sof_ipc_comp_process **process_ipc,
			     struct sof_ipc_comp_process *process)
{
	*process_ipc = malloc(sizeof(struct sof_ipc_comp_process));
	if (!(*process_ipc)) {
		fprintf(stderr, "error: Failed to allocate IPC\n");
		return -errno;
	}

	/* Copy header */
	memcpy(*process_ipc, process, sizeof(struct sof_ipc_comp_process));
	(*process_ipc)->size = 0;
	return 0;
}

static int process_append_data(struct sof_ipc_comp_process **process_ipc,
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
	ipc_size = sizeof(struct sof_ipc_comp_process);
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
	memcpy(*process_ipc, process, sizeof(struct sof_ipc_comp_process));

	/* Copy configuration data, need to strip ABI header*/
	memcpy((char *)*process_ipc + sizeof(struct sof_ipc_comp_process),
	       priv_data + sizeof(struct sof_abi_hdr), size);
	(*process_ipc)->size = size;
	return 0;
}

/* load process dapm widget */
int load_process(struct tplg_context *ctx)
{
	struct sof *sof = ctx->sof;
	struct snd_soc_tplg_dapm_widget *widget = ctx->widget;
	struct sof_ipc_comp_process process = {0};
	struct sof_ipc_comp_process *process_ipc = NULL;
	struct snd_soc_tplg_ctl_hdr *ctl = NULL;
	struct sof_ipc_comp_ext comp_ext;
	char *priv_data = NULL;
	int ret = 0;
	int i;

	ret = tplg_create_process(ctx, &process, &comp_ext);
	if (ret < 0)
		return ret;

	printf("debug uuid:\n");
	for (i = 0; i < SOF_UUID_SIZE; i++)
		printf("%u ", comp_ext.uuid[i]);
	printf("\n");

	/* Get control into ctl and priv_data */
	for (i = 0; i < widget->num_kcontrols; i++) {
		ret = tplg_create_single_control(&ctl, &priv_data, ctx->file);

		if (ret < 0) {
			fprintf(stderr, "error: failed control load\n");
			return ret;
		}

		/* Merge process and priv_data into process_ipc */
		if (priv_data)
			ret = process_append_data(&process_ipc, &process, ctl, priv_data);

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
		ret = process_init_data(&process_ipc, &process);
		if (ret)
			return ret;
	}

	/* load process component */
	register_comp(process_ipc->comp.type, &comp_ext);

	/* Instantiate */
	ret = ipc_comp_new(sof->ipc, ipc_to_comp_new(process_ipc));
	free(process_ipc);

	if (ret < 0)
		fprintf(stderr, "error: new process comp\n");

	return ret;
}
