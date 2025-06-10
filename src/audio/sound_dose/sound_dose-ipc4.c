// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <ipc4/base-config.h>
#include <ipc4/header.h>
#include <ipc4/module.h>
#include <ipc4/notification.h>

#include "sound_dose.h"

LOG_MODULE_DECLARE(sound_dose, CONFIG_SOF_LOG_LEVEL);

static struct ipc_msg *sound_dose_notification_init(struct processing_module *mod,
						    uint32_t control_type_param_id,
						    uint32_t control_id)
{
	struct ipc_msg msg_proto;
	struct comp_dev *dev = mod->dev;
	struct comp_ipc_config *ipc_config = &dev->ipc_config;
	union ipc4_notification_header *primary =
		(union ipc4_notification_header *)&msg_proto.header;
	struct sof_ipc4_notify_module_data *msg_module_data;
	struct sof_ipc4_control_msg_payload *msg_payload;
	struct ipc_msg *msg;

	/* Clear header, extension, and other ipc_msg members */
	memset_s(&msg_proto, sizeof(msg_proto), 0, sizeof(msg_proto));
	primary->r.notif_type = SOF_IPC4_MODULE_NOTIFICATION;
	primary->r.type = SOF_IPC4_GLB_NOTIFICATION;
	primary->r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	primary->r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
	msg = ipc_msg_w_ext_init(msg_proto.header, msg_proto.extension,
				 sizeof(struct sof_ipc4_notify_module_data) +
				 sizeof(struct sof_ipc4_control_msg_payload));
	if (!msg)
		return NULL;

	msg_module_data = (struct sof_ipc4_notify_module_data *)msg->tx_data;
	msg_module_data->instance_id = IPC4_INST_ID(ipc_config->id);
	msg_module_data->module_id = IPC4_MOD_ID(ipc_config->id);
	msg_module_data->event_id = SOF_IPC4_NOTIFY_MODULE_EVENTID_ALSA_MAGIC_VAL |
		control_type_param_id;
	msg_module_data->event_data_size = sizeof(struct sof_ipc4_control_msg_payload);
	msg_payload = (struct sof_ipc4_control_msg_payload *)msg_module_data->event_data;
	msg_payload->id = control_id;
	msg_payload->num_elems = 0;
	comp_dbg(dev, "instance_id = 0x%08x, module_id = 0x%08x",
		 msg_module_data->instance_id, msg_module_data->module_id);
	return msg;
}

int sound_dose_ipc_notification_init(struct processing_module *mod)
{
	struct sound_dose_comp_data *cd = module_get_private_data(mod);

	cd->msg = sound_dose_notification_init(mod, SOF_IPC4_BYTES_CONTROL_PARAM_ID,
					       SOF_SOUND_DOSE_PAYLOAD_PARAM_ID);
	if (!cd->msg) {
		comp_err(mod->dev, "Failed to initialize control notification.");
		return -ENOMEM;
	}

	return 0;
}

void sound_dose_send_ipc_notification(const struct processing_module *mod)
{
	struct sound_dose_comp_data *cd = module_get_private_data(mod);

	ipc_msg_send(cd->msg, cd->msg->tx_data, false);
}

/* This function handles the set_config() commands. The controls have the next purpose:
 * - SOF_SOUND_DOSE_SETUP_PARAM_ID sets the acoustical sensitivity of the DAC, headphone
 *   amplifier and assumed worst-case loud headphones. E.g. 0 dBFS equals 100 dBSPL. The
 *   sensitivity is for max user volume.
 *  -SOF_SOUND_DOSE_VOLUME_PARAM_ID is set to new decibels value if volume is adjusted
 *   down from user maximum.
 * - SOF_SOUND_DOSE_GAIN_PARAM_ID is normally set to 0 decibels value. If the user's
 *   listening is exceeding the safe MSD threshold the user's volume can be forced down
 *   with this gain. The bytes control is not visible in the mixer, so there is no simple
 *   way for user to force volume up with alsamixer or amixer utililities. The gain
 *   should be restored to 0 dB after the MSD value looks again safe.
 *   E.g. setting -10 dB value lowers user's listening volume with -10 dB and also lower
 *   the reported MEL values by -10 dB.
 *
 * These controls can be used as preferred by the MEL->MSD algorithm in user space. The
 * controls offset the reported MEL values. The same offset can be also done in user
 * space. And could use codec volume instead of SOF_SOUND_DOSE_GAIN_PARAM_ID if preferred.
 */
__cold static int
_sound_dose_set_config(struct processing_module *mod, uint32_t param_id,
		       uint32_t control_id, const void *data, uint32_t data_size)
{
	struct sound_dose_comp_data *cd = module_get_private_data(mod);
	struct sound_dose_setup_config *new_setup;
	struct sound_dose_volume_config *new_volume;
	struct sound_dose_gain_config *new_gain;
	struct comp_dev *dev = mod->dev;
	uint8_t *dest;
	size_t dest_size;

	assert_can_be_cold();

	if (param_id != SOF_IPC4_BYTES_CONTROL_PARAM_ID) {
		comp_warn(dev, "Not supported control type: %u", param_id);
		return 0;
	}

	comp_dbg(dev, "param_id = %u, control_id: %u", param_id, control_id);

	switch (control_id) {
	case SOF_SOUND_DOSE_SETUP_PARAM_ID:
		dest_size = sizeof(struct sound_dose_setup_config);
		dest = (uint8_t *)&cd->setup;
		break;
	case SOF_SOUND_DOSE_VOLUME_PARAM_ID:
		dest_size = sizeof(struct sound_dose_volume_config);
		dest = (uint8_t *)&cd->vol;
		break;
	case SOF_SOUND_DOSE_GAIN_PARAM_ID:
		dest_size = sizeof(struct sound_dose_gain_config);
		dest = (uint8_t *)&cd->att;
		break;
	case SOF_SOUND_DOSE_PAYLOAD_PARAM_ID:
		return 0; /* Just return, no need to set the audio feature data */
	default:
		comp_warn(dev, "Ignored illegal control_id: %u", control_id);
		return 0;
	}

	if (data_size != dest_size) {
		comp_err(dev, "Illegal fragment_size %u for %u:%u",
			 data_size, param_id, control_id);
		return -EINVAL;
	}

	switch (control_id) {
	case SOF_SOUND_DOSE_SETUP_PARAM_ID:
		new_setup = (struct sound_dose_setup_config *)data;
		if (new_setup->sens_dbfs_dbspl < SOF_SOUND_DOSE_SENS_MIN_DB ||
		    new_setup->sens_dbfs_dbspl > SOF_SOUND_DOSE_SENS_MAX_DB) {
			comp_err(dev, "Illegal sensitivity = %d", new_setup->sens_dbfs_dbspl);
			return -EINVAL;
		}
		break;
	case SOF_SOUND_DOSE_VOLUME_PARAM_ID:
		new_volume = (struct sound_dose_volume_config *)data;
		if (new_volume->volume_offset < SOF_SOUND_DOSE_VOLUME_MIN_DB ||
		    new_volume->volume_offset > SOF_SOUND_DOSE_VOLUME_MAX_DB) {
			comp_err(dev, "Illegal volume = %d", new_volume->volume_offset);
			return -EINVAL;
		}
		break;
	case SOF_SOUND_DOSE_GAIN_PARAM_ID:
		new_gain = (struct sound_dose_gain_config *)data;
		if (new_gain->gain < SOF_SOUND_DOSE_GAIN_MIN_DB ||
		    new_gain->gain > SOF_SOUND_DOSE_GAIN_MAX_DB) {
			comp_err(dev, "Illegal gain = %d", new_gain->gain);
			return -EINVAL;
		}
		cd->gain_update = true;
		break;
	}

	memcpy_s(dest, dest_size, data, data_size);
	return 0;
}

/* This function is the main set_config() handler. The two variants of bytes control
 * are handled. The case where param_id is set to SOF_IPC4_BYTES_CONTROL_PARAM_ID
 * with header sof_ipc4_control_msg_payload has the benefit of being able to pass
 * the id of the control. It is useful when there are multiple instances of
 * bytes control. The legacy case is where control instances are identified with
 * param_id value. The first format with header must be used when a control supports
 * a notification to user space. The legacy works only for simple set control usage.
 */
__cold int sound_dose_set_config(struct processing_module *mod,
				 uint32_t param_id,
				 enum module_cfg_fragment_position pos,
				 uint32_t data_offset_size,
				 const uint8_t *fragment,
				 size_t fragment_size,
				 uint8_t *response,
				 size_t response_size)
{
	int ret;

	assert_can_be_cold();

	if (!fragment_size) {
		comp_warn(mod->dev, "Zero fragment size for param_id %d", param_id);
		return 0;
	}

	if (param_id == SOF_IPC4_BYTES_CONTROL_PARAM_ID) {
		struct sof_ipc4_control_msg_payload *msg_payload;

		msg_payload = (struct sof_ipc4_control_msg_payload *)fragment;

		ret = _sound_dose_set_config(mod, param_id, msg_payload->id,
					     msg_payload->data, msg_payload->num_elems);
	} else {
		ret = _sound_dose_set_config(mod, SOF_IPC4_BYTES_CONTROL_PARAM_ID,
					     param_id, fragment, fragment_size);
	}

	return ret;
}

/* This function copies the data for get_config() request by the driver. Only
 * the SOF_SOUND_DOSE_PAYLOAD_PARAM_ID as control_id is supported. The audio
 * feature payload for sound_dose is copied to the recipient.
 */
__cold static int
_sound_dose_get_config(struct processing_module *mod, uint32_t param_id,
		       uint32_t control_id, uint32_t *size_out,
				 uint8_t *data, size_t data_size)
{
	struct sound_dose_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	size_t payload_size;

	assert_can_be_cold();

	if (param_id != SOF_IPC4_BYTES_CONTROL_PARAM_ID) {
		comp_warn(dev, "Not supported control type: %u", param_id);
		return 0;
	}

	comp_dbg(dev, "param_id = %u, control_id: %u", param_id, control_id);

	if (control_id != SOF_SOUND_DOSE_PAYLOAD_PARAM_ID) {
		comp_warn(dev, "Ignored get config control_id: %u", control_id);
		memset(data, 0, data_size);
		*size_out = 0;
		return 0;
	}

	payload_size = cd->abi->size;
	memcpy_s(data, data_size, cd->abi->data, payload_size);
	*size_out = payload_size;

	return 0;
}

/* This is the main get_config() handler. As in set_config() case, the no-header
 * way is legacy. The response get_config() after notify must use the
 * sof_ipc4_control_msg_payload header.
 */
__cold int sound_dose_get_config(struct processing_module *mod,
				 uint32_t param_id, uint32_t *data_offset_size,
				 uint8_t *fragment, size_t fragment_size)
{
	int ret;

	assert_can_be_cold();

	if (!fragment_size) {
		comp_warn(mod->dev, "Zero fragment size for param_id %d", param_id);
		return 0;
	}

	if (param_id == SOF_IPC4_BYTES_CONTROL_PARAM_ID) {
		struct sof_ipc4_control_msg_payload *msg_payload;

		/* Note: msg_payload will get overwrite as fragment. */
		msg_payload = (struct sof_ipc4_control_msg_payload *)fragment;
		ret = _sound_dose_get_config(mod, param_id, msg_payload->id,
					     data_offset_size, fragment,
					     fragment_size);
	} else {
		ret = _sound_dose_get_config(mod, SOF_IPC4_BYTES_CONTROL_PARAM_ID,
					     param_id, data_offset_size,
					     fragment, fragment_size);
	}

	return ret;
}
