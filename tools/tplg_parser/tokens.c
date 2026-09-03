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

struct frame_types {
	char *name;
	enum sof_ipc_frame frame;
};

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

enum sof_ipc_frame tplg_find_format(const char *name)
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
int tplg_token_get_uint32_t(void *elem, void *object, uint32_t offset,
			    uint32_t size)
{
	struct snd_soc_tplg_vendor_value_elem *velem = elem;
	char *vobject = object;
	uint32_t *val = (uint32_t *)(vobject + offset);

	*val = velem->value;
	return 0;
}

int tplg_token_get_uuid(void *elem, void *object, uint32_t offset,
			uint32_t size)
{
	struct snd_soc_tplg_vendor_uuid_elem *velem = elem;
	uint8_t *dst = (uint8_t *)object + offset;

	memcpy(dst, velem->uuid, UUID_SIZE);

	return 0;
}

int tplg_token_get_comp_format(void *elem, void *object, uint32_t offset,
			       uint32_t size)
{
	struct snd_soc_tplg_vendor_string_elem *velem = elem;
	char *vobject = object;
	uint32_t *val = (uint32_t *)(vobject + offset);

	*val = tplg_find_format(velem->string);
	return 0;
}

int sof_parse_token_sets(void *object, const struct sof_topology_token *tokens,
			 int count, struct snd_soc_tplg_vendor_array *array,
			 int priv_size, int num_sets, int object_size)
{
	size_t offset = 0;
	int total = 0;
	int found = 0;
	int asize;
	int ret;

	while (priv_size > 0 && total < count * num_sets) {
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
				priv_size);
			return -EINVAL;
		}

		/* call correct parser depending on type */
		switch (array->type) {
		case SND_SOC_TPLG_TUPLE_TYPE_UUID:
			found = sof_parse_uuid_tokens(object + offset, tokens, count, array);
			break;
		case SND_SOC_TPLG_TUPLE_TYPE_STRING:
			ret = sof_parse_string_tokens(object + offset, tokens, count, array);
			if (ret < 0)
				return ret;
			found = ret;
			break;
		case SND_SOC_TPLG_TUPLE_TYPE_BOOL:
		case SND_SOC_TPLG_TUPLE_TYPE_BYTE:
		case SND_SOC_TPLG_TUPLE_TYPE_WORD:
		case SND_SOC_TPLG_TUPLE_TYPE_SHORT:
			found = sof_parse_word_tokens(object + offset, tokens, count, array);
			break;
		default:
			fprintf(stderr, "error: unknown token type %d\n",
				array->type);
			return -EINVAL;
		}

		array = MOVE_POINTER_BY_BYTES(array, array->size);

		if (found > 0) {
			total += count;
			offset += object_size;
			found = 0;
		}
	}

	return 0;
}

/* parse vendor tokens in topology */
int sof_parse_tokens(void *object, const struct sof_topology_token *tokens,
		     int count, struct snd_soc_tplg_vendor_array *array,
		     int priv_size)
{
	/* object_size is not needed when parsing only 1 set of tokens */
	return sof_parse_token_sets(object, tokens, count, array, priv_size, 1, 0);
}

/* parse word tokens */
int sof_parse_word_tokens(void *object,
			  const struct sof_topology_token *tokens,
			  int count,
			  struct snd_soc_tplg_vendor_array *array)
{
	struct snd_soc_tplg_vendor_value_elem *elem;
	int found = 0;
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
			found++;
		}
	}

	return found;
}

/* parse uuid tokens */
int sof_parse_uuid_tokens(void *object,
			  const struct sof_topology_token *tokens,
			  int count,
			  struct snd_soc_tplg_vendor_array *array)
{
	struct snd_soc_tplg_vendor_uuid_elem *elem;
	int found = 0;
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
			found++;
		}
	}

	return found;
}

/* parse string tokens */
int sof_parse_string_tokens(void *object,
			    const struct sof_topology_token *tokens,
			    int count,
			    struct snd_soc_tplg_vendor_array *array)
{
	struct snd_soc_tplg_vendor_string_elem *elem;
	int found = 0;
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
			found++;
		}
	}

	return found;
}

bool tplg_is_valid_priv_size(size_t size_read, size_t priv_size,
			     struct snd_soc_tplg_vendor_array *array)
{
	size_t arr_size, elem_size, arr_elems_size;
	bool valid;

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
	valid = size_read + arr_size + arr_elems_size <= priv_size;
	if (!valid)
		fprintf(stderr, "error: invalid private data size %zu read %zu array %zu elems %zu\n",
			priv_size, size_read, arr_size, arr_elems_size);
	return valid;
}
