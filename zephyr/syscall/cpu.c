// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <sof/lib/cpu.h>
#include <zephyr/kernel.h>
#include <zephyr/internal/syscall_handler.h>

/**
 * \brief Userspace verification wrapper for cpu_get_id().
 *
 * The system call takes no arguments and passes no pointers, so no
 * access validation is required; the call is simply forwarded to the
 * implementation running in supervisor context.
 *
 * @return Id of the DSP core executing the call.
 */
static inline int z_vrfy_cpu_get_id(void)
{
	return z_impl_cpu_get_id();
}
#include <zephyr/syscalls/cpu_get_id_mrsh.c>
