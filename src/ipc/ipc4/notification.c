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

#if CONFIG_XRUN_NOTIFICATIONS_ENABLE
void xrun_notif_msg_init(struct ipc_msg *msg_xrun, uint32_t resource_id, uint32_t event_type)
{
	struct ipc4_resource_event_data_notification *notif_data = msg_xrun->tx_data;

	resource_notif_header_init(msg_xrun);
	notif_data->resource_id = resource_id;
	notif_data->event_type = event_type;
	notif_data->resource_type = SOF_IPC4_GATEWAY;
}
#endif

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
