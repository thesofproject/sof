/*
 * Copyright (c) 2018, Intel Corporation
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
 *	   Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 *
 */

#ifndef __ARCH_ATOMIC_H_
#define __ARCH_ATOMIC_H_

#include <stdint.h>
#include <errno.h>

typedef struct {
	volatile int32_t value;
} atomic_t;

static inline int32_t arch_atomic_read(const atomic_t *a)
{
	return (*(volatile int32_t *)&a->value);
}

static inline void arch_atomic_set(atomic_t *a, int32_t value)
{
	a->value = value;
}

static inline void arch_atomic_init(atomic_t *a, int32_t value)
{
	arch_atomic_set(a, value);
}

/* use gcc atomic built-ins for host library */
static inline int32_t arch_atomic_add(atomic_t *a, int32_t value)
{
	return __sync_fetch_and_add(&a->value, value);
}

static inline int32_t arch_atomic_sub(atomic_t *a, int32_t value)
{
	return __sync_fetch_and_sub(&a->value, value);
}

#endif
