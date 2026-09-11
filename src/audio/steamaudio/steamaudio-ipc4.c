// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Steam Audio IPC4 Control Handler

#include "steamaudio.h"
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <rtos/string.h>

LOG_MODULE_DECLARE(steamaudio, CONFIG_SOF_LOG_LEVEL);

__cold int steamaudio_set_config(struct processing_module *mod,
				 uint32_t param_id,
				 enum module_cfg_fragment_position pos,
				 uint32_t data_offset_size,
				 const uint8_t *fragment,
				 size_t fragment_size,
				 uint8_t *response,
				 size_t response_size)
{
	struct steamaudio_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	assert_can_be_cold();

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID: {
		struct sof_ipc4_control_msg_payload *ctl = (struct sof_ipc4_control_msg_payload *)fragment;
		if (ctl->num_elems != 1)
			return -EINVAL;
		cd->enable = (ctl->chanv[0].value != 0);
		comp_info(dev, "steamaudio: enable set to %d", cd->enable);
		return 0;
	}

	case STEAMAUDIO_PARAM_DIRECT_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_direct_config))
			return -EINVAL;

		const struct sof_steamaudio_direct_config *cfg =
			(const struct sof_steamaudio_direct_config *)fragment;

		cd->direct.target_gain = cfg->distance_attenuation * (1.0f - cfg->occlusion);
		cd->direct.gain_step = (cd->direct.target_gain - cd->direct.current_gain) / 128.0f;
		comp_dbg(dev, "steamaudio: direct dist=%f, occ=%f",
			 (double)cfg->distance_attenuation, (double)cfg->occlusion);
		return 0;
	}

	case STEAMAUDIO_PARAM_BINAURAL_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_binaural_config))
			return -EINVAL;

		const struct sof_steamaudio_binaural_config *cfg =
			(const struct sof_steamaudio_binaural_config *)fragment;

		cd->binaural.direction[0] = cfg->direction[0];
		cd->binaural.direction[1] = cfg->direction[1];
		cd->binaural.direction[2] = cfg->direction[2];
		cd->binaural.spatial_blend = cfg->spatial_blend;

		comp_dbg(dev, "steamaudio: binaural dir=(%f, %f, %f)",
			 (double)cfg->direction[0], (double)cfg->direction[1], (double)cfg->direction[2]);
		return 0;
	}

	case STEAMAUDIO_PARAM_REVERB_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_reverb_config))
			return -EINVAL;

		const struct sof_steamaudio_reverb_config *cfg =
			(const struct sof_steamaudio_reverb_config *)fragment;

		cd->reverb.wet_gain = cfg->wet_gain;
		comp_dbg(dev, "steamaudio: reverb wet=%f", (double)cfg->wet_gain);
		return 0;
	}

	case STEAMAUDIO_PARAM_AMBISONICS_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_ambisonics_config))
			return -EINVAL;

		const struct sof_steamaudio_ambisonics_config *cfg =
			(const struct sof_steamaudio_ambisonics_config *)fragment;

		cd->ambisonics.order = cfg->order;
		memcpy(cd->ambisonics.rotation, cfg->listener_rotation, sizeof(cd->ambisonics.rotation));
		comp_dbg(dev, "steamaudio: ambisonics order=%d", cfg->order);
		return 0;
	}

	case STEAMAUDIO_PARAM_BITSTREAM_MODE: {
		if (fragment_size < sizeof(uint32_t))
			return -EINVAL;
		cd->bitstream_mode = (*((const uint32_t *)fragment) != 0);
		comp_info(dev, "steamaudio: bitstream mode set to %d", cd->bitstream_mode);
		return 0;
	}

	default:
		comp_warn(dev, "steamaudio: unhandled param_id 0x%x", param_id);
		return -EINVAL;
	}
}

__cold int steamaudio_get_config(struct processing_module *mod,
				 uint32_t config_id, uint32_t *data_offset_size,
				 uint8_t *fragment, size_t fragment_size)
{
	struct steamaudio_comp_data *cd = module_get_private_data(mod);

	assert_can_be_cold();

	switch (config_id) {
	case STEAMAUDIO_PARAM_DIRECT_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_direct_config))
			return -EINVAL;

		struct sof_steamaudio_direct_config *cfg = (struct sof_steamaudio_direct_config *)fragment;
		cfg->comp_type = STEAMAUDIO_PARAM_DIRECT_CONFIG;
		cfg->distance_attenuation = cd->direct.current_gain;
		cfg->occlusion = 0.0f;
		return 0;
	}

	case STEAMAUDIO_PARAM_BINAURAL_CONFIG: {
		if (fragment_size < sizeof(struct sof_steamaudio_binaural_config))
			return -EINVAL;

		struct sof_steamaudio_binaural_config *cfg = (struct sof_steamaudio_binaural_config *)fragment;
		cfg->comp_type = STEAMAUDIO_PARAM_BINAURAL_CONFIG;
		cfg->direction[0] = cd->binaural.direction[0];
		cfg->direction[1] = cd->binaural.direction[1];
		cfg->direction[2] = cd->binaural.direction[2];
		cfg->spatial_blend = cd->binaural.spatial_blend;
		return 0;
	}

	default:
		return -EINVAL;
	}
}
