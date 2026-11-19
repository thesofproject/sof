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

/**
 * @brief Supervisor implementation of sof_thread_cpu_pin().
 */
int z_impl_sof_thread_cpu_pin(struct k_thread *thread, int cpu)
{
	return k_thread_cpu_pin(thread, cpu);
}

/**
 * @brief Userspace verification wrapper for sof_thread_cpu_pin().
 *
 * The caller must hold access to an initialized thread object and the
 * core id must be valid.
 */
static inline int z_vrfy_sof_thread_cpu_pin(struct k_thread *thread, int cpu)
{
	K_OOPS(K_SYSCALL_OBJ(thread, K_OBJ_THREAD));
	K_OOPS(K_SYSCALL_VERIFY_MSG(cpu >= 0 && cpu < CONFIG_MP_MAX_NUM_CPUS,
				    "invalid cpu %d", cpu));

	return z_impl_sof_thread_cpu_pin(thread, cpu);
}
#include <zephyr/syscalls/sof_thread_cpu_pin_mrsh.c>
