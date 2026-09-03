// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include "template.h"

LOG_MODULE_DECLARE(template, CONFIG_SOF_LOG_LEVEL);

/* IPC4 controls handler */
__cold int template_set_config(struct processing_module *mod,
			       uint32_t param_id,
			       enum module_cfg_fragment_position pos,
			       uint32_t data_offset_size,
			       const uint8_t *fragment,
			       size_t fragment_size,
			       uint8_t *response,
			       size_t response_size)
{
	struct sof_ipc4_control_msg_payload *ctl = (struct sof_ipc4_control_msg_payload *)fragment;
	struct template_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	assert_can_be_cold();

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID:
		comp_dbg(dev, "Switch control id = %d, num_elems = %d.", ctl->id, ctl->num_elems);
		if (ctl->id != 0) {
			comp_err(dev, "Illegal switch control id = %d.", ctl->id);
			return -EINVAL;
		}

		if (ctl->num_elems != 1) {
			comp_err(dev, "Illegal switch control num_elems = %d.", ctl->num_elems);
			return -EINVAL;
		}

		cd->enable = ctl->chanv[0].value;
		comp_info(dev, "Setting enable = %d.", cd->enable);
		return 0;

	case SOF_IPC4_ENUM_CONTROL_PARAM_ID:
		comp_err(dev, "Illegal enum control, no support in this component.");
		return -EINVAL;
	}

	comp_err(mod->dev, "Illegal bytes control, no support in this component.");
	return -EINVAL;
}

/* Not used in IPC4 systems, if IPC4 only component, omit .get_configuration set */
__cold int template_get_config(struct processing_module *mod,
			       uint32_t config_id, uint32_t *data_offset_size,
			       uint8_t *fragment, size_t fragment_size)
{
	assert_can_be_cold();
	return 0;
}
