// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <sof/lib/cpu.h>
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
	int vad_state;

	/*
	 * Restrict userspace notifications to explicit, fixed-payload events
	 * dispatched synchronously on the local core so privileged kernel
	 * events and cross-core pointers to user memory cannot escape.
	 */
	K_OOPS(K_SYSCALL_VERIFY(event_type == NOTIFIER_ID_VAD));
	K_OOPS(K_SYSCALL_VERIFY((core_mask & ~NOTIFIER_TARGET_CORE_LOCAL) == 0));
	K_OOPS(K_SYSCALL_VERIFY(caller_data != NULL && data_size == sizeof(vad_state)));
	K_OOPS(k_usermode_from_copy(&vad_state, caller_data, sizeof(vad_state)));

	z_impl_notifier_event(caller_id, event_type, core_mask,
			      &vad_state, sizeof(vad_state));
}
#include <zephyr/syscalls/notifier_event_mrsh.c>
