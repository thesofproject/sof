// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Google LLC. All rights reserved.
//
// Author: Pin-chih Lin <johnylin@google.com>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/module_adapter/module/module_interface.h>
#include <module/module/base.h>
#include <ipc/control.h>
#include <sof/trace/trace.h>
#include <sof/audio/component.h>
#include <module/module/interface.h>
#include <sof/audio/data_blob.h>

#include "multiband_drc.h"

LOG_MODULE_DECLARE(multiband_drc, CONFIG_SOF_LOG_LEVEL);

void multiband_drc_process_enable(bool *process_enabled)
{
	*process_enabled = false;
}

static int multiband_drc_cmd_set_value(struct processing_module *mod,
				       struct sof_ipc_ctrl_data *cdata)
{
	struct comp_dev *dev = mod->dev;
	struct multiband_drc_comp_data *cd = module_get_private_data(mod);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_SWITCH:
		comp_dbg(dev, "multiband_drc_SOF_CTRL_CMD_SWITCH");
		if (cdata->num_elems == 1) {
			cd->process_enabled = cdata->chanv[0].value;
			comp_info(dev, "process_enabled = %d",
				  cd->process_enabled);
			return 0;
		}
	}

	comp_err(mod->dev, "cmd_set_value() error: invalid cdata->cmd");
	return -EINVAL;
}

int multiband_drc_set_ipc_config(struct processing_module *mod, uint32_t param_id,
				 const uint8_t *fragment, enum module_cfg_fragment_position pos,
				 uint32_t data_offset_size, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct multiband_drc_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	if (cdata->cmd != SOF_CTRL_CMD_BINARY)
		return multiband_drc_cmd_set_value(mod, cdata);

	comp_dbg(dev, "multiband_drc_set_config(), SOF_CTRL_CMD_BINARY");
	return comp_data_blob_set(cd->model_handler, pos, data_offset_size, fragment,
				  fragment_size);
}

static int multiband_drc_cmd_get_value(struct processing_module *mod,
				       struct sof_ipc_ctrl_data *cdata)
{
	struct comp_dev *dev = mod->dev;
	struct multiband_drc_comp_data *cd = module_get_private_data(mod);
	int j;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_SWITCH:
		comp_dbg(dev, "SOF_CTRL_CMD_SWITCH");
		for (j = 0; j < cdata->num_elems; j++)
			cdata->chanv[j].value = cd->process_enabled;
		if (cdata->num_elems == 1)
			return 0;

		comp_warn(dev, "warn: num_elems should be 1, got %d",
			  cdata->num_elems);
		return 0;
	}

	comp_err(dev, "tdfb_cmd_get_value() error: invalid cdata->cmd");
	return -EINVAL;
}

int multiband_drc_get_ipc_config(struct processing_module *mod, struct sof_ipc_ctrl_data *cdata,
				 size_t fragment_size)
{
	struct multiband_drc_comp_data *cd = module_get_private_data(mod);

	if (cdata->cmd != SOF_CTRL_CMD_BINARY)
		return multiband_drc_cmd_get_value(mod, cdata);

	comp_dbg(mod->dev, "SOF_CTRL_CMD_BINARY");
	return comp_data_blob_get_cmd(cd->model_handler, cdata, fragment_size);
}

int multiband_drc_params(struct processing_module *mod)
{
	return 0;
}

