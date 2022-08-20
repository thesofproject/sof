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

static const struct frame_types sof_frames[] = {
	/* TODO: fix topology to use ALSA formats */
	{"s16le", SOF_IPC_FRAME_S16_LE},
	{"s24le", SOF_IPC_FRAME_S24_4LE},
	{"s32le", SOF_IPC_FRAME_S32_LE},
	{"float", SOF_IPC_FRAME_FLOAT},
	/* ALSA formats */
	{"S16_LE", SOF_IPC_FRAME_S16_LE},
	{"S24_LE", SOF_IPC_FRAME_S24_4LE},
	{"S32_LE", SOF_IPC_FRAME_S32_LE},
	{"FLOAT_LE", SOF_IPC_FRAME_FLOAT},
};

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

	*val = tplg_get_process_name(velem->string);
	return 0;
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

enum sof_ipc_process_type tplg_get_process_name(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sof_process); i++) {
		if (strcmp(name, sof_process[i].name) == 0)
			return sof_process[i].type;
	}

	return SOF_PROCESS_NONE;
}

enum sof_comp_type tplg_get_process_type(enum sof_ipc_process_type type)
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
