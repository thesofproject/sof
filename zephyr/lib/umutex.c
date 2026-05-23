// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

/**
 * @file
 * @brief SOF dynamic user-space mutex implementation.
 *
 * Provides the kernel-side implementation of sof_umutex operations.
 * The backing k_mutex is dynamically allocated via k_object_alloc
 * and freed via k_object_free.
 */

#include <rtos/umutex.h>
#include <zephyr/kernel.h>
#include <zephyr/internal/syscall_handler.h>

int z_impl_z_sof_umutex_init(struct sof_umutex *umutex)
{
	struct k_mutex *m;
	int ret;

	m = k_object_alloc(K_OBJ_MUTEX);
	if (m == NULL) {
		return -ENOMEM;
	}

	ret = k_mutex_init(m);
	if (ret) {
		k_object_free(m);
		return ret;
	}

	umutex->mutex = m;
	return 0;
}

int z_impl_z_sof_umutex_lock(struct sof_umutex *umutex, k_timeout_t timeout)
{
	if (umutex->mutex == NULL) {
		return -EINVAL;
	}

	return k_mutex_lock(umutex->mutex, timeout);
}

int z_impl_z_sof_umutex_unlock(struct sof_umutex *umutex)
{
	if (umutex->mutex == NULL) {
		return -EINVAL;
	}

	return k_mutex_unlock(umutex->mutex);
}

void z_impl_z_sof_umutex_free(struct sof_umutex *umutex)
{
	if (umutex->mutex != NULL) {
		k_object_free(umutex->mutex);
		umutex->mutex = NULL;
	}
}
