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
#include <reef/trace.h>

#define DEBUG_LOCKS	0

#if DEBUG_LOCKS

#define trace_lock(__e)		trace_event(TRACE_CLASS_LOCK, __e)
#define tracev_lock(__e)	tracev_event(TRACE_CLASS_LOCK, __e)

#define spin_lock_dbg() \
	trace_lock("LcE"); \
	trace_value(__LINE__);
#define spin_unlock_dbg() \
	trace_lock("LcX");

#else

#define trace_lock(__e)
#define tracev_lock(__e)

#define spin_lock_dbg()
#define spin_unlock_dbg()

#endif

/* uni-processor locking implementation using same syntax as Linux */
/* TODO: add multi-processor support */

/* all SMP spinlocks need init, nothing todo on UP */
#define spinlock_init(lock) \
	arch_spinlock_init(lock)

/* does nothing on UP systems */
#define spin_lock(lock) \
	spin_lock_dbg(); \
	arch_spin_lock(lock);

#define spin_unlock(lock) \
	arch_spin_unlock(lock); \
	spin_unlock_dbg();

/* disables all IRQ sources and tales lock - atomic context */
#define spin_lock_irq(lock) \
	interrupt_global_disable(); \
	spin_lock(lock);

/* re-enables current IRQ sources and releases lock */
#define spin_unlock_irq(lock) \
	spin_unlock(lock); \
	interrupt_global_enable();

#endif
