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
	assert_can_be_cold();
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
