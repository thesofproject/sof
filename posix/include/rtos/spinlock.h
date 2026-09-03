/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/*
 * Simple spinlock implementation for SOF.
 */

#ifndef __POSIX_RTOS_SPINLOCK_H__
#define __POSIX_RTOS_SPINLOCK_H__

#include <stdint.h>

struct k_spinlock {
};

typedef uint32_t k_spinlock_key_t;

#define trace_lock(__e) do {} while (0)
#define tracev_lock(__e) do {} while (0)

#define spin_lock_dbg(line) do {} while (0)
#define spin_unlock_dbg(line) do {} while (0)

#define k_spinlock_init(lock) do {} while (0)

static inline k_spinlock_key_t k_spin_lock(struct k_spinlock *l)
{
	return 0;
}

static inline void k_spin_unlock(struct k_spinlock *l,
				 k_spinlock_key_t key)
{
	(void)l;
	(void)key;
}

#endif /* __POSIX_RTOS_SPINLOCK_H__ */
