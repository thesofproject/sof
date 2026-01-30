// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include "phase_vocoder.h"

LOG_MODULE_DECLARE(phase_vocoder, CONFIG_SOF_LOG_LEVEL);

/* IPC4 controls handler */
__cold int phase_vocoder_set_config(struct processing_module *mod, uint32_t param_id,
				    enum module_cfg_fragment_position pos,
				    uint32_t data_offset_size, const uint8_t *fragment,
				    size_t fragment_size, uint8_t *response, size_t response_size)
{
	struct sof_ipc4_control_msg_payload *ctl = (struct sof_ipc4_control_msg_payload *)fragment;
	struct phase_vocoder_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	assert_can_be_cold();

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID:
		if (ctl->id != 0) {
			comp_err(dev, "Illegal switch control id = %d.", ctl->id);
			return -EINVAL;
		}

		if (ctl->num_elems != 1) {
			comp_err(dev, "Illegal switch control num_elems = %d.", ctl->num_elems);
			return -EINVAL;
		}

		cd->enable = ctl->chanv[0].value;
		comp_info(dev, "enable = %d.", cd->enable);
		return 0;

	case SOF_IPC4_ENUM_CONTROL_PARAM_ID:
		if (ctl->id != 0) {
			comp_err(dev, "Illegal enum control id = %d.", ctl->id);
			return -EINVAL;
		}

		if (ctl->num_elems != 1) {
			comp_err(dev, "Illegal enum control num_elems = %d.", ctl->num_elems);
			return -EINVAL;
		}

		if (ctl->chanv[0].value < 0 || ctl->chanv[0].value > 15) {
			comp_err(dev, "Illegal enum control value = %d.", ctl->chanv[0].value);
			return -EINVAL;
		}

		cd->speed_enum = ctl->chanv[0].value;
		cd->speed_ctrl = PHASE_VOCODER_MIN_SPEED_Q29 +
			    Q_MULTSR_32X32((int64_t)cd->speed_enum, PHASE_VOCODER_SPEED_STEP_Q31, 0,
					   31, 29);

		comp_info(dev, "speed_enum = %d, speed = %d", cd->speed_enum, cd->speed_ctrl);
		return 0;
	}

	if (fragment_size != sizeof(struct sof_phase_vocoder_config)) {
		comp_err(dev, "Illegal fragment size %d, expect %d.", fragment_size,
			 sizeof(struct sof_phase_vocoder_config));
		return -EINVAL;
	}

	if (!cd->config) {
		cd->config = mod_alloc(mod, sizeof(struct sof_phase_vocoder_config));
		if (!cd->config) {
			comp_err(dev, "Failed to allocate configuration.");
			return -ENOMEM;
		}
	}

	memcpy_s(cd->config, sizeof(struct sof_phase_vocoder_config), fragment, fragment_size);
	return 0;
}

/* Not used in IPC4 systems, if IPC4 only component, omit .get_configuration set */
__cold int phase_vocoder_get_config(struct processing_module *mod, uint32_t config_id,
				    uint32_t *data_offset_size, uint8_t *fragment,
				    size_t fragment_size)
{
	assert_can_be_cold();
	return 0;
}
