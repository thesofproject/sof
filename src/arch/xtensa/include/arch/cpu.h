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
 * Author: Rander Wang <rander.wang@linux.intel.com>
 *
 */

#ifndef __INCLUDE_ARCH_CPU__
#define __INCLUDE_ARCH_CPU__

#include <xtensa/config/core.h>
#include <platform/platcfg.h>

void arch_cpu_enable_core(int id);

void arch_cpu_disable_core(int id);

static inline int arch_cpu_get_id(void)
{
	int prid;
#if XCHAL_HAVE_PRID
	__asm__("rsr.prid %0" : "=a"(prid));
#else
	prid = PLATFORM_MASTER_CORE_ID;
#endif
	return prid;
}

static inline void cpu_write_threadptr(int threadptr)
{
#if XCHAL_HAVE_THREADPTR
	__asm__ __volatile__(
		"wur.threadptr %0" : : "a" (threadptr) : "memory");
#else
#error "Core support for XCHAL_HAVE_THREADPTR is required"
#endif
}

static inline int cpu_read_threadptr(void)
{
	int threadptr;
#if XCHAL_HAVE_THREADPTR
	__asm__ __volatile__(
		"rur.threadptr %0" : "=a"(threadptr));
#else
#error "Core support for XCHAL_HAVE_THREADPTR is required"
#endif
	return threadptr;
}

#endif
