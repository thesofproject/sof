/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_SPINLOCK_H__

#ifndef __ARCH_SPINLOCK_H__
#define __ARCH_SPINLOCK_H__

#include <pthread.h>

typedef struct {
	pthread_mutex_t mutex;
} spinlock_t;

static inline void arch_spinlock_init(spinlock_t *lock)
{
	/* this is fatal on host and no way to tell caller */
	assert(!pthread_mutex_init(&lock->mutex, NULL));
}

static inline void arch_spin_lock(spinlock_t *lock)
{
	pthread_mutex_lock(&lock->mutex);
}

static inline int arch_try_lock(spinlock_t *lock)
{
	/* try lock logic on SOF is opposite of posix ! */
	return !pthread_mutex_trylock(&lock->mutex);
}

static inline void arch_spin_unlock(spinlock_t *lock)
{
	pthread_mutex_unlock(&lock->mutex);
}

#endif /* __ARCH_SPINLOCK_H__ */

#else

#error "This file shouldn't be included from outside of sof/spinlock.h"

#endif /* __SOF_SPINLOCK_H__ */
