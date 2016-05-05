/*
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 */

#ifndef __INCLUDE_LOCK__
#define __INCLUDE_LOCK__

#include <stdint.h>
#include <reef/interrupt.h>
#include <arch/spinlock.h>


/* uni-processor locking implementation using same syntax as Linux */
/* TODO: add multi-processor support */

/* all SMP spinlocks need init, nothing todo on UP */
#define spinlock_init(lock) \
	arch_spinlock_init(lock)

/* does nothing on UP systems */
#define spin_lock(lock) \
	arch_spin_lock(lock)

#define spin_unlock(lock) \
	arch_spin_unlock(lock)

/* disables single IRQ and takes lock */
#define spin_lock_local_irq(lock, irq) \
	interrupt_disable(irq); \
	spin_lock(lock);

/* enables single IRQ and releases lock */
#define spin_unlock_local_irq(lock, irq) \
	spin_unlock(lock); \
	interrupt_enable(irq);

/* disables all IRQ sources and tales lock - atomic context */
#define spin_lock_irq(lock) \
	interrupt_global_disable(); \
	spin_lock(lock);

/* re-enables current IRQ sources and releases lock */
#define spin_unlock_irq(lock) \
	spin_unlock(lock); \
	interrupt_global_enable();

#endif
