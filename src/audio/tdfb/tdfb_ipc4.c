// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//
// Author: Baofeng Tian <baofeng.tian@intel.com>

/* this file contains ipc4 specific functions for tdfb.
 */

#include <sof/audio/module_adapter/module/module_interface.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/trace/trace.h>
#include <sof/ipc/msg.h>
#include <errno.h>
#include <sof/audio/component.h>
#include <ipc4/notification.h>
#include <ipc4/module.h>
#include <ipc4/header.h>
#include <ipc4/base-config.h>
#include <rtos/string.h>
#include <ipc/stream.h>
#include <sof/audio/buffer.h>
#include <sof/audio/data_blob.h>
#include <sof/list.h>
#include <stdint.h>

#include "tdfb.h"
#include "tdfb_comp.h"

struct tdfb_notification_payload {
	struct sof_ipc4_notify_module_data module_data;
	struct sof_ipc4_control_msg_payload control_msg;
	struct sof_ipc4_ctrl_value_chan control_value; /* One channel value */
};

LOG_MODULE_DECLARE(tdfb, CONFIG_SOF_LOG_LEVEL);

static struct ipc_msg *tdfb_notification_init(struct processing_module *mod,
					      uint32_t control_type_param_id,
					      uint32_t control_id)
{
	struct ipc_msg msg_proto;
	struct comp_dev *dev = mod->dev;
	struct comp_ipc_config *ipc_config = &dev->ipc_config;
	union ipc4_notification_header *primary =
		(union ipc4_notification_header *)&msg_proto.header;
	struct ipc_msg *msg;
	struct tdfb_notification_payload *payload;

	/* Clear header, extension, and other ipc_msg members */
	memset_s(&msg_proto, sizeof(msg_proto), 0, sizeof(msg_proto));
	primary->r.notif_type = SOF_IPC4_MODULE_NOTIFICATION;
	primary->r.type = SOF_IPC4_GLB_NOTIFICATION;
	primary->r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	primary->r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
	msg = ipc_msg_w_ext_init(msg_proto.header, msg_proto.extension,
				 sizeof(struct tdfb_notification_payload));
	if (!msg)
		return NULL;

	payload = (struct tdfb_notification_payload *)msg->tx_data;
	payload->module_data.instance_id = IPC4_INST_ID(ipc_config->id);
	payload->module_data.module_id = IPC4_MOD_ID(ipc_config->id);
	payload->module_data.event_id = SOF_IPC4_NOTIFY_MODULE_EVENTID_ALSA_MAGIC_VAL |
		control_type_param_id;
	payload->module_data.event_data_size = sizeof(struct sof_ipc4_control_msg_payload) +
		sizeof(struct sof_ipc4_ctrl_value_chan);
	payload->control_msg.id = control_id;
	payload->control_msg.num_elems = 1;
	payload->control_value.channel = 0;

	comp_dbg(dev, "instance_id = 0x%08x, module_id = 0x%08x",
		 payload->module_data.instance_id, payload->module_data.module_id);
	return msg;
}

static void tdfb_send_notification(struct ipc_msg *msg, uint32_t val)
{
	struct tdfb_notification_payload *ipc_payload;

	ipc_payload = (struct tdfb_notification_payload *)msg->tx_data;
	ipc_payload->control_value.value = val;
	ipc_msg_send(msg, NULL, false);
}

int tdfb_ipc_notification_init(struct processing_module *mod)
{
	struct tdfb_comp_data *cd = module_get_private_data(mod);

	cd->msg = tdfb_notification_init(mod, SOF_IPC4_ENUM_CONTROL_PARAM_ID,
					 SOF_TDFB_CTRL_INDEX_AZIMUTH_ESTIMATE);
	if (!cd->msg) {
		comp_err(mod->dev, "Failed to initialize control notification.");
		return -EINVAL;
	}

	return 0;
}

void tdfb_send_ipc_notification(struct processing_module *mod)
{
	struct tdfb_comp_data *cd = module_get_private_data(mod);

	tdfb_send_notification(cd->msg, cd->az_value_estimate);
}

int tdfb_get_ipc_config(struct processing_module *mod,
			uint32_t param_id, uint32_t *data_offset_size,
			uint8_t *fragment, size_t fragment_size)
{
	comp_err(mod->dev, "tdfb_get_ipc_config, Not supported, should not happen");
	return -EINVAL;
}

static int tdfb_cmd_enum_set(struct sof_ipc4_control_msg_payload *ctl, struct tdfb_comp_data *cd)
{
	if (ctl->num_elems != 1)
		return -EINVAL;

	if (ctl->chanv[0].value > SOF_TDFB_MAX_ANGLES)
		return -EINVAL;

	switch (ctl->id) {
	case SOF_TDFB_CTRL_INDEX_AZIMUTH:
		cd->az_value = ctl->chanv[0].value;
		cd->update = true;
		break;
	case SOF_TDFB_CTRL_INDEX_AZIMUTH_ESTIMATE:
		cd->az_value_estimate = ctl->chanv[0].value;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tdfb_cmd_switch_set(struct sof_ipc4_control_msg_payload *ctl, struct tdfb_comp_data *cd)
{
	if (ctl->num_elems != 1)
		return -EINVAL;

	switch (ctl->id) {
	case SOF_TDFB_CTRL_INDEX_PROCESS:
		cd->beam_on = ctl->chanv[0].value;
		cd->update = true;
		break;
	case SOF_TDFB_CTRL_INDEX_DIRECTION:
		cd->direction_updates = ctl->chanv[0].value;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int tdfb_set_ipc_config(struct processing_module *mod, uint32_t param_id,
			enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			size_t response_size)
{
	struct sof_ipc4_control_msg_payload *ctl = (struct sof_ipc4_control_msg_payload *)fragment;
	struct tdfb_comp_data *cd = module_get_private_data(mod);

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID:
		comp_dbg(mod->dev, "SOF_IPC4_SWITCH_CONTROL_PARAM_ID id = %d, num_elems = %d",
			 ctl->id, ctl->num_elems);
		return tdfb_cmd_switch_set(ctl, cd);

	case SOF_IPC4_ENUM_CONTROL_PARAM_ID:
		comp_dbg(mod->dev, "SOF_IPC4_ENUM_CONTROL_PARAM_ID id = %d, num_elems = %d",
			 ctl->id, ctl->num_elems);
		return tdfb_cmd_enum_set(ctl, cd);
	default:
		comp_info(mod->dev, "tdfb_set_ipc_config(), binary");
		return comp_data_blob_set(cd->model_handler, pos, data_offset_size,
					  fragment, fragment_size);
	}
}

int tdfb_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_buffer *sinkb, *sourceb;
	struct comp_dev *dev = mod->dev;

	if (mod->priv.cfg.nb_input_pins != SOF_TDFB_NUM_INPUT_PINS) {
		comp_err(dev, "Illegal input pins count %d", mod->priv.cfg.nb_input_pins);
		return -EINVAL;
	}

	if (mod->priv.cfg.nb_output_pins != SOF_TDFB_NUM_OUTPUT_PINS) {
		comp_err(dev, "Illegal output pins count %d", mod->priv.cfg.nb_output_pins);
		return -EINVAL;
	}

	ipc4_base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, params);
	component_set_nearest_period_frames(dev, params->rate);
	sourceb = comp_dev_get_first_data_producer(dev);
	ipc4_update_buffer_format(sourceb, &mod->priv.cfg.input_pins[0].audio_fmt);

	sinkb = comp_dev_get_first_data_consumer(dev);
	ipc4_update_buffer_format(sinkb, &mod->priv.cfg.output_pins[0].audio_fmt);
	return 0;
}

