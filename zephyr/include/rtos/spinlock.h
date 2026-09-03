// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//

#ifndef __ZEPHYR_RTOS_SPINLOCK_H__
#define __ZEPHYR_RTOS_SPINLOCK_H__

#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>

/* not implemented on Zephyr, but used within SOF */
static inline void k_spinlock_init(struct k_spinlock *lock)
{
#ifdef CONFIG_SMP
	atomic_set(&lock->locked, 0);
#endif
}

#endif /* __ZEPHYR_RTOS_SPINLOCK_H__ */
