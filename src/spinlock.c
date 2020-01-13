// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <sof/drivers/interrupt.h>
#include <sof/spinlock.h>
#include <config.h>
#include <stdint.h>

uint32_t _spin_lock_irq(spinlock_t *lock)
{
	uint32_t flags;

	flags = interrupt_global_disable();
#if CONFIG_DEBUG_LOCKS
	lock_dbg_atomic++;
#endif
	spin_lock(lock);
#if CONFIG_DEBUG_LOCKS
	if (lock_dbg_atomic < DBG_LOCK_USERS)
		lock_dbg_user[lock_dbg_atomic - 1] = (lock)->user;
#endif
	return flags;
}

void _spin_unlock_irq(spinlock_t *lock, uint32_t flags, int line)
{
	_spin_unlock(lock, line);
#if CONFIG_DEBUG_LOCKS
	lock_dbg_atomic--;
#endif
	interrupt_global_enable(flags);
}
