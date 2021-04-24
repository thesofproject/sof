/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __ZEPHYR__
#include <sys/atomic.h>
#else
#ifndef __SOF_ATOMIC_H__
#define __SOF_ATOMIC_H__

#include <arch/atomic.h>
#include <stdint.h>

static inline void atomic_init(atomic_t *a, int32_t value)
{
	arch_atomic_init(a, value);
}

static inline int32_t atomic_read(const atomic_t *a)
{
	return arch_atomic_read(a);
}

static inline void atomic_set(atomic_t *a, int32_t value)
{
	arch_atomic_set(a, value);
}

static inline int32_t atomic_add(atomic_t *a, int32_t value)
{
	return arch_atomic_add(a, value);
}

static inline int32_t atomic_sub(atomic_t *a, int32_t value)
{
	return arch_atomic_sub(a, value);
}

#endif /* __SOF_ATOMIC_H__ */

#endif /*__ZEPHYR__ */
