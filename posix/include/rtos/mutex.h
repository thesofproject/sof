/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 */

/*
 * Simple mutex implementation for SOF.
 */

#ifndef __POSIX_RTOS_MUTEX_H
#define __POSIX_RTOS_MUTEX_H

#include <rtos/kernel.h>
#include <rtos/spinlock.h>
#include <stdint.h>

#define K_FOREVER ((k_timeout_t) { .ticks = 0xffffffff })

struct k_mutex {
	struct k_spinlock lock;
	k_spinlock_key_t key;
};

static inline int k_mutex_init(struct k_mutex *mutex)
{
	k_spinlock_init(&mutex->lock);
	return 0;
}

static inline int k_mutex_lock(struct k_mutex *mutex, k_timeout_t timeout)
{
	mutex->key = k_spin_lock(&mutex->lock);
	return 0;
}

static inline int k_mutex_unlock(struct k_mutex *mutex)
{
	k_spin_unlock(&mutex->lock, mutex->key);
	return 0;
}

/* provide a no-op implementation for zephyr/sys/mutex.h */

struct sys_mutex {
};

#define SYS_MUTEX_DEFINE(name) \
	struct sys_mutex name

static inline void sys_mutex_init(struct sys_mutex *mutex)
{
}

static inline int sys_mutex_lock(struct sys_mutex *mutex, k_timeout_t timeout)
{
	return 0;
}

static inline int sys_mutex_unlock(struct sys_mutex *mutex)
{
	return 0;
}

/* provide a no-op implementation for zephyr/sys/sem.h */

struct sys_sem {
};

static inline int sys_sem_init(struct sys_sem *sem, unsigned int initial_count,
			       unsigned int limit)
{
	return 0;
}

static inline int sys_sem_give(struct sys_sem *sem)
{
	return 0;
}

static inline int sys_sem_take(struct sys_sem *sem, k_timeout_t timeout)
{
	return 0;
}

/**
 * @brief User-space accessible mutex stub for host/testbench builds.
 */
struct sof_umutex {
	struct k_mutex mutex;  /**< Inline k_mutex for POSIX (no dynamic alloc needed) */
};

static inline int sof_umutex_init(struct sof_umutex *umutex)
{
	return k_mutex_init(&umutex->mutex);
}

static inline int sof_umutex_lock(struct sof_umutex *umutex, k_timeout_t timeout)
{
	return k_mutex_lock(&umutex->mutex, timeout);
}

static inline int sof_umutex_unlock(struct sof_umutex *umutex)
{
	return k_mutex_unlock(&umutex->mutex);
}

static inline void sof_umutex_free(struct sof_umutex *umutex)
{
	/* No-op on POSIX — no kernel objects to free */
}

#endif
