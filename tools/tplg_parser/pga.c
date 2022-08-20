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
#include <math.h>
#include <ipc/topology.h>
#include <sof/lib/uuid.h>
#include <sof/ipc/topology.h>
#include <tplg_parser/topology.h>
#include <tplg_parser/tokens.h>

/* volume */
static const struct sof_topology_token volume_tokens[] = {
	{SOF_TKN_VOLUME_RAMP_STEP_TYPE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		get_token_uint32_t,
		offsetof(struct sof_ipc_comp_volume, ramp), 0},
	{SOF_TKN_VOLUME_RAMP_STEP_MS,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, get_token_uint32_t,
		offsetof(struct sof_ipc_comp_volume, initial_ramp), 0},
};

/* load pda dapm widget */
int tplg_create_pga(struct tplg_context *ctx, struct sof_ipc_comp_volume *volume,
		    size_t max_comp_size)
{
	struct snd_soc_tplg_vendor_array *array = NULL;
	size_t total_array_size = 0, read_size;
	FILE *file = ctx->file;
	int size = ctx->widget->priv.size;
	int comp_id = ctx->comp_id;
	char uuid[UUID_SIZE];
	int ret;

	if (max_comp_size < sizeof(struct sof_ipc_comp_volume) + UUID_SIZE)
		return -EINVAL;

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

		/* parse uuid token */
		ret = sof_parse_tokens(uuid, comp_ext_tokens,
				       ARRAY_SIZE(comp_ext_tokens), array,
				       array->size);
		if (ret != 0) {
			fprintf(stderr, "error: parse pga uuid token %d\n", size);
			free(array);
			return -EINVAL;
		}

		total_array_size += array->size;
	}

	/* configure volume */
	volume->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	volume->comp.id = comp_id;
	volume->comp.hdr.size = sizeof(struct sof_ipc_comp_volume) + UUID_SIZE;
	volume->comp.type = SOF_COMP_VOLUME;
	volume->comp.pipeline_id = ctx->pipeline_id;
	volume->comp.ext_data_length = UUID_SIZE;
	volume->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	memcpy(volume + 1, &uuid, UUID_SIZE);

	free(array);
	return 0;
}

/* load pda dapm widget */
int tplg_new_pga(struct tplg_context *ctx, struct sof_ipc_comp *comp, size_t comp_size,
		struct snd_soc_tplg_ctl_hdr *rctl, size_t max_ctl_size)
{
	struct snd_soc_tplg_ctl_hdr *ctl = NULL;
	struct sof_ipc_comp_volume *volume = (struct sof_ipc_comp_volume *)comp;
	struct snd_soc_tplg_mixer_control *mixer_ctl;
	char *priv_data = NULL;
	int32_t vol_min = 0;
	int32_t vol_step = 0;
	int32_t vol_maxs = 0;
	float vol_min_db;
	float vol_max_db;
	int channels = 0;
	int ret = 0;

	ret = tplg_create_pga(ctx, volume, comp_size);
	if (ret < 0)
		goto err;

	/* Only one control is supported*/
	if (ctx->widget->num_kcontrols > 1) {
		fprintf(stderr, "error: more than one kcontrol defined\n");
		ret = -EINVAL;
		goto err;
	}

	/* Get control into ctl and priv_data */
	if (ctx->widget->num_kcontrols) {
		ret = tplg_create_single_control(&ctl, &priv_data, ctx->file);
		if (ret < 0) {
			fprintf(stderr, "error: failed control load\n");
			goto err;
		}

		/* Get volume scale */
		mixer_ctl = (struct snd_soc_tplg_mixer_control *)ctl;
		vol_min = (int32_t)mixer_ctl->hdr.tlv.scale.min;
		vol_step = mixer_ctl->hdr.tlv.scale.step;
		vol_maxs = mixer_ctl->max;
		channels = mixer_ctl->num_channels;

		/* make sure the CTL will fit if we need to copy it for others */
		if (max_ctl_size && ctl->size > max_ctl_size) {
			fprintf(stderr, "error: failed pga control copy\n");
			ret = -EINVAL;
			goto out;
		} else if (rctl)
			memcpy(rctl, ctl, ctl->size);
	}

	vol_min_db = 0.01 * vol_min;
	vol_max_db = 0.01 * (vol_maxs * vol_step) + vol_min_db;
	volume->min_value = round(pow(10, vol_min_db / 20.0) * 65535);
	volume->max_value = round(pow(10, vol_max_db / 20.0) * 65536);
	volume->channels = channels;

out:
	free(ctl);
	free(priv_data);

err:
	return ret;
}
