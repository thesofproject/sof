/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_SPINLOCK_H__

#ifndef __ARCH_SPINLOCK_H__
#define __ARCH_SPINLOCK_H__

#include <config.h>
#include <stdint.h>

typedef struct {
	volatile uint32_t lock;
#if CONFIG_DEBUG_LOCKS
	uint32_t user;
#endif
} spinlock_t;

static inline void arch_spinlock_init(spinlock_t *lock)
{
	lock->lock = 0;
}

static inline void arch_spin_lock(spinlock_t *lock)
{
	uint32_t result, current;

	__asm__ __volatile__(
		"1:     l32i    %1, %2, 0\n"
		"       wsr     %1, scompare1\n"
		"       movi    %0, 1\n"
		"       s32c1i  %0, %2, 0\n"
		"       bne     %0, %1, 1b\n"
		: "=&a" (result), "=&a" (current)
		: "a" (&lock->lock)
		: "memory");
}

static inline int arch_try_lock(spinlock_t *lock)
{
	uint32_t result, current;

	__asm__ __volatile__(
		"       l32i    %1, %2, 0\n"
		"       wsr     %1, scompare1\n"
		"       movi    %0, 1\n"
		"       s32c1i  %0, %2, 0\n"
		: "=&a" (result), "=&a" (current)
		: "a" (&lock->lock)
		: "memory");

	if (result)
		return 0; /* lock failed */
	return 1; /* lock acquired */
}

static inline void arch_spin_unlock(spinlock_t *lock)
{
	uint32_t result;

	__asm__ __volatile__(
		"       movi    %0, 0\n"
		"       s32ri   %0, %1, 0\n"
		: "=&a" (result)
		: "a" (&lock->lock)
		: "memory");
}

#endif /* __ARCH_SPINLOCK_H__ */

#else

#error "This file shouldn't be included from outside of sof/spinlock.h"

#endif /* __SOF_SPINLOCK_H__ */
