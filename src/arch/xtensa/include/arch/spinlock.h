/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * Xtensa architecture initialisation.
 */

#ifndef __ARCH_SPINLOCK_H_
#define __ARCH_SPINLOCK_H_

#include <stdint.h>
#include <errno.h>

typedef struct {
	volatile uint32_t lock;
} spinlock_t;

static inline void arch_spinlock_init(spinlock_t *lock)
{
	lock->lock = 0;
}

static inline void arch_spin_lock(spinlock_t *lock)
{
	uint32_t result;

	__asm__ __volatile__(
		"       movi    %0, 0\n"
		"       wsr     %0, scompare1\n"
		"1:     movi    %0, 1\n"
		"       s32c1i  %0, %1, 0\n"
		"       bnez    %0, 1b\n"
		: "=&a" (result)
		: "a" (&lock->lock)
		: "memory");
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

#endif
