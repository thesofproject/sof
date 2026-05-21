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

__syscall int z_sof_umutex_init(struct sof_umutex *umutex);
__syscall int z_sof_umutex_lock(struct sof_umutex *umutex, k_timeout_t timeout);
__syscall int z_sof_umutex_unlock(struct sof_umutex *umutex);
__syscall void z_sof_umutex_free(struct sof_umutex *umutex);

/**
 * @brief Initialize a dynamic user-space mutex.
 *
 * Allocates the backing k_mutex kernel object. The sof_umutex struct
 * must reside in memory accessible to the calling thread.
 *
 * @param umutex Pointer to the sof_umutex state (in user-accessible memory)
 * @return 0 on success, -ENOMEM if allocation fails
 */
static inline int sof_umutex_init(struct sof_umutex *umutex)
{
	return z_sof_umutex_init(umutex);
}

/**
 * @brief Lock a dynamic user-space mutex.
 *
 * @param umutex Pointer to the sof_umutex state
 * @param timeout Timeout value (K_FOREVER for indefinite wait)
 * @return 0 on success, -EAGAIN on timeout, -EINVAL if not initialized
 */
static inline int sof_umutex_lock(struct sof_umutex *umutex, k_timeout_t timeout)
{
	return z_sof_umutex_lock(umutex, timeout);
}

/**
 * @brief Unlock a dynamic user-space mutex.
 *
 * @param umutex Pointer to the sof_umutex state
 * @return 0 on success, -EINVAL if not initialized, -EPERM if not owner
 */
static inline int sof_umutex_unlock(struct sof_umutex *umutex)
{
	return z_sof_umutex_unlock(umutex);
}

/**
 * @brief Free a dynamic user-space mutex.
 *
 * Releases the backing k_mutex kernel object via k_object_free().
 * The sof_umutex state must not be used after this call.
 *
 * @param umutex Pointer to the sof_umutex state
 */
static inline void sof_umutex_free(struct sof_umutex *umutex)
{
	z_sof_umutex_free(umutex);
}

#include <zephyr/syscalls/umutex.h>

#endif /* __ZEPHYR_RTOS_UMUTEX_H__ */
