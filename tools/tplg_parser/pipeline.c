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

/* Buffers */
static const struct sof_topology_token buffer_tokens[] = {
	{SOF_TKN_BUF_SIZE, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_buffer, size), 0},
	{SOF_TKN_BUF_CAPS, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_buffer, caps), 0},
};

static const struct sof_topology_token buffer_comp_tokens[] = {
	{SOF_TKN_COMP_CORE_ID, SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_comp, core), 0},
};

/* scheduling */
static const struct sof_topology_token sched_tokens[] = {
	{SOF_TKN_SCHED_PERIOD, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_pipe_new, period), 0},
	{SOF_TKN_SCHED_PRIORITY, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_pipe_new, priority), 0},
	{SOF_TKN_SCHED_MIPS, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_pipe_new, period_mips), 0},
	{SOF_TKN_SCHED_CORE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_pipe_new, core), 0},
	{SOF_TKN_SCHED_FRAMES, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_pipe_new, frames_per_sched), 0},
	{SOF_TKN_SCHED_TIME_DOMAIN, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_pipe_new, time_domain), 0},
};

/* load buffer DAPM widget */
int tplg_create_buffer(struct tplg_context *ctx,
		     struct sof_ipc_buffer *buffer)
{
	struct snd_soc_tplg_vendor_array *array;
	size_t read_size;
	size_t parsed_size = 0;
	FILE *file = ctx->file;
	int size = ctx->widget->priv.size;
	int comp_id = ctx->comp_id;
	int ret;

	/* configure buffer */
	buffer->comp.core = 0;
	buffer->comp.id = comp_id;
	buffer->comp.pipeline_id = ctx->pipeline_id;
	buffer->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_BUFFER_NEW;
	buffer->comp.type = SOF_COMP_BUFFER;
	buffer->comp.hdr.size = sizeof(struct sof_ipc_buffer);

	/* allocate memory for vendor tuple array */
	array = (struct snd_soc_tplg_vendor_array *)malloc(size);
	if (!array) {
		fprintf(stderr, "error: malloc fail during load_buffer\n");
		return -errno;
	}

	/* read vendor tokens */
	while (parsed_size < size) {
		read_size = sizeof(struct snd_soc_tplg_vendor_array);
		ret = fread(array, read_size, 1, file);
		if (ret != 1) {
			fprintf(stderr,
				"error: fread fail during load_buffer\n");
			free(array);
			return -EINVAL;
		}

		/* check for array size mismatch */
		if (!is_valid_priv_size(parsed_size, size, array)) {
			fprintf(stderr, "error: load buffer array size mismatch\n");
			free(array);
			return -EINVAL;
		}

		ret = tplg_read_array(array, file);
		if (ret) {
			fprintf(stderr, "error: read array fail\n");
			free(array);
			return ret;
		}

		/* parse buffer comp tokens */
		ret = sof_parse_tokens(&buffer->comp, buffer_comp_tokens,
				       ARRAY_SIZE(buffer_comp_tokens), array,
				       array->size);
		if (ret) {
			fprintf(stderr, "error: parse buffer comp tokens %d\n",
				size);
			free(array);
			return -EINVAL;
		}

		/* parse buffer tokens */
		ret = sof_parse_tokens(buffer, buffer_tokens,
				       ARRAY_SIZE(buffer_tokens), array,
				       array->size);
		if (ret) {
			fprintf(stderr, "error: parse buffer tokens %d\n",
				size);
			free(array);
			return -EINVAL;
		}

		parsed_size += array->size;
	}

	free(array);
	return 0;
}

/* load buffer DAPM widget */
int tplg_register_buffer(struct tplg_context *ctx)
{
	struct sof *sof = ctx->sof;
	struct sof_ipc_buffer buffer = {0};
	int ret;

	ret = tplg_create_buffer(ctx, &buffer);
	if (ret < 0)
		return ret;

	if (tplg_create_controls(ctx->widget->num_kcontrols, ctx->file) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	/* create buffer component */
	if (ipc_buffer_new(sof->ipc, &buffer) < 0) {
		fprintf(stderr, "error: buffer new\n");
		return -EINVAL;
	}

	return 0;
}

/* load pipeline graph DAPM widget*/
int tplg_create_graph(int num_comps, int pipeline_id,
		    struct comp_info *temp_comp_list, char *pipeline_string,
		    struct sof_ipc_pipe_comp_connect *connection, FILE *file,
		    int route_num, int count)
{
	struct snd_soc_tplg_dapm_graph_elem *graph_elem;
	char *source = NULL, *sink = NULL;
	int j, ret = 0;
	size_t size;

	/* configure route */
	connection->hdr.size = sizeof(*connection);
	connection->hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_CONNECT;

	/* allocate memory for graph elem */
	size = sizeof(struct snd_soc_tplg_dapm_graph_elem);
	graph_elem = (struct snd_soc_tplg_dapm_graph_elem *)malloc(size);
	if (!graph_elem) {
		fprintf(stderr, "error: mem alloc\n");
		return -errno;
	}

	/* set up component connections */
	connection->source_id = -1;
	connection->sink_id = -1;

	size = sizeof(struct snd_soc_tplg_dapm_graph_elem);
	ret = fread(graph_elem, size, 1, file);
	if (ret != 1) {
		free(graph_elem);
		return -EINVAL;
	}

	/* look up component id from the component list */
	for (j = 0; j < num_comps; j++) {
		if (strcmp(temp_comp_list[j].name,
			   graph_elem->source) == 0) {
			connection->source_id = temp_comp_list[j].id;
			source = graph_elem->source;
		}

		if (strcmp(temp_comp_list[j].name,
			   graph_elem->sink) == 0) {
			connection->sink_id = temp_comp_list[j].id;
			sink = graph_elem->sink;
		}
	}

	if (!source || !sink) {
		fprintf(stderr, "%s() error: source=%p, sink=%p\n",
			__func__, source, sink);
		free(graph_elem);
		return -EINVAL;
	}

	printf("loading route %s -> %s\n", source, sink);

	strcat(pipeline_string, graph_elem->source);
	strcat(pipeline_string, "->");

	if (route_num == (count - 1)) {
		strcat(pipeline_string, graph_elem->sink);
		strcat(pipeline_string, "\n");
	}

	free(graph_elem);
	return 0;
}

/* load pipeline graph DAPM widget*/
int tplg_register_graph(void *dev, struct comp_info *temp_comp_list,
			char *pipeline_string, FILE *file,
			int count, int num_comps, int pipeline_id)
{
	struct sof_ipc_pipe_comp_connect connection;
	struct sof *sof = (struct sof *)dev;
	int ret = 0;
	int i;

	for (i = 0; i < count; i++) {
		ret = tplg_create_graph(num_comps, pipeline_id, temp_comp_list,
				      pipeline_string, &connection, file, i,
				      count);
		if (ret < 0)
			return ret;

		/* connect source and sink */
		if (ipc_comp_connect(sof->ipc, ipc_to_pipe_connect(&connection)) < 0) {
			fprintf(stderr, "error: comp connect\n");
			return -EINVAL;
		}
	}

	/* pipeline complete after pipeline connections are established */
	for (i = 0; i < num_comps; i++) {
		if (temp_comp_list[i].pipeline_id == pipeline_id &&
		    temp_comp_list[i].type == SND_SOC_TPLG_DAPM_SCHEDULER)
			ipc_pipeline_complete(sof->ipc, temp_comp_list[i].id);
	}

	return ret;
}

/* load scheduler dapm widget */
int tplg_create_pipeline(struct tplg_context *ctx,
		       struct sof_ipc_pipe_new *pipeline)
{
	struct snd_soc_tplg_vendor_array *array = NULL;
	size_t total_array_size = 0, read_size;
	FILE *file = ctx->file;
	int size = ctx->widget->priv.size;
	int comp_id = ctx->comp_id;
	int ret;

	/* configure pipeline */
	pipeline->comp_id = comp_id;
	pipeline->pipeline_id = ctx->pipeline_id;
	pipeline->hdr.size = sizeof(*pipeline);
	pipeline->hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_PIPE_NEW;

	/* allocate memory for vendor tuple array */
	array = (struct snd_soc_tplg_vendor_array *)malloc(size);
	if (!array) {
		fprintf(stderr, "error: mem alloc\n");
		return -errno;
	}

	/* read vendor arrays */
	while (total_array_size < size) {
		read_size = sizeof(struct snd_soc_tplg_vendor_array);
		ret = fread(array, read_size, 1, file);
		if (ret != 1) {
			free(MOVE_POINTER_BY_BYTES(array, -total_array_size));
			return -EINVAL;
		}

		/* check for array size mismatch */
		if (!is_valid_priv_size(total_array_size, size, array)) {
			fprintf(stderr, "error: load pipeline array size mismatch\n");
			free(MOVE_POINTER_BY_BYTES(array, -total_array_size));
			return -EINVAL;
		}

		ret = tplg_read_array(array, file);
		if (ret) {
			fprintf(stderr, "error: read array fail\n");
			free(MOVE_POINTER_BY_BYTES(array, -total_array_size));
			return -EINVAL;
		}

		/* parse scheduler tokens */
		ret = sof_parse_tokens(pipeline, sched_tokens,
				       ARRAY_SIZE(sched_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse pipeline tokens %d\n",
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

	free(array);
	return 0;
}

/* load scheduler dapm widget */
int tplg_register_pipeline(struct tplg_context *ctx)
{
	struct sof *sof = ctx->sof;
	struct sof_ipc_pipe_new pipeline = {0};
	FILE *file = ctx->file;
	int ret;

	ret = tplg_create_pipeline(ctx, &pipeline);
	if (ret < 0)
		return ret;

	if (tplg_create_controls(ctx->widget->num_kcontrols, file) < 0) {
		fprintf(stderr, "error: loading controls\n");
		return -EINVAL;
	}

	pipeline.sched_id = ctx->sched_id;

	/* Create pipeline */
	if (ipc_pipeline_new(sof->ipc, (ipc_pipe_new *)&pipeline) < 0) {
		fprintf(stderr, "error: pipeline new\n");
		return -EINVAL;
	}

	return 0;
}
