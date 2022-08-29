// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Jaroslaw Stelter <jaroslaw.stelter@linux.intel.com>

/*
 * System Service interface for ADSP loadable library.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <sof/common.h>
#include <sof/sof.h>
#include <rtos/string.h>
#include <ipc4/notification.h>
#include <sof/ipc/msg.h>
#include <adsp_error_code.h>
#include <system_service.h>
#include <sof/lib_manager.h>

#define RSIZE_MAX 0x7FFFFFFF

void SystemServiceLogMessage(AdspLogPriority log_priority, uint32_t log_entry,
			     AdspLogHandle const *log_handle, uint32_t param1, uint32_t param2,
			     uint32_t param3, uint32_t param4)
{
	uint32_t argc = (log_entry & 0x7);
	/* TODO: Need to call here function like _log_sofdict, since we do not have format */
	/*       passed from library */
	/* This function could be finished when cAVS/ACE logging formats support will be */
	/* added to SOF.*/
	switch (argc) {
	case 1:
	break;

	case 2:
	break;

	case 3:
	break;

	case 4:
	break;

	default:
	break;
	}
}

AdspErrorCode SystemServiceSafeMemcpy(void *RESTRICT dst, size_t maxlen, const void *RESTRICT src,
				      size_t len)
{
	return (AdspErrorCode) memcpy_s(dst, maxlen, src, len);
}

AdspErrorCode SystemServiceSafeMemmove(void *dst, size_t maxlen, const void *src, size_t len)
{
	if (dst == NULL || maxlen > RSIZE_MAX)
		return ADSP_INVALID_PARAMETERS;

	if (src == NULL || len > maxlen) {
		memset(dst, 0, maxlen);
		return ADSP_INVALID_PARAMETERS;
	}

	if (len != 0) {
		/* TODO: now it is memcopy. Finally it will be remap maybe?
		 * Fix it when memory management API will be available.
		 * memmove(dst, src, len);
		 */
		memcpy_s(dst, maxlen, src, len);
	}
	return ADSP_NO_ERROR;
}

void *SystemServiceVecMemset(void *dst, int c, size_t len)
{
	/* TODO: Currently simple memset. Should be changed. */
	memset(dst, c, len);
	return dst;
}

AdspErrorCode SystemServiceCreateNotification(NotificationParams *params,
					      uint8_t *notification_buffer,
					      uint32_t notification_buffer_size,
					      AdspNotificationHandle *handle)
{
	if ((params == NULL) || (notification_buffer == NULL)
		|| (notification_buffer_size <= 0) || (handle == NULL))
		return ADSP_INVALID_PARAMETERS;

	/* TODO: IPC header setup */
	/* https://github.com/thesofproject/sof/pull/5720 needed for completion. */
	union ipc4_notification_header header;

	header.r.notif_type = params->type;
	header.r._reserved_0 = params->user_val_1;
	header.r.type = SOF_IPC4_GLB_NOTIFICATION;
	header.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	header.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
	struct ipc_msg *msg = lib_notif_msg_init((uint32_t)header.dat, notification_buffer_size);

	if (msg) {
		*handle = (AdspNotificationHandle)msg;
		params->payload = msg->tx_data;
	}

	return ADSP_NO_ERROR;
}

AdspErrorCode SystemServiceSendNotificationMessage(NotificationTarget notification_target,
						   AdspNotificationHandle message,
						   uint32_t actual_payload_size)
{
	if ((message == NULL) || (actual_payload_size == 0))
		return ADSP_INVALID_PARAMETERS;

	struct ipc_msg *msg = (struct ipc_msg *)message;

	lib_notif_msg_send(msg);
	return ADSP_NO_ERROR;
}

AdspErrorCode SystemServiceGetInterface(AdspIfaceId id, SystemServiceIface  **iface)
{
	if (id < 0)
		return ADSP_INVALID_PARAMETERS;
	return ADSP_NO_ERROR;
}

