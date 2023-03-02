/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 */

#ifdef __SOF_ATOMIC_H__

#ifndef __ARCH_ATOMIC_H__
#define __ARCH_ATOMIC_H__

#include <stdint.h>

typedef struct {
	volatile int32_t value;
} atomic_t;

#if defined __GNUC__ && !defined __XCC__

static inline int32_t arch_atomic_read(const atomic_t *a)
{
	return __atomic_load_n(&a->value, __ATOMIC_SEQ_CST);
}

static inline void arch_atomic_set(atomic_t *a, int32_t value)
{
	__atomic_store_n(&a->value, value, __ATOMIC_SEQ_CST);
}

static inline void arch_atomic_init(atomic_t *a, int32_t value)
{
	__atomic_store_n(&a->value, value, __ATOMIC_SEQ_CST);
}

static inline int32_t arch_atomic_add(atomic_t *a, int32_t value)
{
	return __atomic_add_fetch(&a->value, value, __ATOMIC_SEQ_CST);
}

static inline int32_t arch_atomic_sub(atomic_t *a, int32_t value)
{
	return __atomic_sub_fetch(&a->value, value, __ATOMIC_SEQ_CST);
}

#elif defined __XCC__

/* fake locking: obviously this does nothing, and provides no real locking */
#define lock() do {} while (0)
#define unlock() do {} while (0)

/* Fake atomic functions for xt-run static single-thread testbench */

static inline int32_t arch_atomic_read(const atomic_t *a)
{
	return a->value;
}

static inline void arch_atomic_set(atomic_t *a, int32_t value)
{
	a->value = value;
}

static inline void arch_atomic_init(atomic_t *a, int32_t value)
{
	arch_atomic_set(a, value);
}

static inline int32_t arch_atomic_add(atomic_t *a, int32_t value)
{
	int32_t tmp;

	lock();
	tmp = a->value;
	a->value += value;
	unlock();
	return tmp;
}

static inline int32_t arch_atomic_sub(atomic_t *a, int32_t value)
{
	int32_t tmp;

	lock();
	tmp = a->value;
	a->value -= value;
	unlock();
	return tmp;
}

#else

#error "Not gcc, not xt-xcc"

#endif

#endif /* __ARCH_ATOMIC_H__ */

#else

#error "This file shouldn't be included from outside of sof/atomic.h"

#endif /* __SOF_ATOMIC_H__ */
