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

#ifndef __SOF_SPINLOCK_H__
#define __SOF_SPINLOCK_H__

#include <arch/spinlock.h>
#include <sof/drivers/interrupt.h>
#include <config.h>

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
 * is the line number of where the lock was acquired. The second number is the
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
 * Every lock entry and exit shows LcE and LcX in trace alongside the lock
 * line numbers in hex. e.g.
 *
 * 0xfd60 [11032.730567]	delta [0.000004]	lock LcE
 * 0xfd70 [11032.730569]	delta [0.000002]	value 0x00000000000000ae
 *
 * Deadlock can be confirmed in rmbox :-
 *
 * Debug log:
 * debug: 0x0 (00) =	0xdead0007	(-559087609)	|....|
 *  ....
 * Error log:
 * using 19.20MHz timestamp clock
 * 0xc30 [26.247240]	delta [26.245851]	lock DED
 * 0xc40 [26.247242]	delta [0.000002]	value 0x00000000000002b4
 * 0xc50 [26.247244]	delta [0.000002]	value 0x0000000000000109
 *
 * DED means deadlock has been detected and the DSP is now halted. The first
 * value after DEA is the line number where deadlock occurs and the second
 * number is the line number where the lock is allocated. These can be grepped
 * like above.
 */

#if CONFIG_DEBUG_LOCKS

#include <sof/debug/panic.h>
#include <sof/trace/trace.h>
#include <ipc/trace.h>
#include <user/trace.h>
#include <stdint.h>

#define DBG_LOCK_USERS		8
#define DBG_LOCK_TRIES		10000

#define trace_lock(__e)		trace_error_atomic(TRACE_CLASS_LOCK, __e)
#define tracev_lock(__e)	tracev_event_atomic(TRACE_CLASS_LOCK, __e)
#define trace_lock_error(__e)	trace_error_atomic(TRACE_CLASS_LOCK, __e)
#define trace_lock_value(__e)	_trace_error_atomic(__e)

extern uint32_t lock_dbg_atomic;
extern uint32_t lock_dbg_user[DBG_LOCK_USERS];

/* all SMP spinlocks need init, nothing todo on UP */
#define spinlock_init(lock) \
	do { \
		arch_spinlock_init(lock); \
		(lock)->user = __LINE__; \
	} while (0)

/* panic on deadlock */
#define spin_try_lock_dbg(lock) \
	do { \
		int __tries; \
		for (__tries = DBG_LOCK_TRIES;  __tries > 0; __tries--) { \
			if (arch_try_lock(lock)) \
				break;	/* lock acquired */ \
		} \
		if (__tries == 0) { \
			trace_lock_error("DED"); \
			trace_lock_value(__LINE__); \
			trace_lock_value((lock)->user); \
			panic(SOF_IPC_PANIC_DEADLOCK); /* lock not acquired */ \
		} \
	} while (0)

#if CONFIG_DEBUG_LOCKS_VERBOSE
#define spin_lock_log(lock) \
	do { \
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
	} while (0)

#define spin_lock_dbg() \
	do { \
		trace_lock("LcE"); \
		trace_lock_value(__LINE__); \
	} while (0)

#define spin_unlock_dbg() \
	do { \
		trace_lock("LcX"); \
		trace_lock_value(__LINE__); \
	} while (0)

#else
#define spin_lock_log(lock)
#define spin_lock_dbg()
#define spin_unlock_dbg()
#endif

/* does nothing on UP systems */
#define spin_lock(lock) \
	do { \
		spin_lock_dbg(); \
		spin_lock_log(lock); \
		spin_try_lock_dbg(lock); \
	} while (0)

#define spin_unlock(lock) \
	do { \
		arch_spin_unlock(lock); \
		spin_unlock_dbg(); \
	} while (0)

/* disables all IRQ sources and takes lock - enter atomic context */
#define spin_lock_irq(lock, flags) \
	do { \
		flags = interrupt_global_disable(); \
		lock_dbg_atomic++; \
		spin_lock(lock); \
		if (lock_dbg_atomic < DBG_LOCK_USERS) \
			lock_dbg_user[lock_dbg_atomic - 1] = (lock)->user; \
	} while (0)

/* re-enables current IRQ sources and releases lock - leave atomic context */
#define spin_unlock_irq(lock, flags) \
	do { \
		spin_unlock(lock); \
		lock_dbg_atomic--; \
		interrupt_global_enable(flags); \
	} while (0)

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
	do { \
		spin_lock_dbg(); \
		arch_spin_lock(lock); \
	} while (0)

#define spin_try_lock(lock, ret) \
	do { \
		spin_lock_dbg(); \
		ret = arch_try_lock(lock); \
	} while (0)

#define spin_unlock(lock) \
	do { \
		arch_spin_unlock(lock); \
		spin_unlock_dbg(); \
	} while (0)

/* disables all IRQ sources and takes lock - enter atomic context */
#define spin_lock_irq(lock, flags) \
	do { \
		flags = interrupt_global_disable(); \
		spin_lock(lock); \
	} while (0)

/* re-enables current IRQ sources and releases lock - leave atomic context */
#define spin_unlock_irq(lock, flags) \
	do { \
		spin_unlock(lock); \
		interrupt_global_enable(flags); \
	} while (0)

#endif

#endif /* __SOF_SPINLOCK_H__ */
