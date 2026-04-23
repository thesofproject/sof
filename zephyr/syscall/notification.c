// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <ipc4/notification.h>
#include <zephyr/internal/syscall_handler.h>

static inline bool z_vrfy_send_resource_notif(uint32_t resource_id, uint32_t event_type,
					      uint32_t resource_type, void *data,
					      uint32_t data_size)
{
	if (data && data_size) {
		K_OOPS(K_SYSCALL_MEMORY_READ(data, data_size));
	}

	return z_impl_send_resource_notif(resource_id, event_type, resource_type, data, data_size);
}
#include <zephyr/syscalls/send_resource_notif_mrsh.c>
