/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2023 - 2024 Intel Corporation. All rights reserved.
 */

#ifndef NATIVE_SYSTEM_SERVICE_H
#define NATIVE_SYSTEM_SERVICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <module/module/system_service.h>

struct native_system_service {
	struct system_service basic;
};

void native_system_service_log_message(enum log_priority log_priority, uint32_t log_entry,
				       struct log_handle const *log_handle, uint32_t param1,
				       uint32_t param2, uint32_t param3, uint32_t param4);

AdspErrorCode native_system_service_safe_memcpy(void *RESTRICT dst, size_t maxlen,
						const void *RESTRICT src, size_t len);

AdspErrorCode native_system_service_safe_memmove(void *dst, size_t maxlen,
						 const void *src, size_t len);

void *native_system_service_vec_memset(void *dst, int c, size_t len);

AdspErrorCode native_system_service_create_notification(struct notification_params *params,
							uint8_t *notification_buffer,
							uint32_t notification_buffer_size,
							struct notification_handle **handle);

AdspErrorCode native_system_service_send_notif_msg(enum notification_target notification_target,
						   struct notification_handle *message,
						   uint32_t actual_payload_size);

AdspErrorCode native_system_service_get_interface(enum interface_id id,
						  struct system_service_iface **iface);

extern const struct native_system_service native_system_service;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NATIVE_SYSTEM_SERVICE_H */
