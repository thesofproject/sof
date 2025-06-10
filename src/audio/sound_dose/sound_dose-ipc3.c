// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include "sound_dose.h"

LOG_MODULE_DECLARE(sound_dose, CONFIG_SOF_LOG_LEVEL);

/* This function handles the real-time controls. The ALSA controls have the
 * param_id set to indicate the control type. The control ID, from topology,
 * is used to separate the controls instances of same type. In control payload
 * the num_elems defines to how many channels the control is applied to.
 */
__cold int sound_dose_set_config(struct processing_module *mod, uint32_t param_id,
				 enum module_cfg_fragment_position pos,
				 uint32_t data_offset_size, const uint8_t *fragment,
				 size_t fragment_size, uint8_t *response,
				 size_t response_size)
{
	assert_can_be_cold();
	return 0;
}

__cold int sound_dose_get_config(struct processing_module *mod,
				 uint32_t config_id, uint32_t *data_offset_size,
				 uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct sound_dose_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	assert_can_be_cold();
	return 0;
}
