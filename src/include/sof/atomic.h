/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_ATOMIC_H_
#define __INCLUDE_ATOMIC_H_

#include <stdint.h>
#include <arch/atomic.h>

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

#endif
