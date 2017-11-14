/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *
 * Simple spinlock implementation for Reef.
 */

#ifndef __INCLUDE_LOCK__
#define __INCLUDE_LOCK__

#define DEBUG_LOCKS	0

#include <stdint.h>
#include <arch/spinlock.h>
#include <reef/interrupt.h>
#include <reef/trace.h>

/*
 * Lock debugging provides a simple interface to debug deadlocks. The rmbox
 * trace output will show an output :-
 *
 * 0xd70 [41.306406]	delta [0.359638]	lock eal
 * 0xd80 [41.306409]	delta [0.000002]	value 0x00000000000001b7
 * 0xd90 [41.306411]	delta [0.000002]	value 0x0000000000000001
 * 0xda0 [41.306413]	delta [0.000002]	value 0x0000000001000348
 *
 * "eal" indicates we are holding a lock with interrupts OFF. The next value
 * is the line number of where the lock was aquired. The second number is the
 * number of other locks held whilst this lock is held and the subsequent
 * numbers list each lock and the line number of it's holder. e.g. to find
 * the locks :-
 *
 * grep -rn lock --include *.c | grep 840   (search for lock at line 0x348)
 *     src/drivers/dw-dma.c:840:	spinlock_init(&dma->lock);
 *
 * grep -rn lock --include *.c | grep 439
 *     src/lib/alloc.c:439:	spin_lock_irq(&memmap.lock, flags);
 *
 * Every lock entry and exit shows LcE and LcX in trace alonside the lock
 * line numbers in hex. e.g.
 *
 * 0xfd60 [11032.730567]	delta [0.000004]	lock LcE
 * 0xfd70 [11032.730569]	delta [0.000002]	value 0x00000000000000ae
 *
 * Deadlock would be a LcE without a subsequent LcX.
 *
 */

#if DEBUG_LOCKS

#define DBG_LOCK_USERS		8

#define trace_lock(__e)		trace_error_atomic(TRACE_CLASS_LOCK, __e)
#define tracev_lock(__e)	tracev_event_atomic(TRACE_CLASS_LOCK, __e)
#define trace_lock_error(__e)	trace_error_atomic(TRACE_CLASS_LOCK, __e)
#define trace_lock_value(__e)	_trace_error_atomic(__e)

extern uint32_t lock_dbg_atomic;
extern uint32_t lock_dbg_user[DBG_LOCK_USERS];

#define spin_lock_dbg() \
	trace_lock("LcE"); \
	trace_lock_value(__LINE__);

#define spin_unlock_dbg() \
	trace_lock("LcX"); \
	trace_lock_value(__LINE__); \

/* all SMP spinlocks need init, nothing todo on UP */
#define spinlock_init(lock) \
	arch_spinlock_init(lock); \
	(lock)->user = __LINE__;

/* does nothing on UP systems */
#define spin_lock(lock) \
	spin_lock_dbg(); \
	if (lock_dbg_atomic) { \
		int __i = 0; \
		int  __count = lock_dbg_atomic >= DBG_LOCK_USERS \
			? DBG_LOCK_USERS : lock_dbg_atomic; \
		trace_lock_error("eal"); \
		trace_lock_value(__LINE__); \
		trace_lock_value(lock_dbg_atomic); \
		for (__i = 0; __i < __count; __i++) { \
			trace_lock_value((lock_dbg_atomic << 24) | \
				lock_dbg_user[__i]); \
		} \
	} \
	arch_spin_lock(lock);

#define spin_unlock(lock) \
	arch_spin_unlock(lock); \
	spin_unlock_dbg();

/* disables all IRQ sources and takes lock - enter atomic context */
#define spin_lock_irq(lock, flags) \
	flags = interrupt_global_disable(); \
	lock_dbg_atomic++; \
	spin_lock(lock); \
	if (lock_dbg_atomic < DBG_LOCK_USERS) \
		lock_dbg_user[lock_dbg_atomic - 1] = (lock)->user;

/* re-enables current IRQ sources and releases lock - leave atomic context */
#define spin_unlock_irq(lock, flags) \
	spin_unlock(lock); \
	lock_dbg_atomic--; \
	interrupt_global_enable(flags);

#else

#define trace_lock(__e)
#define tracev_lock(__e)

#define spin_lock_dbg()
#define spin_unlock_dbg()

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

/* disables all IRQ sources and takes lock - enter atomic context */
#define spin_lock_irq(lock, flags) \
	flags = interrupt_global_disable(); \
	spin_lock(lock);

/* re-enables current IRQ sources and releases lock - leave atomic context */
#define spin_unlock_irq(lock, flags) \
	spin_unlock(lock); \
	interrupt_global_enable(flags);

#endif

#endif
