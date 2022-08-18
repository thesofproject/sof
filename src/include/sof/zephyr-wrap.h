/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Temporary wrapper for stuff that is NOT yet upstream in Zephyr.
 * Will go away at some point.
 */

#ifndef __SOF_ZEPHYR_WRAP_H__
#define __SOF_ZEPHYR_WRAP_H__

#include <zephyr/kernel.h>

/* This has to be moved to Zephyr */
static inline void k_spinlock_init(struct k_spinlock *lock)
{
#ifdef CONFIG_SMP
	atomic_set(&lock->locked, 0);
#endif
}

#endif /* __SOF_ZEPHYR_WRAP_H__ */
