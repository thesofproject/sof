// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <sof/lib/notifier.h>
#include <zephyr/kernel.h>
#include <zephyr/internal/syscall_handler.h>

/**
 * \brief Userspace verification wrapper for notifier_event().
 *
 * Validates the notification type and verifies read access to the caller
 * payload buffer before delegating dispatch to supervisor context so user
 * threads never dereference kernel-only host_notify state.
 */
static inline void z_vrfy_notifier_event(const void *caller_id,
					 enum notify_id event_type,
					 uint32_t core_mask,
					 void *caller_data,
					 uint32_t data_size)
{
	K_OOPS(K_SYSCALL_VERIFY(event_type >= NOTIFIER_ID_CPU_FREQ &&
				event_type < NOTIFIER_ID_COUNT));
	K_OOPS(K_SYSCALL_VERIFY((!caller_data && !data_size) ||
				(caller_data && data_size)));
	if (caller_data && data_size)
		K_OOPS(K_SYSCALL_MEMORY_READ(caller_data, data_size));

	z_impl_notifier_event(caller_id, event_type, core_mask,
			      caller_data, data_size);
}
#include <zephyr/syscalls/notifier_event_mrsh.c>
