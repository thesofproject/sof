// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Piotr Makaruk <piotr.makaruk@intel.com>
 */

#include <sof/common.h>
#include <stdbool.h>
#include <ipc4/notification.h>
#include <sof/ipc/msg.h>

#if CONFIG_XRUN_NOTIFICATIONS_ENABLE
void xrun_notif_msg_init(struct ipc_msg *msg_xrun, uint32_t resource_id, uint32_t event_type)
{
	struct ipc4_resource_event_data_notification *notif_data = msg_xrun->tx_data;
	union ipc4_notification_header header;

	header.r.notif_type = SOF_IPC4_NOTIFY_RESOURCE_EVENT;
	header.r.type = SOF_IPC4_GLB_NOTIFICATION;
	header.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	header.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
	msg_xrun->header = header.dat;

	notif_data->resource_id = resource_id;
	notif_data->event_type = event_type;
	notif_data->resource_type = SOF_IPC4_GATEWAY;
	memset(&notif_data->event_data, 0, sizeof(notif_data->event_data));
}
#endif
