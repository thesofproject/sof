/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_ATOMIC_H__

#ifndef __ARCH_ATOMIC_H__
#define __ARCH_ATOMIC_H__

#include <stdint.h>
#if XCHAL_HAVE_EXCLUSIVE && CONFIG_XTENSA_EXCLUSIVE && __XCC__
#include <xtensa/tie/xt_core.h>
#endif
#include <xtensa/config/core-isa.h>

typedef struct {
	volatile int32_t value;
} atomic_t;

static inline int32_t arch_atomic_read(const atomic_t *a)
{
	return (*(volatile int32_t *)&a->value);
}

static inline void arch_atomic_set(atomic_t *a, int32_t value)
{
	a->value = value;
}

static inline void arch_atomic_init(atomic_t *a, int32_t value)
{
	arch_atomic_set(a, value);
}

#if XCHAL_HAVE_EXCLUSIVE && CONFIG_XTENSA_EXCLUSIVE && __XCC__

/* Use exclusive instructions */
static inline int32_t arch_atomic_add(atomic_t *a, int32_t value)
{
	/*reference xtos : xipc_misc.h*/
	int32_t result = 0;
	int32_t current;

	while (!result) {
		current = XT_L32EX((int32_t *)a);
		result = current + value;
		XT_S32EX(result, (int32_t *)a);
		XT_GETEX(result);
	}
	return current;
}

static inline int32_t arch_atomic_sub(atomic_t *a, int32_t value)
{
	/*reference xtos : xipc_misc.h*/
	int32_t current;
	int32_t result = 0;

	while (!result) {
		current = XT_L32EX((int *)a);
		result = current - value;
		XT_S32EX(result, (int *)a);
		XT_GETEX(result);
	}
	return current;
}

#elif XCHAL_HAVE_S32C1I

/* Use S32C1I instructions */
static inline int32_t arch_atomic_add(atomic_t *a, int32_t value)
{
	int32_t result, current;

	__asm__ __volatile__(
		"1:     l32i    %1, %2, 0\n"
		"       wsr     %1, scompare1\n"
		"       add     %0, %1, %3\n"
		"       s32c1i  %0, %2, 0\n"
		"       bne     %0, %1, 1b\n"
		: "=&a" (result), "=&a" (current)
		: "a" (&a->value), "a" (value)
		: "memory");

	return current;
}

static inline int32_t arch_atomic_sub(atomic_t *a, int32_t value)
{
	int32_t result, current;

	__asm__ __volatile__(
		"1:     l32i    %1, %2, 0\n"
		"       wsr     %1, scompare1\n"
		"       sub     %0, %1, %3\n"
		"       s32c1i  %0, %2, 0\n"
		"       bne     %0, %1, 1b\n"
		: "=&a" (result), "=&a" (current)
		: "a" (&a->value), "a" (value)
		: "memory");

	return current;
}

#else

#if CONFIG_CORE_COUNT > 1

#error No atomic ISA for SMP configuration

#endif

/*
 * The ISA has no atomic operations so use integer arithmetic on uniprocessor systems.
 * This helps support GCC and qemu emulation of certain targets.
 */

/* integer arithmetic methods */
static inline int32_t arch_atomic_add(atomic_t *a, int32_t value)
{
	int32_t result, current;

	current = arch_atomic_read(a);
	result = current + value;
	arch_atomic_set(a, result);
	return current;
}

static inline int32_t arch_atomic_sub(atomic_t *a, int32_t value)
{
	int32_t result, current;

	current = arch_atomic_read(a);
	result = current - value;
	arch_atomic_set(a, result);
	return current;
}

#endif /* XCHAL_HAVE_EXCLUSIVE && CONFIG_XTENSA_EXCLUSIVE && __XCC__ */

#endif /* __ARCH_ATOMIC_H__ */

#else

#error "This file shouldn't be included from outside of sof/atomic.h"

#endif /* __SOF_ATOMIC_H__ */
