// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

/**
 * \file mfcc_ipc4.c
 * \brief IPC4-specific functions for MFCC component.
 *
 * Provides VAD switch control notification to user space via the
 * IPC4 module notification mechanism.
 */

#include <sof/audio/mfcc/mfcc_comp.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/ipc/msg.h>
#include <sof/trace/trace.h>
#include <ipc4/base-config.h>
#include <ipc4/header.h>
#include <ipc4/module.h>
#include <ipc4/notification.h>
#include <rtos/string.h>
#include <errno.h>
#include <stdint.h>

LOG_MODULE_DECLARE(mfcc, CONFIG_SOF_LOG_LEVEL);

/**
 * \brief Initialize IPC notification message for VAD switch control.
 *
 * Allocates and configures the IPC message used to send VAD state
 * change notifications to user space via a switch control.
 */
int mfcc_ipc_notification_init(struct processing_module *mod)
{
	struct mfcc_comp_data *cd = module_get_private_data(mod);
	struct ipc_msg msg_proto;
	struct comp_dev *dev = mod->dev;
	struct comp_ipc_config *ipc_config = &dev->ipc_config;
	union ipc4_notification_header *primary =
		(union ipc4_notification_header *)&msg_proto.header;
	struct sof_ipc4_notify_module_data *msg_module_data;
	struct sof_ipc4_control_msg_payload *msg_payload;

	memset_s(&msg_proto, sizeof(msg_proto), 0, sizeof(msg_proto));
	primary->r.notif_type = SOF_IPC4_MODULE_NOTIFICATION;
	primary->r.type = SOF_IPC4_GLB_NOTIFICATION;
	primary->r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	primary->r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
	cd->msg = ipc_msg_w_ext_init(msg_proto.header, msg_proto.extension,
				     sizeof(struct sof_ipc4_notify_module_data) +
				     sizeof(struct sof_ipc4_control_msg_payload) +
				     sizeof(struct sof_ipc4_ctrl_value_chan));
	if (!cd->msg) {
		comp_err(dev, "Failed to initialize VAD notification");
		return -ENOMEM;
	}

	msg_module_data = (struct sof_ipc4_notify_module_data *)cd->msg->tx_data;
	msg_module_data->instance_id = IPC4_INST_ID(ipc_config->id);
	msg_module_data->module_id = IPC4_MOD_ID(ipc_config->id);
	msg_module_data->event_id = SOF_IPC4_NOTIFY_MODULE_EVENTID_ALSA_MAGIC_VAL |
		SOF_IPC4_SWITCH_CONTROL_PARAM_ID;
	msg_module_data->event_data_size = sizeof(struct sof_ipc4_control_msg_payload) +
		sizeof(struct sof_ipc4_ctrl_value_chan);

	msg_payload = (struct sof_ipc4_control_msg_payload *)msg_module_data->event_data;
	msg_payload->id = MFCC_CTRL_INDEX_VAD;
	msg_payload->num_elems = 1;
	msg_payload->chanv[0].channel = 0;

	comp_dbg(dev, "VAD notification init: instance_id = 0x%08x, module_id = 0x%08x",
		 msg_module_data->instance_id, msg_module_data->module_id);
	return 0;
}

/**
 * \brief Send VAD switch control notification to user space.
 * \param mod Processing module.
 * \param val VAD value: 1 = speech, 0 = silence.
 */
void mfcc_send_vad_notification(struct processing_module *mod, uint32_t val)
{
	struct mfcc_comp_data *cd = module_get_private_data(mod);
	struct sof_ipc4_notify_module_data *msg_module_data;
	struct sof_ipc4_control_msg_payload *msg_payload;

	if (!cd->msg)
		return;

	msg_module_data = (struct sof_ipc4_notify_module_data *)cd->msg->tx_data;
	msg_payload = (struct sof_ipc4_control_msg_payload *)msg_module_data->event_data;
	msg_payload->chanv[0].value = val;
	ipc_msg_send(cd->msg, NULL, false);
}

int mfcc_get_config(struct processing_module *mod,
		    uint32_t config_id, uint32_t *data_offset_size,
		    uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct mfcc_comp_data *cd = module_get_private_data(mod);
	struct sof_ipc4_control_msg_payload *ctl;

	comp_info(mod->dev, "entry");

	switch (config_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID:
		ctl = (struct sof_ipc4_control_msg_payload *)fragment;
		if (ctl->id == MFCC_CTRL_INDEX_VAD && ctl->num_elems == 1) {
			ctl->chanv[0].value = cd->vad_prev ? 1 : 0;
			*data_offset_size = sizeof(*ctl) + sizeof(ctl->chanv[0]);
			return 0;
		}
		return -EINVAL;
	default:
		return comp_data_blob_get_cmd(cd->model_handler, cdata, fragment_size);
	}
}

int mfcc_set_config(struct processing_module *mod, uint32_t config_id,
		    enum module_cfg_fragment_position pos, uint32_t data_offset_size,
		    const uint8_t *fragment, size_t fragment_size, uint8_t *response,
		    size_t response_size)
{
	struct mfcc_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "entry");

	switch (config_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID:
		/* VAD switch is read-only, ignore set requests */
		return 0;
	default:
		return comp_data_blob_set(cd->model_handler, pos, data_offset_size,
					  fragment, fragment_size);
	}
}
