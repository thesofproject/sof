// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include "level_multiplier.h"

LOG_MODULE_DECLARE(level_multiplier, CONFIG_SOF_LOG_LEVEL);

/* IPC4 controls handler */
__cold int level_multiplier_set_config(struct processing_module *mod,
				       uint32_t param_id,
				       enum module_cfg_fragment_position pos,
				       uint32_t data_offset_size,
				       const uint8_t *fragment,
				       size_t fragment_size,
				       uint8_t *response,
				       size_t response_size)
{
	struct level_multiplier_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	assert_can_be_cold();

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID:
	case SOF_IPC4_ENUM_CONTROL_PARAM_ID:
		comp_err(dev, "Illegal control param_id %d.", param_id);
		return -EINVAL;
	}

	if (fragment_size != sizeof(int32_t)) {
		comp_err(dev, "Illegal fragment size %d.", fragment_size);
		return -EINVAL;
	}

	memcpy_s(&cd->gain, sizeof(int32_t), fragment, sizeof(int32_t));
	comp_dbg(mod->dev, "Gain set to %d", cd->gain);
	return 0;
}
