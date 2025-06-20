// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include "sound_dose.h"

LOG_MODULE_DECLARE(sound_dose, CONFIG_SOF_LOG_LEVEL);
/* IPC4 controls handler */
__cold int sound_dose_set_config(struct processing_module *mod,
				 uint32_t param_id,
				 enum module_cfg_fragment_position pos,
				 uint32_t data_offset_size,
				 const uint8_t *fragment,
				 size_t fragment_size,
				 uint8_t *response,
				 size_t response_size)
{
	struct sound_dose_comp_data *cd = module_get_private_data(mod);
	struct sound_dose_setup_config *new_setup;
	struct sound_dose_volume_config *new_volume;
	struct sound_dose_gain_config *new_gain;
	struct comp_dev *dev = mod->dev;
	uint8_t *dest;
	size_t dest_size;

	assert_can_be_cold();
	comp_info(dev, "sound_dose_set_config()");

	switch (param_id) {
	case SOF_SOUND_DOSE_SETUP_PARAM_ID:
		new_setup = (struct sound_dose_setup_config *)fragment;
		if (new_setup->sens_dbfs_dbspl < SOF_SOUND_DOSE_SENS_MIN_DB ||
		    new_setup->sens_dbfs_dbspl > SOF_SOUND_DOSE_SENS_MAX_DB) {
			comp_err(dev, "Illegal sensitivity = %d", new_setup->sens_dbfs_dbspl);
			return -EINVAL;
		}
		dest_size = sizeof(struct sound_dose_setup_config);
		dest = (uint8_t *)&cd->setup;
		break;
	case SOF_SOUND_DOSE_VOLUME_PARAM_ID:
		new_volume = (struct sound_dose_volume_config *)fragment;
		if (new_volume->volume_offset < SOF_SOUND_DOSE_VOLUME_MIN_DB ||
		    new_volume->volume_offset > SOF_SOUND_DOSE_VOLUME_MIN_DB) {
			comp_err(dev, "Illegal volume = %d", new_volume->volume_offset);
			return -EINVAL;
		}
		dest_size = sizeof(struct sound_dose_volume_config);
		dest = (uint8_t *)&cd->vol;
		break;
	case SOF_SOUND_DOSE_GAIN_PARAM_ID:
		new_gain = (struct sound_dose_gain_config *)fragment;
		if (new_gain->gain < SOF_SOUND_DOSE_GAIN_MIN_DB ||
		    new_gain->gain > SOF_SOUND_DOSE_GAIN_MIN_DB) {
			comp_err(dev, "Illegal gain = %d", new_gain->gain);
			return -EINVAL;
		}
		dest_size = sizeof(struct sound_dose_gain_config);
		dest = (uint8_t *)&cd->att;
		break;
	default:
		comp_err(dev, "Illegal param_id = %d", param_id);
		return -EINVAL;
	}

	if (fragment_size != dest_size) {
		comp_err(dev, "Illegal fragment_size = %d", fragment_size);
		return -EINVAL;
	}
	memcpy_s(dest, dest_size, fragment, fragment_size);
	return 0;
}

/* Not used in IPC4 systems, if IPC4 only component, omit .get_configuration set */
__cold int sound_dose_get_config(struct processing_module *mod,
				 uint32_t config_id, uint32_t *data_offset_size,
				 uint8_t *fragment, size_t fragment_size)
{
	assert_can_be_cold();
	return 0;
}
