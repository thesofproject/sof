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

#include <rtos/symbol.h>

static void resource_notif_header_init(struct ipc_msg *msg)
{
	struct ipc4_resource_event_data_notification *notif_data = msg->tx_data;
	union ipc4_notification_header header;

	header.r.notif_type = SOF_IPC4_NOTIFY_RESOURCE_EVENT;
	header.r.type = SOF_IPC4_GLB_NOTIFICATION;
	header.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	header.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
	msg->header = header.dat;
	memset(&notif_data->event_data, 0, sizeof(notif_data->event_data));
}

void copier_gateway_underrun_notif_msg_init(struct ipc_msg *msg, uint32_t pipeline_id)
{
	struct ipc4_resource_event_data_notification *notif_data = msg->tx_data;

	resource_notif_header_init(msg);
	notif_data->resource_id = pipeline_id;
	notif_data->event_type = SOF_IPC4_GATEWAY_UNDERRUN_DETECTED;
	notif_data->resource_type = SOF_IPC4_PIPELINE;
}

void gateway_underrun_notif_msg_init(struct ipc_msg *msg, uint32_t resource_id)
{
	struct ipc4_resource_event_data_notification *notif_data = msg->tx_data;

	resource_notif_header_init(msg);
	notif_data->resource_id = resource_id;
	notif_data->event_type = SOF_IPC4_GATEWAY_UNDERRUN_DETECTED;
	notif_data->resource_type = SOF_IPC4_GATEWAY;
}

void copier_gateway_overrun_notif_msg_init(struct ipc_msg *msg, uint32_t pipeline_id)
{
	struct ipc4_resource_event_data_notification *notif_data = msg->tx_data;

	resource_notif_header_init(msg);
	notif_data->resource_id = pipeline_id;
	notif_data->event_type = SOF_IPC4_GATEWAY_OVERRUN_DETECTED;
	notif_data->resource_type = SOF_IPC4_PIPELINE;
}

void gateway_overrun_notif_msg_init(struct ipc_msg *msg, uint32_t resource_id)
{
	struct ipc4_resource_event_data_notification *notif_data = msg->tx_data;

	resource_notif_header_init(msg);
	notif_data->resource_id = resource_id;
	notif_data->event_type = SOF_IPC4_GATEWAY_OVERRUN_DETECTED;
	notif_data->resource_type = SOF_IPC4_GATEWAY;
}

void mixer_underrun_notif_msg_init(struct ipc_msg *msg, uint32_t resource_id, uint32_t eos_flag,
				   uint32_t data_mixed, uint32_t expected_data_mixed)
{
	struct ipc4_resource_event_data_notification *notif_data = msg->tx_data;

	resource_notif_header_init(msg);
	notif_data->resource_id = resource_id;
	notif_data->event_type = SOF_IPC4_MIXER_UNDERRUN_DETECTED;
	notif_data->resource_type = SOF_IPC4_PIPELINE;
	notif_data->event_data.mixer_underrun.eos_flag = eos_flag;
	notif_data->event_data.mixer_underrun.data_mixed = data_mixed;
	notif_data->event_data.mixer_underrun.expected_data_mixed = expected_data_mixed;
}
EXPORT_SYMBOL(mixer_underrun_notif_msg_init);

void process_data_error_notif_msg_init(struct ipc_msg *msg, uint32_t resource_id,
				       uint32_t error_code)
{
	struct ipc4_resource_event_data_notification *notif_data = msg->tx_data;

	resource_notif_header_init(msg);
	notif_data->resource_id = resource_id;
	notif_data->event_type = SOF_IPC4_PROCESS_DATA_ERROR;
	notif_data->resource_type = SOF_IPC4_MODULE_INSTANCE;
	notif_data->event_data.process_data_error.error_code = error_code;
}
