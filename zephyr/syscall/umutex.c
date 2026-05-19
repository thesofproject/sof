// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

/**
 * @file
 * @brief SOF dynamic user-space mutex syscall verification.
 *
 * Verify handlers ensure the calling user thread has write access to
 * the sof_umutex state struct before forwarding to the implementation.
 */

#include <rtos/umutex.h>
#include <zephyr/internal/syscall_handler.h>

static inline int z_vrfy_sof_umutex_init(struct sof_umutex *umutex)
{
	K_OOPS(K_SYSCALL_MEMORY_WRITE(umutex, sizeof(struct sof_umutex)));
	return z_impl_sof_umutex_init(umutex);
}
#include <zephyr/syscalls/sof_umutex_init_mrsh.c>

static inline int z_vrfy_sof_umutex_lock(struct sof_umutex *umutex,
					    k_timeout_t timeout)
{
	K_OOPS(K_SYSCALL_MEMORY_WRITE(umutex, sizeof(struct sof_umutex)));
	return z_impl_sof_umutex_lock(umutex, timeout);
}
#include <zephyr/syscalls/sof_umutex_lock_mrsh.c>

static inline int z_vrfy_sof_umutex_unlock(struct sof_umutex *umutex)
{
	K_OOPS(K_SYSCALL_MEMORY_WRITE(umutex, sizeof(struct sof_umutex)));
	return z_impl_sof_umutex_unlock(umutex);
}
#include <zephyr/syscalls/sof_umutex_unlock_mrsh.c>

static inline void z_vrfy_sof_umutex_free(struct sof_umutex *umutex)
{
	K_OOPS(K_SYSCALL_MEMORY_WRITE(umutex, sizeof(struct sof_umutex)));
	z_impl_sof_umutex_free(umutex);
}
#include <zephyr/syscalls/sof_umutex_free_mrsh.c>
