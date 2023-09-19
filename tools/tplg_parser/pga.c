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
#include <volume/peak_volume.h>

#define SOF_IPC4_VOL_ZERO_DB	0x7fffffff

/* volume IPC3 */
static const struct sof_topology_token volume3_tokens[] = {
	{SOF_TKN_VOLUME_RAMP_STEP_TYPE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_comp_volume, ramp), 0},
	{SOF_TKN_VOLUME_RAMP_STEP_MS,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, tplg_token_get_uint32_t,
		offsetof(struct sof_ipc_comp_volume, initial_ramp), 0},
};

static const struct sof_topology_token_group pga_ipc3_tokens[] = {
	{volume3_tokens, ARRAY_SIZE(volume3_tokens)},
	{comp_tokens, ARRAY_SIZE(comp_tokens),
		offsetof(struct sof_ipc_comp_volume, config)},
	{comp_ext_tokens, ARRAY_SIZE(comp_ext_tokens),
		sizeof(struct sof_ipc_comp_volume)},
};

static int pga_ipc3_build(struct tplg_context *ctx, void *_pga)
{
	struct sof_ipc_comp_volume *volume = _pga;
	struct snd_soc_tplg_private *priv_data = NULL;
	struct snd_soc_tplg_ctl_hdr *ctl = NULL;
	struct snd_soc_tplg_mixer_control *mixer_ctl;
	int32_t vol_min = 0;
	int32_t vol_step = 0;
	int32_t vol_maxs = 0;
	float vol_min_db;
	float vol_max_db;
	int channels = 0;
	int i, ret = 0;

	/* configure volume */
	volume->comp.hdr.cmd = SOF_IPC_GLB_TPLG_MSG | SOF_IPC_TPLG_COMP_NEW;
	volume->comp.id = ctx->comp_id;
	volume->comp.hdr.size = sizeof(struct sof_ipc_comp_volume) + UUID_SIZE;
	volume->comp.type = SOF_COMP_VOLUME;
	volume->comp.pipeline_id = ctx->pipeline_id;
	volume->comp.ext_data_length = UUID_SIZE;
	volume->config.hdr.size = sizeof(struct sof_ipc_comp_config);

	/* Get control into ctl and priv_data */
	for (i = 0; i < ctx->widget->num_kcontrols; i++) {
		ret = tplg_get_single_control(ctx, &ctl, &priv_data);
		if (ret < 0) {
			fprintf(stderr, "error: failed control load\n");
			goto err;
		}

		/* call ctl creation callback if needed */
		if (ctx->ctl_cb)
			ctx->ctl_cb(ctl, volume, ctx->ctl_arg);

		/* we only care about the volume ctl - ignore others atm */
		if (ctl->ops.get != 256)
			continue;

		/* Get volume scale */
		mixer_ctl = (struct snd_soc_tplg_mixer_control *)ctl;
		vol_min = (int32_t)mixer_ctl->hdr.tlv.scale.min;
		vol_step = mixer_ctl->hdr.tlv.scale.step;
		vol_maxs = mixer_ctl->max;
		channels = mixer_ctl->num_channels;

		vol_min_db = 0.01 * vol_min;
		vol_max_db = 0.01 * (vol_maxs * vol_step) + vol_min_db;
		volume->min_value = round(pow(10, vol_min_db / 20.0) * 65535);
		volume->max_value = round(pow(10, vol_max_db / 20.0) * 65536);
		volume->channels = channels;
	}

err:
	return ret;
}

/* Peak Volume - IPC4 */
static const struct sof_topology_token pga4_tokens[] = {
	{SOF_TKN_GAIN_VAL,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, tplg_token_get_uint32_t,
		offsetof(struct ipc4_peak_volume_config, target_volume), 0},
	{SOF_TKN_GAIN_RAMP_TYPE, SND_SOC_TPLG_TUPLE_TYPE_WORD,
		tplg_token_get_uint32_t,
		offsetof(struct ipc4_peak_volume_config, curve_type), 0},
	{SOF_TKN_GAIN_RAMP_DURATION,
		SND_SOC_TPLG_TUPLE_TYPE_WORD, tplg_token_get_uint32_t,
		offsetof(struct ipc4_peak_volume_config, curve_duration), 0},
};

static const struct sof_topology_token_group pga_ipc4_tokens[] = {
	{pga4_tokens, ARRAY_SIZE(pga4_tokens)},
};

static int pga_ipc4_build(struct tplg_context *ctx, void *_pga)
{
	struct ipc4_peak_volume_config *volume = _pga;

	volume->channel_id = IPC4_ALL_CHANNELS_MASK;

	tplg_debug("volume channel ID: %d, target_volume %#x, curve_type: %d curve_duration: %ld\n",
		   volume->channel_id, volume->target_volume, volume->curve_type,
		   volume->curve_duration);

	return tplg_parse_widget_audio_formats(ctx);
}

static const struct sof_topology_module_desc pga_ipc[] = {
	{3, pga_ipc3_tokens, ARRAY_SIZE(pga_ipc3_tokens),
		pga_ipc3_build, sizeof(struct sof_ipc_comp_volume) + UUID_SIZE},
	{4, pga_ipc4_tokens, ARRAY_SIZE(pga_ipc4_tokens), pga_ipc4_build},
};

/* load pda dapm widget */
int tplg_new_pga(struct tplg_context *ctx, void *pga,
		 size_t pga_size, struct snd_soc_tplg_ctl_hdr *rctl,
		 size_t ctl_size)
{
	int ret;

	ret = tplg_create_object(ctx, pga_ipc, ARRAY_SIZE(pga_ipc),
				 "pga", pga, pga_size);
	return ret;
}
