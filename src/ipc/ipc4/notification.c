// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Piotr Makaruk <piotr.makaruk@intel.com>
 *	   Adrian Warecki <adrian.warecki@intel.com>
 */

#include <sof/common.h>
#include <sof/ipc/msg.h>
#include <stdbool.h>
#include <ipc4/notification.h>
#include <sof/ipc/notification_pool.h>

#include <rtos/symbol.h>

static uint32_t notification_mask = 0xFFFFFFFF;

static bool is_notif_filtered_out(uint32_t event_type)
{
	uint32_t notif_idx;

	switch (event_type) {
	case SOF_IPC4_GATEWAY_UNDERRUN_DETECTED:
		notif_idx = IPC4_UNDERRUN_AT_GATEWAY_NOTIFICATION_MASK_IDX;
		break;
	case SOF_IPC4_MIXER_UNDERRUN_DETECTED:
		notif_idx = IPC4_UNDERRUN_AT_MIXER_NOTIFICATION_MASK_IDX;
		break;
	case SOF_IPC4_GATEWAY_OVERRUN_DETECTED:
		notif_idx = IPC4_OVERRUN_AT_GATEWAY_NOTIFICATION_MASK_IDX;
		break;
	default:
		return false;
	}

	return (notification_mask & BIT(notif_idx)) == 0;
}

static bool send_resource_notif(uint32_t resource_id, uint32_t event_type, uint32_t resource_type,
				void *data, uint32_t data_size)
{
	struct ipc_msg *msg;

	if (is_notif_filtered_out(event_type))
		return true; //silently ignore

	msg = ipc_notification_pool_get(IPC4_RESOURCE_EVENT_SIZE);
	if (!msg)
		return false;

	struct ipc4_resource_event_data_notification *notif = msg->tx_data;
	union ipc4_notification_header header;

	header.r.notif_type = SOF_IPC4_NOTIFY_RESOURCE_EVENT;
	header.r.type = SOF_IPC4_GLB_NOTIFICATION;
	header.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	header.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
	msg->header = header.dat;

	notif->resource_id = resource_id;
	notif->event_type = event_type;
	notif->resource_type = resource_type;
	memset(&notif->event_data, 0, sizeof(notif->event_data));
	if (data && data_size)
		memcpy_s(&notif->event_data, sizeof(notif->event_data), data, data_size);

	ipc_msg_send(msg, msg->tx_data, false);

	return true;
}

void ipc4_update_notification_mask(uint32_t ntfy_mask, uint32_t enabled_mask)
{
	notification_mask &= enabled_mask | (~ntfy_mask);
	notification_mask |= enabled_mask & ntfy_mask;
}

bool send_copier_gateway_underrun_notif_msg(uint32_t pipeline_id)
{
	return send_resource_notif(pipeline_id, SOF_IPC4_GATEWAY_UNDERRUN_DETECTED,
				   SOF_IPC4_PIPELINE, NULL, 0);
}

bool send_gateway_underrun_notif_msg(uint32_t resource_id)
{
	return send_resource_notif(resource_id, SOF_IPC4_GATEWAY_UNDERRUN_DETECTED,
				   SOF_IPC4_GATEWAY, NULL, 0);
}

bool send_copier_gateway_overrun_notif_msg(uint32_t pipeline_id)
{
	return send_resource_notif(pipeline_id, SOF_IPC4_GATEWAY_OVERRUN_DETECTED,
				   SOF_IPC4_PIPELINE, NULL, 0);
}

bool send_gateway_overrun_notif_msg(uint32_t resource_id)
{
	return send_resource_notif(resource_id, SOF_IPC4_GATEWAY_OVERRUN_DETECTED,
				   SOF_IPC4_GATEWAY, NULL, 0);
}

void send_mixer_underrun_notif_msg(uint32_t resource_id, uint32_t eos_flag, uint32_t data_mixed,
				   uint32_t expected_data_mixed)
{
	struct ipc4_mixer_underrun_event_data mixer_underrun_data;

	mixer_underrun_data.eos_flag = eos_flag;
	mixer_underrun_data.data_mixed = data_mixed;
	mixer_underrun_data.expected_data_mixed = expected_data_mixed;

	send_resource_notif(resource_id, SOF_IPC4_MIXER_UNDERRUN_DETECTED, SOF_IPC4_PIPELINE,
			    &mixer_underrun_data, sizeof(mixer_underrun_data));
}
EXPORT_SYMBOL(send_mixer_underrun_notif_msg);

void send_process_data_error_notif_msg(uint32_t resource_id, uint32_t error_code)
{
	struct ipc4_process_data_error_event_data error_data;

	error_data.error_code = error_code;

	send_resource_notif(resource_id, SOF_IPC4_PROCESS_DATA_ERROR, SOF_IPC4_MODULE_INSTANCE,
			    &error_data, sizeof(error_data));
}
