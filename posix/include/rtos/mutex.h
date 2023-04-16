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

#endif
