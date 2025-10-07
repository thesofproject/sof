// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include "stft_process.h"

LOG_MODULE_DECLARE(stft_process, CONFIG_SOF_LOG_LEVEL);

/* IPC4 controls handler */
__cold int stft_process_set_config(struct processing_module *mod, uint32_t param_id,
				   enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				   const uint8_t *fragment, size_t fragment_size, uint8_t *response,
				   size_t response_size)
{
	struct stft_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	assert_can_be_cold();

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID:
	case SOF_IPC4_ENUM_CONTROL_PARAM_ID:
		comp_err(dev, "Illegal control param_id %d.", param_id);
		return -EINVAL;
	}

	if (fragment_size != sizeof(struct sof_stft_process_config)) {
		comp_err(dev, "Illegal fragment size %d, expect %d.", fragment_size,
			 sizeof(struct sof_stft_process_config));
		return -EINVAL;
	}

	if (!cd->config) {
		cd->config = mod_alloc(mod, sizeof(struct sof_stft_process_config));
		if (!cd->config) {
			comp_err(dev, "Failed to allocate configuration.");
			return -ENOMEM;
		}
	}

	memcpy_s(cd->config, sizeof(struct sof_stft_process_config), fragment, fragment_size);
	return 0;
}

/* Not used in IPC4 systems, if IPC4 only component, omit .get_configuration set */
__cold int stft_process_get_config(struct processing_module *mod, uint32_t config_id,
				   uint32_t *data_offset_size, uint8_t *fragment,
				   size_t fragment_size)
{
	assert_can_be_cold();
	return 0;
}
