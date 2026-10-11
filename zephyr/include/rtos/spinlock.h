// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//

/**
 * @file rtos/spinlock.h
 * @brief RTOS spinlock primitives and initialization.
 * @ingroup rtos_api
 */

#ifndef __ZEPHYR_RTOS_SPINLOCK_H__
#define __ZEPHYR_RTOS_SPINLOCK_H__

#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>

/**
 * @brief Initialize a spinlock structure to the unlocked state.
 * @param lock Pointer to the Zephyr k_spinlock structure.
 *
 * Clears the locked atomic flag on SMP systems. Not provided by upstream
 * Zephyr core, but required by SOF multi-core synchronization primitives.
 */
static inline void k_spinlock_init(struct k_spinlock *lock)
{
#ifdef CONFIG_SMP
	atomic_set(&lock->locked, 0);
#endif
}

#endif /* __ZEPHYR_RTOS_SPINLOCK_H__ */
