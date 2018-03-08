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
 *
 */

#ifndef __ARCH_SPINLOCK_H_
#define __ARCH_SPINLOCK_H_

#include <stdint.h>
#include <errno.h>

typedef struct {
	volatile uint32_t lock;
#if DEBUG_LOCKS
	uint32_t user;
#endif
} spinlock_t;

static inline void arch_spinlock_init(spinlock_t *lock)
{
	lock->lock = 0;
}

static inline void arch_spin_lock(spinlock_t *lock)
{
	uint32_t result;

	__asm__ __volatile__(
		"       movi    %0, 0\n"
		"       wsr     %0, scompare1\n"
		"1:     movi    %0, 1\n"
		"       s32c1i  %0, %1, 0\n"
		"       bnez    %0, 1b\n"
		: "=&a" (result)
		: "a" (&lock->lock)
		: "memory");
}

static inline int arch_try_lock(spinlock_t *lock)
{
	uint32_t result;

	__asm__ __volatile__(
		"       movi    %0, 0\n"
		"       wsr     %0, scompare1\n"
		"       movi    %0, 1\n"
		"       s32c1i  %0, %1, 0\n"
		: "=&a" (result)
		: "a" (&lock->lock)
		: "memory");

	if (result)
		return 0; /* lock failed */
	return 1; /* lock acquired */
}

static inline void arch_spin_unlock(spinlock_t *lock)
{
	uint32_t result;

	__asm__ __volatile__(
		"       movi    %0, 0\n"
		"       s32ri   %0, %1, 0\n"
		: "=&a" (result)
		: "a" (&lock->lock)
		: "memory");
}

#endif
