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
#include <tplg_parser/topology.h>
#include <sof/lib/uuid.h>

#define MOVE_POINTER_BY_BYTES(p, b) ((typeof(p))((uint8_t *)(p) + (b)))

struct sof_process_types {
	const char *name;
	enum sof_ipc_process_type type;
	enum sof_comp_type comp_type;
};

static const struct sof_process_types sof_process[] = {
	{"EQFIR", SOF_PROCESS_EQFIR, SOF_COMP_EQ_FIR},
	{"EQIIR", SOF_PROCESS_EQIIR, SOF_COMP_EQ_IIR},
	{"KEYWORD_DETECT", SOF_PROCESS_KEYWORD_DETECT, SOF_COMP_KEYWORD_DETECT},
	{"KPB", SOF_PROCESS_KPB, SOF_COMP_KPB},
	{"CHAN_SELECTOR", SOF_PROCESS_CHAN_SELECTOR, SOF_COMP_SELECTOR},
	{"MUX", SOF_PROCESS_MUX, SOF_COMP_MUX},
	{"DEMUX", SOF_PROCESS_DEMUX, SOF_COMP_DEMUX},
	{"DCBLOCK", SOF_PROCESS_DCBLOCK, SOF_COMP_DCBLOCK},
};

static enum sof_ipc_process_type find_process(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sof_process); i++) {
		if (strcmp(name, sof_process[i].name) == 0)
			return sof_process[i].type;
	}

	return SOF_PROCESS_NONE;
}

static enum sof_comp_type find_process_comp_type(enum sof_ipc_process_type type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sof_process); i++) {
		if (sof_process[i].type == type)
			return sof_process[i].comp_type;
	}

	return SOF_COMP_NONE;
}

bool is_valid_priv_size(size_t size_read, size_t priv_size,
			      struct snd_soc_tplg_vendor_array *array)
{
	size_t arr_size, elem_size, arr_elems_size;

	arr_size = sizeof(struct snd_soc_tplg_vendor_array);

	switch (array->type) {
	case SND_SOC_TPLG_TUPLE_TYPE_UUID:
		elem_size = sizeof(struct snd_soc_tplg_vendor_uuid_elem);
		break;
	case SND_SOC_TPLG_TUPLE_TYPE_STRING:
		elem_size = sizeof(struct snd_soc_tplg_vendor_string_elem);
		break;
	case SND_SOC_TPLG_TUPLE_TYPE_BOOL:
	case SND_SOC_TPLG_TUPLE_TYPE_BYTE:
	case SND_SOC_TPLG_TUPLE_TYPE_WORD:
	case SND_SOC_TPLG_TUPLE_TYPE_SHORT:
		elem_size = sizeof(struct snd_soc_tplg_vendor_value_elem);
		break;
	default:
		/* This is handled in the further calls */
		return true;
	}

	arr_elems_size = array->num_elems * elem_size;

	/*
	 * check if size of data to be read from widget's private data
	 * doesn't exceed private data's size.
	 */
	return size_read + arr_size + arr_elems_size <= priv_size;
}

/* read vendor tuples array from topology */
int tplg_read_array(struct snd_soc_tplg_vendor_array *array, FILE *file)
{
	struct snd_soc_tplg_vendor_uuid_elem uuid;
	struct snd_soc_tplg_vendor_string_elem string;
	struct snd_soc_tplg_vendor_value_elem value;
	int j, ret = 0;
	size_t size;

	switch (array->type) {
	case SND_SOC_TPLG_TUPLE_TYPE_UUID:

		/* copy uuid elems into array */
		for (j = 0; j < array->num_elems; j++) {
			size = sizeof(struct snd_soc_tplg_vendor_uuid_elem);
			ret = fread(&uuid, size, 1, file);
			if (ret != 1)
				return -EINVAL;
			memcpy(&array->uuid[j], &uuid, size);
		}
		break;
	case SND_SOC_TPLG_TUPLE_TYPE_STRING:

		/* copy string elems into array */
		for (j = 0; j < array->num_elems; j++) {
			size = sizeof(struct snd_soc_tplg_vendor_string_elem);
			ret = fread(&string, size, 1, file);
			if (ret != 1)
				return -EINVAL;
			memcpy(&array->string[j], &string, size);
		}
		break;
	case SND_SOC_TPLG_TUPLE_TYPE_BOOL:
	case SND_SOC_TPLG_TUPLE_TYPE_BYTE:
	case SND_SOC_TPLG_TUPLE_TYPE_WORD:
	case SND_SOC_TPLG_TUPLE_TYPE_SHORT:
		/* copy value elems into array */
		for (j = 0; j < array->num_elems; j++) {
			size = sizeof(struct snd_soc_tplg_vendor_value_elem);
			ret = fread(&value, size, 1, file);
			if (ret != 1)
				return -EINVAL;
			memcpy(&array->value[j], &value, size);
		}
		break;
	default:
		fprintf(stderr, "error: unknown token type %d\n", array->type);
		return -EINVAL;
	}
	return 0;
}

/* load buffer DAPM widget */
int tplg_load_buffer(int comp_id, int pipeline_id, int size,
		     struct sof_ipc_buffer *buffer, FILE *file)
{
	struct snd_soc_tplg_vendor_array *array;
	size_t read_size;
	size_t parsed_size = 0;
	int ret;

	/* configure buffer */
	buffer->comp.core = 0;
	buffer->comp.id = comp_id;
	buffer->comp.pipeline_id = pipeline_id;
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

int tplg_load_pcm(int comp_id, int pipeline_id, int size, int dir,
		  struct sof_ipc_comp_host *host, FILE *file)
{
	struct snd_soc_tplg_vendor_array *array = NULL;
	size_t total_array_size = 0, read_size;
	int ret;

	/* configure host comp IPC message */
	host->comp.hdr.size = sizeof(*host);
	host->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	host->comp.id = comp_id;
	host->comp.type = SOF_COMP_HOST;
	host->comp.pipeline_id = pipeline_id;
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

/* load dai component */
int tplg_load_dai(int comp_id, int pipeline_id, int size,
		  struct sof_ipc_comp_dai *comp_dai, FILE *file)
{
	struct snd_soc_tplg_vendor_array *array;
	size_t total_array_size = 0, read_size;
	int ret;

	/* configure comp_dai */
	comp_dai->comp.hdr.size = sizeof(*comp_dai);
	comp_dai->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	comp_dai->comp.id = comp_id;
	comp_dai->comp.type = SOF_COMP_DAI;
	comp_dai->comp.pipeline_id = pipeline_id;
	comp_dai->config.hdr.size = sizeof(comp_dai->config);

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
		if (ret != 1) {
			free(array);
			return -EINVAL;
		}

		/* check for array size mismatch */
		if (!is_valid_priv_size(total_array_size, size, array)) {
			fprintf(stderr, "error: load dai array size mismatch\n");
			free(array);
			return -EINVAL;
		}

		ret = tplg_read_array(array, file);
		if (ret) {
			fprintf(stderr, "error: read array fail\n");
			free(array);
			return ret;
		}

		ret = sof_parse_tokens(comp_dai, dai_tokens,
				       ARRAY_SIZE(dai_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse dai tokens failed %d\n",
				size);
			free(array);
			return -EINVAL;
		}

		/* parse comp tokens */
		ret = sof_parse_tokens(&comp_dai->config, comp_tokens,
				       ARRAY_SIZE(comp_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse filewrite tokens %d\n",
				size);
			free(array);
			return -EINVAL;
		}
		total_array_size += array->size;
	}

	free(array);
	return 0;
}

/* load pda dapm widget */
int tplg_load_pga(int comp_id, int pipeline_id, int size,
		  struct sof_ipc_comp_volume *volume, FILE *file)
{
	struct snd_soc_tplg_vendor_array *array = NULL;
	size_t total_array_size = 0, read_size;
	int ret;

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
		if (ret != 1) {
			free(array);
			return -EINVAL;
		}

		/* check for array size mismatch */
		if (!is_valid_priv_size(total_array_size, size, array)) {
			fprintf(stderr, "error: pga array size mismatch\n");
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
		ret = sof_parse_tokens(&volume->config, comp_tokens,
				       ARRAY_SIZE(comp_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse pga comp tokens %d\n",
				size);
			free(array);
			return -EINVAL;
		}

		/* parse volume tokens */
		ret = sof_parse_tokens(volume, volume_tokens,
				       ARRAY_SIZE(volume_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse src tokens %d\n", size);
			free(array);
			return -EINVAL;
		}

		total_array_size += array->size;
	}

	/* configure volume */
	volume->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	volume->comp.id = comp_id;
	volume->comp.hdr.size = sizeof(struct sof_ipc_comp_volume);
	volume->comp.type = SOF_COMP_VOLUME;
	volume->comp.pipeline_id = pipeline_id;
	volume->config.hdr.size = sizeof(struct sof_ipc_comp_config);

	free(array);
	return 0;
}

/* load scheduler dapm widget */
int tplg_load_pipeline(int comp_id, int pipeline_id, int size,
		       struct sof_ipc_pipe_new *pipeline, FILE *file)
{
	struct snd_soc_tplg_vendor_array *array = NULL;
	size_t total_array_size = 0, read_size;
	int ret;

	/* configure pipeline */
	pipeline->comp_id = comp_id;
	pipeline->pipeline_id = pipeline_id;
	pipeline->hdr.size = sizeof(*pipeline);
	pipeline->hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_PIPE_NEW;
	pipeline->comp_id = comp_id;
	pipeline->pipeline_id = pipeline_id;

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

int tplg_load_one_control(struct snd_soc_tplg_ctl_hdr **ctl, char **priv_data,
			  FILE *file)
{
	struct snd_soc_tplg_ctl_hdr *ctl_hdr;
	struct snd_soc_tplg_mixer_control *mixer_ctl = NULL;
	struct snd_soc_tplg_enum_control *enum_ctl = NULL;
	struct snd_soc_tplg_bytes_control *bytes_ctl = NULL;
	size_t rewind_size;
	size_t read_size;
	size_t hdr_size;
	int ret;

	/* These are set if success */
	*ctl = NULL;
	*priv_data = NULL;

	/* allocate memory */
	hdr_size = sizeof(struct snd_soc_tplg_ctl_hdr);
	ctl_hdr = (struct snd_soc_tplg_ctl_hdr *)malloc(hdr_size);
	if (!ctl_hdr) {
		fprintf(stderr, "error: mem alloc\n");
		return -errno;
	}

	/* read control header */
	ret = fread(ctl_hdr, hdr_size, 1, file);
	if (ret != 1) {
		ret = -EINVAL;
		goto err;
	}

	/* load control based on type */
	switch (ctl_hdr->ops.info) {
	case SND_SOC_TPLG_CTL_VOLSW:
	case SND_SOC_TPLG_CTL_STROBE:
	case SND_SOC_TPLG_CTL_VOLSW_SX:
	case SND_SOC_TPLG_CTL_VOLSW_XR_SX:
	case SND_SOC_TPLG_CTL_RANGE:
	case SND_SOC_TPLG_DAPM_CTL_VOLSW:
		/* load mixer type control */
		rewind_size = sizeof(struct snd_soc_tplg_ctl_hdr);
		if (fseek(file, (long)rewind_size * -1, SEEK_CUR)) {
			ret = -errno;
			goto err;
		}

		read_size = sizeof(struct snd_soc_tplg_mixer_control);
		mixer_ctl = malloc(read_size);
		if (!mixer_ctl) {
			fprintf(stderr, "error: mem alloc\n");
			ret = -errno;
			goto err;
		}

		ret = fread(mixer_ctl, read_size, 1, file);
		if (ret != 1) {
			ret = -EINVAL;
			goto err;
		}

		/* skip mixer private data */
		if (fseek(file, mixer_ctl->priv.size, SEEK_CUR)) {
			ret = -errno;
			goto err;
		}

		*ctl = (struct snd_soc_tplg_ctl_hdr *)mixer_ctl;
		break;

	case SND_SOC_TPLG_CTL_ENUM:
	case SND_SOC_TPLG_CTL_ENUM_VALUE:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_DOUBLE:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_VIRT:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_VALUE:
		/* load enum type control */
		rewind_size = sizeof(struct snd_soc_tplg_ctl_hdr);
		if (fseek(file, (long)rewind_size * -1, SEEK_CUR)) {
			ret = -errno;
			goto err;
		}

		read_size = sizeof(struct snd_soc_tplg_enum_control);
		enum_ctl = malloc(read_size);
		if (!enum_ctl) {
			fprintf(stderr, "error: mem alloc\n");
			ret = -errno;
			goto err;
		}

		ret = fread(enum_ctl, read_size, 1, file);
		if (ret != 1) {
			ret = -EINVAL;
			goto err;
		}

		/* skip enum private data */
		if (fseek(file, enum_ctl->priv.size, SEEK_CUR)) {
			ret = -errno;
			goto err;
		}

		*ctl = (struct snd_soc_tplg_ctl_hdr *)enum_ctl;
		break;

	case SND_SOC_TPLG_CTL_BYTES:
		/* load bytes type controls */
		rewind_size = sizeof(struct snd_soc_tplg_ctl_hdr);
		if (fseek(file, (long)rewind_size * -1, SEEK_CUR)) {
			ret = -errno;
			goto err;
		}

		read_size = sizeof(struct snd_soc_tplg_bytes_control);
		bytes_ctl = malloc(read_size);
		if (!bytes_ctl) {
			fprintf(stderr, "error: mem alloc\n");
			ret = -errno;
			goto err;
		}

		ret = fread(bytes_ctl, read_size, 1, file);
		if (ret != 1) {
			ret = -EINVAL;
			goto err;
		}

		/* Get private data */
		read_size = bytes_ctl->priv.size;
		*priv_data = malloc(read_size);
		if (!(*priv_data)) {
			fprintf(stderr, "error: mem alloc\n");
			ret = -errno;
			goto err;
		}

		ret = fread(*priv_data, read_size, 1, file);
		if (ret != 1) {
			ret = -EINVAL;
			goto err;
		}

		*ctl = (struct snd_soc_tplg_ctl_hdr *)bytes_ctl;
		break;

	default:
		printf("info: control type not supported\n");
		return -EINVAL;
	}

	free(ctl_hdr);
	return 0;

err:
	/* free all data */
	free(ctl_hdr);
	free(mixer_ctl);
	free(enum_ctl);
	free(bytes_ctl);
	free(*priv_data);
	return ret;
}

/* load dapm widget kcontrols
 * we don't use controls in the fuzzer atm.
 * so just skip to the next dapm widget
 */
int tplg_load_controls(int num_kcontrols, FILE *file)
{
	struct snd_soc_tplg_ctl_hdr *ctl_hdr;
	struct snd_soc_tplg_mixer_control *mixer_ctl;
	struct snd_soc_tplg_enum_control *enum_ctl;
	struct snd_soc_tplg_bytes_control *bytes_ctl;
	size_t read_size, size;
	int j, ret = 0;

	/* allocate memory */
	size = sizeof(struct snd_soc_tplg_ctl_hdr);
	ctl_hdr = (struct snd_soc_tplg_ctl_hdr *)malloc(size);
	if (!ctl_hdr) {
		fprintf(stderr, "error: mem alloc\n");
		return -errno;
	}

	size = sizeof(struct snd_soc_tplg_mixer_control);
	mixer_ctl = (struct snd_soc_tplg_mixer_control *)malloc(size);
	if (!mixer_ctl) {
		fprintf(stderr, "error: mem alloc\n");
		free(ctl_hdr);
		return -errno;
	}

	size = sizeof(struct snd_soc_tplg_enum_control);
	enum_ctl = (struct snd_soc_tplg_enum_control *)malloc(size);
	if (!enum_ctl) {
		fprintf(stderr, "error: mem alloc\n");
		free(ctl_hdr);
		free(mixer_ctl);
		return -errno;
	}

	size = sizeof(struct snd_soc_tplg_bytes_control);
	bytes_ctl = (struct snd_soc_tplg_bytes_control *)malloc(size);
	if (!bytes_ctl) {
		fprintf(stderr, "error: mem alloc\n");
		free(ctl_hdr);
		free(mixer_ctl);
		free(enum_ctl);
		return -errno;
	}

	for (j = 0; j < num_kcontrols; j++) {
		/* read control header */
		read_size = sizeof(struct snd_soc_tplg_ctl_hdr);
		ret = fread(ctl_hdr, read_size, 1, file);
		if (ret != 1) {
			ret = -EINVAL;
			goto err;
		}

		/* load control based on type */
		switch (ctl_hdr->ops.info) {
		case SND_SOC_TPLG_CTL_VOLSW:
		case SND_SOC_TPLG_CTL_STROBE:
		case SND_SOC_TPLG_CTL_VOLSW_SX:
		case SND_SOC_TPLG_CTL_VOLSW_XR_SX:
		case SND_SOC_TPLG_CTL_RANGE:
		case SND_SOC_TPLG_DAPM_CTL_VOLSW:
			/* load mixer type control */
			read_size = sizeof(struct snd_soc_tplg_ctl_hdr);
			if (fseek(file, (long)read_size * -1, SEEK_CUR)) {
				ret = -errno;
				goto err;
			}

			read_size = sizeof(struct snd_soc_tplg_mixer_control);
			ret = fread(mixer_ctl, read_size, 1, file);
			if (ret != 1) {
				ret = -EINVAL;
				goto err;
			}

			/* skip mixer private data */
			if (fseek(file, mixer_ctl->priv.size, SEEK_CUR)) {
				ret = -errno;
				goto err;
			}
			break;

		case SND_SOC_TPLG_CTL_ENUM:
		case SND_SOC_TPLG_CTL_ENUM_VALUE:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_DOUBLE:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_VIRT:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_VALUE:
			/* load enum type control */
			read_size = sizeof(struct snd_soc_tplg_ctl_hdr);
			if (fseek(file, (long)read_size * -1, SEEK_CUR)) {
				ret = -errno;
				goto err;
			}

			read_size = sizeof(struct snd_soc_tplg_enum_control);
			ret = fread(enum_ctl, read_size, 1, file);
			if (ret != 1) {
				ret = -EINVAL;
				goto err;
			}

			/* skip enum private data */
			if (fseek(file, enum_ctl->priv.size, SEEK_CUR)) {
				ret = -errno;
				goto err;
			}
			break;

		case SND_SOC_TPLG_CTL_BYTES:
			/* load bytes type controls */
			read_size = sizeof(struct snd_soc_tplg_ctl_hdr);
			if (fseek(file, (long)read_size * -1, SEEK_CUR)) {
				ret = -errno;
				goto err;
			}

			read_size = sizeof(struct snd_soc_tplg_bytes_control);
			ret = fread(bytes_ctl, read_size, 1, file);
			if (ret != 1) {
				ret = -EINVAL;
				goto err;
			}

			/* skip bytes private data */
			if (fseek(file, bytes_ctl->priv.size, SEEK_CUR)) {
				ret = -errno;
				goto err;
			}
			break;

		default:
			printf("info: control type not supported\n");
			return -EINVAL;
		}
	}

err:
	/* free all data */
	free(mixer_ctl);
	free(enum_ctl);
	free(bytes_ctl);
	free(ctl_hdr);
	return ret;
}

/* load src dapm widget */
int tplg_load_src(int comp_id, int pipeline_id, int size,
		  struct sof_ipc_comp_src *src, FILE *file)
{
	struct snd_soc_tplg_vendor_array *array = NULL;
	size_t total_array_size = 0, read_size;
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
	src->comp.pipeline_id = pipeline_id;
	src->config.hdr.size = sizeof(struct sof_ipc_comp_config);

	free(array);
	return 0;
}

/* load asrc dapm widget */
int tplg_load_asrc(int comp_id, int pipeline_id, int size,
		   struct sof_ipc_comp_asrc *asrc, FILE *file)
{
	struct snd_soc_tplg_vendor_array *array = NULL;
	size_t total_array_size = 0, read_size;
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
			fprintf(stderr, "error: parse asrc comp_tokens %d\n",
				size);
			free(MOVE_POINTER_BY_BYTES(array, -total_array_size));
			return -EINVAL;
		}

		/* parse asrc tokens */
		ret = sof_parse_tokens(asrc, asrc_tokens,
				       ARRAY_SIZE(asrc_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse asrc tokens %d\n", size);
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
	asrc->comp.pipeline_id = pipeline_id;
	asrc->config.hdr.size = sizeof(struct sof_ipc_comp_config);

	free(array);
	return 0;
}

/* load asrc dapm widget */
int tplg_load_process(int comp_id, int pipeline_id, int size,
		      struct sof_ipc_comp_process *process, FILE *file,
		      struct sof_ipc_comp_ext *comp_ext)
{
	struct snd_soc_tplg_vendor_array *array = NULL;
	size_t total_array_size = 0;
	size_t read_size;
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
	process->comp.type = find_process_comp_type(process->type);
	process->comp.pipeline_id = pipeline_id;
	process->config.hdr.size = sizeof(struct sof_ipc_comp_config);

	free(array);
	return 0;
}

/* load mixer dapm widget */
int tplg_load_mixer(int comp_id, int pipeline_id, int size,
		    struct sof_ipc_comp_mixer *mixer, FILE *file)
{
	struct snd_soc_tplg_vendor_array *array = NULL;
	size_t total_array_size = 0, read_size;
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
			fprintf(stderr, "error: load mixer array size mismatch\n");
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
		ret = sof_parse_tokens(&mixer->config, comp_tokens,
				       ARRAY_SIZE(comp_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse src comp_tokens %d\n",
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

	/* configure src */
	mixer->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	mixer->comp.id = comp_id;
	mixer->comp.hdr.size = sizeof(struct sof_ipc_comp_src);
	mixer->comp.type = SOF_COMP_MIXER;
	mixer->comp.pipeline_id = pipeline_id;
	mixer->config.hdr.size = sizeof(struct sof_ipc_comp_config);

	free(array);
	return 0;
}

/* load pipeline graph DAPM widget*/
int tplg_load_graph(int num_comps, int pipeline_id,
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

/* load dapm widget */
int load_widget(void *dev, int dev_type, struct comp_info *temp_comp_list,
		int comp_id, int comp_index, int pipeline_id,
		void *tp, int *sched_id, FILE *file)
{
	struct snd_soc_tplg_dapm_widget *widget;
	size_t read_size;
	size_t size;
	int ret = 0;

	if (!temp_comp_list) {
		fprintf(stderr, "load_widget: temp_comp_list argument NULL\n");
		return -EINVAL;
	}

	/* allocate memory for widget */
	size = sizeof(struct snd_soc_tplg_dapm_widget);
	widget = (struct snd_soc_tplg_dapm_widget *)malloc(size);
	if (!widget) {
		fprintf(stderr, "error: mem alloc\n");
		return -errno;
	}

	/* read widget data */
	read_size = sizeof(struct snd_soc_tplg_dapm_widget);
	ret = fread(widget, read_size, 1, file);
	if (ret != 1) {
		ret = -EINVAL;
		goto exit;
	}

	/*
	 * create a list with all widget info
	 * containing mapping between component names and ids
	 * which will be used for setting up component connections
	 */
	temp_comp_list[comp_index].id = comp_id;
	temp_comp_list[comp_index].name = strdup(widget->name);
	temp_comp_list[comp_index].type = widget->id;
	temp_comp_list[comp_index].pipeline_id = pipeline_id;

	printf("debug: loading comp_id %d: widget %s id %d\n",
	       comp_id, widget->name, widget->id);

	/* load widget based on type */
	switch (widget->id) {

	/* load pga widget */
	case(SND_SOC_TPLG_DAPM_PGA):
		if (load_pga(dev, comp_id, pipeline_id, widget) < 0) {
			fprintf(stderr, "error: load pga\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case(SND_SOC_TPLG_DAPM_AIF_IN):
		if (load_aif_in_out(dev, comp_id, pipeline_id, widget,
				    SOF_IPC_STREAM_PLAYBACK, tp) < 0) {
			fprintf(stderr, "error: load AIF IN failed\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case(SND_SOC_TPLG_DAPM_AIF_OUT):
		if (load_aif_in_out(dev, comp_id, pipeline_id, widget,
				    SOF_IPC_STREAM_CAPTURE, tp) < 0) {
			fprintf(stderr, "error: load AIF OUT failed\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case(SND_SOC_TPLG_DAPM_DAI_IN):
		if (load_dai_in_out(dev, comp_id, pipeline_id, widget,
				    SOF_IPC_STREAM_PLAYBACK, tp) < 0) {
			fprintf(stderr, "error: load filewrite\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case(SND_SOC_TPLG_DAPM_DAI_OUT):
		if (load_dai_in_out(dev, comp_id, pipeline_id, widget,
				    SOF_IPC_STREAM_CAPTURE, tp) < 0) {
			fprintf(stderr, "error: load filewrite\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case(SND_SOC_TPLG_DAPM_BUFFER):
		if (load_buffer(dev, comp_id, pipeline_id, widget) < 0) {
			fprintf(stderr, "error: load buffer\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case(SND_SOC_TPLG_DAPM_SCHEDULER):
		/* find comp id for scheduling comp */
		if (dev_type == FUZZER_DEV)
			*sched_id = find_widget(temp_comp_list, comp_id,
						widget->sname);

		if (load_pipeline(dev, comp_id, pipeline_id, widget,
				  *sched_id) < 0) {
			fprintf(stderr, "error: load pipeline\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case(SND_SOC_TPLG_DAPM_SRC):
		if (load_src(dev, comp_id, pipeline_id, widget, tp) < 0) {
			fprintf(stderr, "error: load src\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case(SND_SOC_TPLG_DAPM_ASRC):
		if (load_asrc(dev, comp_id, pipeline_id, widget, tp) < 0) {
			fprintf(stderr, "error: load src\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case(SND_SOC_TPLG_DAPM_MIXER):
		if (load_mixer(dev, comp_id, pipeline_id, widget) < 0) {
			fprintf(stderr, "error: load mixer\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	case(SND_SOC_TPLG_DAPM_EFFECT):
		if (load_process(dev, comp_id, pipeline_id, widget) < 0) {
			fprintf(stderr, "error: load effect\n");
			ret = -EINVAL;
			goto exit;
		}
		break;
	/* unsupported widgets */
	default:
		if (fseek(file, widget->priv.size, SEEK_CUR)) {
			fprintf(stderr, "error: fseek unsupported widget\n");
			ret = -errno;
			goto exit;
		}

		printf("info: Widget type not supported %d\n", widget->id);
		ret = tplg_load_controls(widget->num_kcontrols, file);
		if (ret < 0) {
			fprintf(stderr, "error: loading controls\n");
			goto exit;
		}
		break;
	}

	ret = 0;

exit:
	/* free allocated widget data */
	free(widget);
	return ret;
}

/* parse vendor tokens in topology */
int sof_parse_tokens(void *object, const struct sof_topology_token *tokens,
		     int count, struct snd_soc_tplg_vendor_array *array,
		     int priv_size)
{
	int asize;
	int ret = 0;

	while (priv_size > 0 && ret == 0) {
		asize = array->size;

		/* validate asize */
		if (asize < 0) { /* FIXME: A zero-size array makes no sense */
			fprintf(stderr, "error: invalid array size 0x%x\n",
				asize);
			return -EINVAL;
		}

		/* make sure there is enough data before parsing */
		priv_size -= asize;

		if (priv_size < 0) {
			fprintf(stderr, "error: invalid priv size 0x%x\n",
				asize);
			return -EINVAL;
		}

		/* call correct parser depending on type */
		switch (array->type) {
		case SND_SOC_TPLG_TUPLE_TYPE_UUID:
			ret = sof_parse_uuid_tokens(object, tokens, count, array);
			break;
		case SND_SOC_TPLG_TUPLE_TYPE_STRING:
			ret = sof_parse_string_tokens(object, tokens, count, array);
			break;
		case SND_SOC_TPLG_TUPLE_TYPE_BOOL:
		case SND_SOC_TPLG_TUPLE_TYPE_BYTE:
		case SND_SOC_TPLG_TUPLE_TYPE_WORD:
		case SND_SOC_TPLG_TUPLE_TYPE_SHORT:
			ret = sof_parse_word_tokens(object, tokens, count, array);
			break;
		default:
			fprintf(stderr, "error: unknown token type %d\n",
				array->type);
			return -EINVAL;
		}
	}
	return ret;
}

/* parse word tokens */
int sof_parse_word_tokens(void *object,
			  const struct sof_topology_token *tokens,
			  int count,
			  struct snd_soc_tplg_vendor_array *array)
{
	struct snd_soc_tplg_vendor_value_elem *elem;
	int i, j;

	if (sizeof(struct snd_soc_tplg_vendor_value_elem) * array->num_elems +
		sizeof(struct snd_soc_tplg_vendor_array) > array->size) {
		fprintf(stderr, "error: illegal array number of elements %d\n",
			array->num_elems);
		return -EINVAL;
	}

	/* parse element by element */
	for (i = 0; i < array->num_elems; i++) {
		elem = &array->value[i];

		/* search for token */
		for (j = 0; j < count; j++) {
			/* match token type */
			if (tokens[j].type != SND_SOC_TPLG_TUPLE_TYPE_WORD)
				continue;

			/* match token id */
			if (tokens[j].token != elem->token)
				continue;

			/* matched - now load token */
			tokens[j].get_token(elem, object, tokens[j].offset,
					    tokens[j].size);
		}
	}

	return 0;
}

/* parse uuid tokens */
int sof_parse_uuid_tokens(void *object,
			  const struct sof_topology_token *tokens,
			  int count,
			  struct snd_soc_tplg_vendor_array *array)
{
	struct snd_soc_tplg_vendor_uuid_elem *elem;
	int i, j;

	if (sizeof(struct snd_soc_tplg_vendor_uuid_elem) * array->num_elems +
		sizeof(struct snd_soc_tplg_vendor_array) > array->size) {
		fprintf(stderr, "error: illegal array number of elements %d\n",
			array->num_elems);
		return -EINVAL;
	}

	/* parse element by element */
	for (i = 0; i < array->num_elems; i++) {
		elem = &array->uuid[i];

		/* search for token */
		for (j = 0; j < count; j++) {
			/* match token type */
			if (tokens[j].type != SND_SOC_TPLG_TUPLE_TYPE_UUID)
				continue;

			/* match token id */
			if (tokens[j].token != elem->token)
				continue;

			/* matched - now load token */
			tokens[j].get_token(elem, object, tokens[j].offset,
					    tokens[j].size);
		}
	}

	return 0;
}

/* parse string tokens */
int sof_parse_string_tokens(void *object,
			    const struct sof_topology_token *tokens,
			    int count,
			    struct snd_soc_tplg_vendor_array *array)
{
	struct snd_soc_tplg_vendor_string_elem *elem;
	int i, j;

	if (sizeof(struct snd_soc_tplg_vendor_string_elem) * array->num_elems +
		sizeof(struct snd_soc_tplg_vendor_array) > array->size) {
		fprintf(stderr, "error: illegal array number of elements %d\n",
			array->num_elems);
		return -EINVAL;
	}

	/* parse element by element */
	for (i = 0; i < array->num_elems; i++) {
		elem = &array->string[i];

		/* search for token */
		for (j = 0; j < count; j++) {
			/* match token type */
			if (tokens[j].type != SND_SOC_TPLG_TUPLE_TYPE_STRING)
				continue;

			/* match token id */
			if (tokens[j].token != elem->token)
				continue;

			/* matched - now load token */
			tokens[j].get_token(elem, object, tokens[j].offset,
					    tokens[j].size);
		}
	}

	return 0;
}

enum sof_ipc_frame find_format(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sof_frames); i++) {
		if (strcmp(name, sof_frames[i].name) == 0)
			return sof_frames[i].frame;
	}

	/* use s32le if nothing is specified */
	return SOF_IPC_FRAME_S32_LE;
}

/* helper functions to get tokens */
int get_token_uint32_t(void *elem, void *object, uint32_t offset,
		       uint32_t size)
{
	struct snd_soc_tplg_vendor_value_elem *velem = elem;
	uint32_t *val = object + offset;

	*val = velem->value;
	return 0;
}

int get_token_uuid(void *elem, void *object, uint32_t offset,
		   uint32_t size)
{
	struct snd_soc_tplg_vendor_uuid_elem *velem = elem;
	uint8_t *dst = (uint8_t *)object + offset;

	memcpy(dst, velem->uuid, UUID_SIZE);

	return 0;
}

int get_token_comp_format(void *elem, void *object, uint32_t offset,
			  uint32_t size)
{
	struct snd_soc_tplg_vendor_string_elem *velem = elem;
	uint32_t *val = object + offset;

	*val = find_format(velem->string);
	return 0;
}

int get_token_dai_type(void *elem, void *object, uint32_t offset, uint32_t size)
{
	struct snd_soc_tplg_vendor_string_elem *velem = elem;
	uint32_t *val = (uint32_t *)((uint8_t *)object + offset);

	*val = find_dai(velem->string);
	return 0;
}

int get_token_process_type(void *elem, void *object, uint32_t offset,
			   uint32_t size)
{
	struct snd_soc_tplg_vendor_string_elem *velem = elem;
	uint32_t *val = (uint32_t *)((uint8_t *)object + offset);

	*val = find_process(velem->string);
	return 0;
}
