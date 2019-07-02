/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __ARCH_SPINLOCK_H_
#define __ARCH_SPINLOCK_H_

#include <stdint.h>
#include <errno.h>

typedef struct {
} spinlock_t;

static inline void arch_spinlock_init(spinlock_t *lock) {}
static inline void arch_spin_lock(spinlock_t *lock) {}
static inline void arch_spin_unlock(spinlock_t *lock) {}

#endif

