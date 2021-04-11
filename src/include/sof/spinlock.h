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
#include <sof/lib/memory.h>
#include <ipc/trace.h>

#include <stdint.h>

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

#define DBG_LOCK_USERS		8
#define DBG_LOCK_TRIES		10000

extern uint32_t lock_dbg_atomic;
extern uint32_t lock_dbg_user[DBG_LOCK_USERS];

extern struct tr_ctx sl_tr;

/* panic on deadlock */
#define spin_try_lock_dbg(lock, line) \
	do { \
		int __tries; \
		for (__tries = DBG_LOCK_TRIES;  __tries > 0; __tries--) { \
			if (arch_try_lock(lock)) \
				break;	/* lock acquired */ \
		} \
		if (__tries == 0) { \
			tr_err_atomic(&sl_tr, "DED"); \
			tr_err_atomic(&sl_tr, "line: %d", line); \
			tr_err_atomic(&sl_tr, "user: %d", (lock)->user); \
			panic(SOF_IPC_PANIC_DEADLOCK); /* lock not acquired */ \
		} \
	} while (0)

#if CONFIG_DEBUG_LOCKS_VERBOSE
#define spin_lock_log(lock, line) \
	do { \
		if (lock_dbg_atomic) { \
			int __i = 0; \
			int  __count = lock_dbg_atomic >= DBG_LOCK_USERS \
				? DBG_LOCK_USERS : lock_dbg_atomic; \
			tr_err_atomic(&sl_tr, "eal"); \
			tr_err_atomic(&sl_tr, "line: %d", line); \
			tr_err_atomic(&sl_tr, "dbg_atomic: %d", lock_dbg_atomic); \
			for (__i = 0; __i < __count; __i++) { \
				tr_err_atomic(&sl_tr, "value: %d", \
					      (lock_dbg_atomic << 24) | \
					      lock_dbg_user[__i]); \
			} \
		} \
	} while (0)

#define spin_lock_dbg(line) \
	do { \
		tr_info(&sl_tr, "LcE"); \
		tr_info(&sl_tr, "line: %d", line); \
	} while (0)

#define spin_unlock_dbg(line) \
	do { \
		tr_info(&sl_tr, "LcX"); \
		tr_info(&sl_tr, "line: %d", line); \
	} while (0)

#else  /* CONFIG_DEBUG_LOCKS_VERBOSE */
#define spin_lock_log(lock, line) do {} while (0)
#define spin_lock_dbg(line) do {} while (0)
#define spin_unlock_dbg(line) do {} while (0)
#endif /* CONFIG_DEBUG_LOCKS_VERBOSE */

#else  /* CONFIG_DEBUG_LOCKS */

#define trace_lock(__e) do {} while (0)
#define tracev_lock(__e) do {} while (0)

#define spin_lock_dbg(line) do {} while (0)
#define spin_unlock_dbg(line) do {} while (0)

#endif /* CONFIG_DEBUG_LOCKS */

static inline int _spin_try_lock(spinlock_t *lock, int line)
{
	spin_lock_dbg(line);
	return arch_try_lock(lock);
}

#define spin_try_lock(lock) _spin_try_lock(lock, __LINE__)

/* all SMP spinlocks need init, nothing todo on UP */
static inline void _spinlock_init(spinlock_t *lock, int line)
{
	arch_spinlock_init(lock);
#if CONFIG_DEBUG_LOCKS
	lock->user = line;
#endif
}

#define spinlock_init(lock) _spinlock_init(lock, __LINE__)

/* does nothing on UP systems */
static inline void _spin_lock(spinlock_t *lock, int line)
{
	spin_lock_dbg(line);
#if CONFIG_DEBUG_LOCKS
	spin_lock_log(lock, line);
	spin_try_lock_dbg(lock, line);
#else
	arch_spin_lock(lock);
#endif

	/* spinlock has to be in a shared memory */
}

#define spin_lock(lock) _spin_lock(lock, __LINE__)

/* disables all IRQ sources and takes lock - enter atomic context */
uint32_t _spin_lock_irq(spinlock_t *lock);

#define spin_lock_irq(lock, flags) (flags = _spin_lock_irq(lock))

static inline void _spin_unlock(spinlock_t *lock, int line)
{
	arch_spin_unlock(lock);
#if CONFIG_DEBUG_LOCKS
	spin_unlock_dbg(line);
#endif

	/* spinlock has to be in a shared memory */
}

#define spin_unlock(lock) _spin_unlock(lock, __LINE__)

/* re-enables current IRQ sources and releases lock - leave atomic context */
void _spin_unlock_irq(spinlock_t *lock, uint32_t flags, int line);

#define spin_unlock_irq(lock, flags) _spin_unlock_irq(lock, flags, __LINE__)

#endif /* __SOF_SPINLOCK_H__ */
