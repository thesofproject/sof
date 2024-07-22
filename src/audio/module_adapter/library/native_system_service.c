// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Jaroslaw Stelter <jaroslaw.stelter@linux.intel.com>
 */
/*
 * Native System Service interface for ADSP loadable library.
 */

#include <errno.h>
#include <stdbool.h>
#include <sof/common.h>
#include <rtos/sof.h>
#include <rtos/string.h>
#include <ipc4/notification.h>
#include <sof/ipc/msg.h>
#include <native_system_service.h>
#include <sof/lib_manager.h>

#define RSIZE_MAX 0x7FFFFFFF

void native_system_service_log_message(AdspLogPriority log_priority, uint32_t log_entry,
				       AdspLogHandle const *log_handle, uint32_t param1,
				       uint32_t param2, uint32_t param3, uint32_t param4)
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

AdspErrorCode native_system_service_safe_memcpy(void *RESTRICT dst, size_t maxlen,
						const void *RESTRICT src,
						size_t len)
{
	return (AdspErrorCode) memcpy_s(dst, maxlen, src, len);
}

AdspErrorCode native_system_service_safe_memmove(void *dst, size_t maxlen, const void *src,
						 size_t len)
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

void *native_system_service_vec_memset(void *dst, int c, size_t len)
{
	/* TODO: Currently simple memset. Should be changed. */
	memset(dst, c, len);
	return dst;
}

AdspErrorCode native_system_service_create_notification(notification_params *params,
							uint8_t *notification_buffer,
							uint32_t notification_buffer_size,
							adsp_notification_handle *handle)
{
	if ((params == NULL) || (notification_buffer == NULL)
		|| (notification_buffer_size <= 0) || (handle == NULL))
		return ADSP_INVALID_PARAMETERS;

	union ipc4_notification_header header;

	header.r.notif_type = params->type;
	header.r._reserved_0 = params->user_val_1;
	header.r.type = SOF_IPC4_GLB_NOTIFICATION;
	header.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REQUEST;
	header.r.msg_tgt = SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG;
	struct ipc_msg *msg = lib_notif_msg_init((uint32_t)header.dat, notification_buffer_size);

	if (msg) {
		*handle = (adsp_notification_handle)msg;
		params->payload = msg->tx_data;
	}

	return ADSP_NO_ERROR;
}

AdspErrorCode native_system_service_send_notif_msg(adsp_notification_target notification_target,
						   adsp_notification_handle message,
						   uint32_t actual_payload_size)
{
	if ((message == NULL) || (actual_payload_size == 0))
		return ADSP_INVALID_PARAMETERS;

	struct ipc_msg *msg = (struct ipc_msg *)message;

	lib_notif_msg_send(msg);
	return ADSP_NO_ERROR;
}

AdspErrorCode native_system_service_get_interface(adsp_iface_id id, system_service_iface  **iface)
{
	if (id < 0)
		return ADSP_INVALID_PARAMETERS;
	return ADSP_NO_ERROR;
}

const struct native_system_service_api native_system_service = {
	.log_message  = native_system_service_log_message,
	.safe_memcpy = native_system_service_safe_memcpy,
	.safe_memmove = native_system_service_safe_memmove,
	.vec_memset = native_system_service_vec_memset,
	.notification_create = native_system_service_create_notification,
	.notification_send = native_system_service_send_notif_msg,
	.get_interface = native_system_service_get_interface
};
