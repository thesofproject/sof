/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __XTOS_RTOS_SPINLOCK_H__

#ifndef __ARCH_SPINLOCK_H__
#define __ARCH_SPINLOCK_H__

#include <stdint.h>
#include <xtensa/config/core-isa.h>

struct k_spinlock {
	volatile uint32_t lock;
#if CONFIG_DEBUG_LOCKS
	uint32_t user;
#endif
};

static inline void arch_spinlock_init(struct k_spinlock *lock)
{
	lock->lock = 0;
}

#if XCHAL_HAVE_EXCLUSIVE && CONFIG_XTENSA_EXCLUSIVE && __XCC__

static inline void arch_spin_lock(struct k_spinlock *lock)
{
	uint32_t result;

	__asm__ __volatile__(
		"       movi %0, 0\n"
		"       l32ex %0, %1\n"
		"1:     movi %0, 1\n"
		"       s32ex %0, %1\n"
		"       getex %0\n"
		"       bnez %0, 1b\n"
		: "=&a" (result)
		: "a" (&lock->lock)
		: "memory");
}

#elif XCHAL_HAVE_S32C1I

static inline void arch_spin_lock(struct k_spinlock *lock)
{
	uint32_t result;

	/* TODO: Should be platform specific, since on SMP platforms
	 * without uncached memory region we'll need additional cache
	 * invalidations in a loop
	 */
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

#else

#if CONFIG_CORE_COUNT > 1

#error No atomic ISA for SMP configuration

#endif /* CONFIG_CORE_COUNT > 1 */

/*
 * The ISA has no atomic operations so use integer arithmetic on uniprocessor systems.
 * This helps support GCC and qemu emulation of certain targets.
 */
static inline void arch_spin_lock(struct k_spinlock *lock)
{
	uint32_t result;

	do {
		if (lock->lock == 0) {
			lock->lock = 1;
			result = 1;
		}
	} while (!result);
}

#endif /* XCHAL_HAVE_EXCLUSIVE && CONFIG_XTENSA_EXCLUSIVE && __XCC__ */

#if XCHAL_HAVE_EXCLUSIVE || XCHAL_HAVE_S32C1I

static inline void arch_spin_unlock(struct k_spinlock *lock)
{
	uint32_t result;

	__asm__ __volatile__(
		"       movi    %0, 0\n"
		"       s32ri   %0, %1, 0\n"
		: "=&a" (result)
		: "a" (&lock->lock)
		: "memory");
}

#else

#if CONFIG_CORE_COUNT > 1

#error No atomic ISA for SMP configuration

#endif /* CONFIG_CORE_COUNT > 1 */

/*
 * The ISA has no atomic operations so use integer arithmetic on uniprocessor systems.
 * This helps support GCC and qemu emulation of certain targets.
 */
static inline void arch_spin_unlock(struct k_spinlock *lock)
{
	uint32_t result;

	lock->lock = 0;
	result = 1;
}

#endif /* XCHAL_HAVE_EXCLUSIVE || XCHAL_HAVE_S32C1I  */

#endif /* __ARCH_SPINLOCK_H__ */

#else

#error "This file shouldn't be included from outside of sof/spinlock.h"

#endif /* __SOF_SPINLOCK_H__ */
