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

#include <stdint.h>
#include <arch/spinlock.h>
#include <reef/interrupt.h>
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
