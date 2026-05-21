// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//

#ifndef __ZEPHYR_RTOS_UMUTEX_H__
#define __ZEPHYR_RTOS_UMUTEX_H__

#include <zephyr/kernel.h>

/**
 * @brief User-space accessible mutex with dynamic allocation.
 *
 * This mutex variant can be dynamically allocated at runtime. The state
 * struct resides in user-accessible memory; access to the mutex is granted
 * to any thread that can access this memory region (no per-thread
 * k_thread_access_grant needed).
 *
 * The backing k_mutex is allocated via k_object_alloc(K_OBJ_MUTEX) during
 * init and freed via k_object_free() during free.
 */
struct sof_umutex {
	struct k_mutex *mutex;  /**< Pointer to dynamically-allocated backing k_mutex */
};

/**
 * @brief Initialize a dynamic user-space mutex.
 *
 * Allocates the backing k_mutex kernel object. The sof_umutex struct
 * must reside in memory accessible to the calling thread.
 *
 * @param umutex Pointer to the sof_umutex state (in user-accessible memory)
 * @return 0 on success, -ENOMEM if allocation fails
 */
__syscall int sof_umutex_init(struct sof_umutex *umutex);

/**
 * @brief Lock a dynamic user-space mutex.
 *
 * @param umutex Pointer to the sof_umutex state
 * @param timeout Timeout value (K_FOREVER for indefinite wait)
 * @return 0 on success, -EAGAIN on timeout, -EINVAL if not initialized
 */
__syscall int sof_umutex_lock(struct sof_umutex *umutex, k_timeout_t timeout);

/**
 * @brief Unlock a dynamic user-space mutex.
 *
 * @param umutex Pointer to the sof_umutex state
 * @return 0 on success, -EINVAL if not initialized, -EPERM if not owner
 */
__syscall int sof_umutex_unlock(struct sof_umutex *umutex);

/**
 * @brief Free a dynamic user-space mutex.
 *
 * Releases the backing k_mutex kernel object via k_object_free().
 * The sof_umutex state must not be used after this call.
 *
 * @param umutex Pointer to the sof_umutex state
 */
__syscall void sof_umutex_free(struct sof_umutex *umutex);

int z_impl_sof_umutex_init(struct sof_umutex *umutex);
int z_impl_sof_umutex_lock(struct sof_umutex *umutex, k_timeout_t timeout);
int z_impl_sof_umutex_unlock(struct sof_umutex *umutex);
void z_impl_sof_umutex_free(struct sof_umutex *umutex);

#ifdef CONFIG_SOF_USERSPACE_INTERFACE_MUTEX
#include <zephyr/syscalls/umutex.h>
#else
static inline int sof_umutex_init(struct sof_umutex *umutex)
{
	return z_impl_sof_umutex_init(umutex);
}

static inline int sof_umutex_lock(struct sof_umutex *umutex, k_timeout_t timeout)
{
	return z_impl_sof_umutex_lock(umutex, timeout);
}

static inline int sof_umutex_unlock(struct sof_umutex *umutex)
{
	return z_impl_sof_umutex_unlock(umutex);
}

static inline void sof_umutex_free(struct sof_umutex *umutex)
{
	z_impl_sof_umutex_free(umutex);
}

#endif /* CONFIG_SOF_USERSPACE_INTERFACE_MUTEX */

#endif /* __ZEPHYR_RTOS_UMUTEX_H__ */
