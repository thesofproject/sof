/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/* TODO: this needs fixed as part of the "host does not need rtos headers work" */
#ifdef __XTOS_RTOS_SPINLOCK_H__

#ifndef __ARCH_SPINLOCK_H__
#define __ARCH_SPINLOCK_H__

struct k_spinlock {
};

static inline void arch_spinlock_init(struct k_spinlock *lock) {}
static inline void arch_spin_lock(struct k_spinlock *lock) {}
static inline void arch_spin_unlock(struct k_spinlock *lock) {}

#endif /* __ARCH_SPINLOCK_H__ */

#else

#error "This file shouldn't be included from outside of sof/spinlock.h"

#endif /* __SOF_SPINLOCK_H__ */
