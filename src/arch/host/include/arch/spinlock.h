/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_SPINLOCK_H__

#ifndef __ARCH_SPINLOCK_H__
#define __ARCH_SPINLOCK_H__

struct spinlock {
};

static inline void arch_spinlock_init(struct spinlock **lock) {}
static inline void arch_spin_lock(struct spinlock *lock) {}
static inline int arch_try_lock(struct spinlock *lock)
{
	return 1;
}
static inline void arch_spin_unlock(struct spinlock *lock) {}

#endif /* __ARCH_SPINLOCK_H__ */

#else

#error "This file shouldn't be included from outside of sof/spinlock.h"

#endif /* __SOF_SPINLOCK_H__ */
