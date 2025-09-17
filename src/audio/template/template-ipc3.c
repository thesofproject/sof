// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include "template.h"

LOG_MODULE_DECLARE(template, CONFIG_SOF_LOG_LEVEL);

/* This function handles the real-time controls. The ALSA controls have the
 * param_id set to indicate the control type. The control ID, from topology,
 * is used to separate the controls instances of same type. In control payload
 * the num_elems defines to how many channels the control is applied to.
 */
__cold int template_set_config(struct processing_module *mod, uint32_t param_id,
			       enum module_cfg_fragment_position pos,
			       uint32_t data_offset_size, const uint8_t *fragment,
			       size_t fragment_size, uint8_t *response,
			       size_t response_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct template_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	assert_can_be_cold();

	comp_dbg(dev, "template_set_config()");

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_SWITCH:
		if (cdata->index != 0) {
			comp_err(dev, "Illegal switch control index = %d.", cdata->index);
			return -EINVAL;
		}

		if (cdata->num_elems != 1) {
			comp_err(dev, "Illegal switch control num_elems = %d.", cdata->num_elems);
			return -EINVAL;
		}

		cd->enable = cdata->chanv[0].value;
		comp_info(dev, "Setting enable = %d.", cd->enable);
		return 0;

	case SOF_CTRL_CMD_ENUM:
		comp_err(dev, "Illegal enum control, no support in this component.");
		return -EINVAL;
	case SOF_CTRL_CMD_BINARY:
		comp_err(dev, "Illegal bytes control, no support in this component.");
		return -EINVAL;
	}

	comp_err(dev, "Illegal control, unknown type.");
	return -EINVAL;
}

__cold int template_get_config(struct processing_module *mod,
			       uint32_t config_id, uint32_t *data_offset_size,
			       uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct template_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	assert_can_be_cold();

	comp_info(dev, "template_get_config()");

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_SWITCH:
		if (cdata->index != 0) {
			comp_err(dev, "Illegal switch control index = %d.", cdata->index);
			return -EINVAL;
		}

		if (cdata->num_elems != 1) {
			comp_err(dev, "Illegal switch control num_elems = %d.", cdata->num_elems);
			return -EINVAL;
		}

		cdata->chanv[0].value = cd->enable;
		return 0;

	case SOF_CTRL_CMD_ENUM:
		comp_err(dev, "Illegal enum control, no support in this component.");
		return -EINVAL;

	case SOF_CTRL_CMD_BINARY:
		comp_err(dev, "Illegal bytes control, no support in this component.");
		return -EINVAL;
	}

	comp_err(dev, "Illegal control, unknown type.");
	return -EINVAL;
}
