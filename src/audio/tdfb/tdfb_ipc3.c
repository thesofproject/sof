// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//
// Author: Baofeng Tian <baofeng.tian@intel.com>

/* this file contains ipc3 specific functions for tdfb.
 */

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <rtos/alloc.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <ipc/header.h>
#include <ipc/control.h>
#include <sof/ipc/msg.h>
#include <errno.h>
#include <sof/audio/data_blob.h>
#include <sof/trace/trace.h>
#include <stdint.h>

#include "tdfb.h"
#include "tdfb_comp.h"

LOG_MODULE_DECLARE(tdfb, CONFIG_SOF_LOG_LEVEL);

static int init_get_ctl_ipc(struct processing_module *mod)
{
	struct tdfb_comp_data *cd = module_get_private_data(mod);
	int comp_id = dev_comp_id(mod->dev);

	cd->ctrl_data = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, TDFB_GET_CTRL_DATA_SIZE);
	if (!cd->ctrl_data)
		return -ENOMEM;

	cd->ctrl_data->rhdr.hdr.cmd = SOF_IPC_GLB_COMP_MSG | SOF_IPC_COMP_GET_VALUE | comp_id;
	cd->ctrl_data->rhdr.hdr.size = TDFB_GET_CTRL_DATA_SIZE;
	cd->msg = ipc_msg_init(cd->ctrl_data->rhdr.hdr.cmd, cd->ctrl_data->rhdr.hdr.size);

	cd->ctrl_data->comp_id = comp_id;
	cd->ctrl_data->type = SOF_CTRL_TYPE_VALUE_CHAN_GET;
	cd->ctrl_data->cmd = SOF_CTRL_CMD_ENUM;
	cd->ctrl_data->index = SOF_TDFB_CTRL_INDEX_AZIMUTH_ESTIMATE;
	cd->ctrl_data->num_elems = 0;
	return 0;
}

static void send_get_ctl_ipc(struct processing_module *mod)
{
	struct tdfb_comp_data *cd = module_get_private_data(mod);

#if TDFB_ADD_DIRECTION_TO_GET_CMD
	cd->ctrl_data->chanv[0].channel = 0;
	cd->ctrl_data->chanv[0].value = cd->az_value_estimate;
	cd->ctrl_data->num_elems = 1;
#endif

	ipc_msg_send(cd->msg, cd->ctrl_data, false);
}

int tdfb_ipc_notification_init(struct processing_module *mod)
{
	return init_get_ctl_ipc(mod);
}

void tdfb_send_ipc_notification(struct processing_module *mod)
{
	send_get_ctl_ipc(mod);
}

/*
 * Module commands handling
 */
static int tdfb_cmd_switch_get(struct sof_ipc_ctrl_data *cdata, struct tdfb_comp_data *cd)
{
	int j;

	/* Fail if wrong index in control, needed if several in same type */
	if (cdata->index != SOF_TDFB_CTRL_INDEX_PROCESS)
		return -EINVAL;

	for (j = 0; j < cdata->num_elems; j++)
		cdata->chanv[j].value = cd->beam_on;

	return 0;
}

static int tdfb_cmd_enum_get(struct sof_ipc_ctrl_data *cdata, struct tdfb_comp_data *cd)
{
	int j;

	switch (cdata->index) {
	case SOF_TDFB_CTRL_INDEX_AZIMUTH:
		for (j = 0; j < cdata->num_elems; j++)
			cdata->chanv[j].value = cd->az_value;

		break;
	case SOF_TDFB_CTRL_INDEX_AZIMUTH_ESTIMATE:
		for (j = 0; j < cdata->num_elems; j++)
			cdata->chanv[j].value = cd->az_value_estimate;

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tdfb_cmd_get_value(struct processing_module *mod, struct sof_ipc_ctrl_data *cdata)
{
	struct tdfb_comp_data *cd = module_get_private_data(mod);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		comp_dbg(mod->dev, "tdfb_cmd_get_value(), SOF_CTRL_CMD_ENUM index=%d",
			 cdata->index);
		return tdfb_cmd_enum_get(cdata, cd);
	case SOF_CTRL_CMD_SWITCH:
		comp_dbg(mod->dev, "tdfb_cmd_get_value(), SOF_CTRL_CMD_SWITCH index=%d",
			 cdata->index);
		return tdfb_cmd_switch_get(cdata, cd);
	}

	comp_err(mod->dev, "tdfb_cmd_get_value() error: invalid cdata->cmd");
	return -EINVAL;
}

int tdfb_get_ipc_config(struct processing_module *mod,
			uint32_t param_id, uint32_t *data_offset_size,
			uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct tdfb_comp_data *cd = module_get_private_data(mod);

	if (cdata->cmd != SOF_CTRL_CMD_BINARY)
		return tdfb_cmd_get_value(mod, cdata);

	comp_dbg(mod->dev, "tdfb_get_ipc_config(), binary");
	return comp_data_blob_get_cmd(cd->model_handler, cdata, fragment_size);
}

static int tdfb_cmd_enum_set(struct sof_ipc_ctrl_data *cdata, struct tdfb_comp_data *cd)
{
	if (cdata->num_elems != 1)
		return -EINVAL;

	if (cdata->chanv[0].value > SOF_TDFB_MAX_ANGLES)
		return -EINVAL;

	switch (cdata->index) {
	case SOF_TDFB_CTRL_INDEX_AZIMUTH:
		cd->az_value = cdata->chanv[0].value;
		cd->update = true;
		break;
	case SOF_TDFB_CTRL_INDEX_AZIMUTH_ESTIMATE:
		cd->az_value_estimate = cdata->chanv[0].value;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tdfb_cmd_switch_set(struct sof_ipc_ctrl_data *cdata, struct tdfb_comp_data *cd)
{
	if (cdata->num_elems != 1)
		return -EINVAL;

	switch (cdata->index) {
	case SOF_TDFB_CTRL_INDEX_PROCESS:
		cd->beam_on = cdata->chanv[0].value;
		cd->update = true;
		break;
	case SOF_TDFB_CTRL_INDEX_DIRECTION:
		cd->direction_updates = cdata->chanv[0].value;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tdfb_cmd_set_value(struct processing_module *mod, struct sof_ipc_ctrl_data *cdata)
{
	struct tdfb_comp_data *cd = module_get_private_data(mod);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		comp_dbg(mod->dev, "tdfb_cmd_set_value(), SOF_CTRL_CMD_ENUM index=%d",
			 cdata->index);
		return tdfb_cmd_enum_set(cdata, cd);
	case SOF_CTRL_CMD_SWITCH:
		comp_dbg(mod->dev, "tdfb_cmd_set_value(), SOF_CTRL_CMD_SWITCH index=%d",
			 cdata->index);
		return tdfb_cmd_switch_set(cdata, cd);
	}

	comp_err(mod->dev, "tdfb_cmd_set_value() error: invalid cdata->cmd");
	return -EINVAL;
}

int tdfb_set_ipc_config(struct processing_module *mod, uint32_t param_id,
			enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			size_t response_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct tdfb_comp_data *cd = module_get_private_data(mod);
	int ret;

	if (cdata->cmd != SOF_CTRL_CMD_BINARY) {
		ret = tdfb_cmd_set_value(mod, cdata);
	} else {
		comp_info(mod->dev, "tdfb_set_ipc_config(), binary");
		ret = comp_data_blob_set(cd->model_handler, pos, data_offset_size,
					 fragment, fragment_size);
	}

	return ret;
}

int tdfb_params(struct processing_module *mod)
{
	return 0;
}

