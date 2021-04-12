/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Rander Wang <rander.wang@linux.intel.com>
 */

#ifdef __SOF_LIB_CPU_H__

#ifndef __ARCH_LIB_CPU_H__
#define __ARCH_LIB_CPU_H__

#include <xtensa/config/core-isa.h>

#if CONFIG_MULTICORE

void cpu_power_down_core(void);

void cpu_alloc_core_context(int id);

int arch_cpu_enable_core(int id);

void arch_cpu_disable_core(int id);

int arch_cpu_is_core_enabled(int id);

int arch_cpu_enabled_cores(void);

#else

static inline int arch_cpu_enable_core(int id) { return 0; }

static inline void arch_cpu_disable_core(int id) { }

static inline int arch_cpu_is_core_enabled(int id) { return 1; }

static inline int arch_cpu_enabled_cores(void) { return 1; }

#endif

static inline int arch_cpu_get_id(void)
{
	int prid;
#if XCHAL_HAVE_PRID
	__asm__("rsr.prid %0" : "=a"(prid));
#else
	prid = PLATFORM_PRIMARY_CORE_ID;
#endif
	return prid;
}

#if !XCHAL_HAVE_THREADPTR
extern unsigned int _virtual_thread_start;
static unsigned int *virtual_thread_ptr =
	(unsigned int *)&_virtual_thread_start;
#endif

static inline void cpu_write_threadptr(int threadptr)
{
#if XCHAL_HAVE_THREADPTR
	__asm__ __volatile__(
		"wur.threadptr %0" : : "a" (threadptr) : "memory");
#else
	*virtual_thread_ptr = threadptr;
#endif
}

static inline int cpu_read_threadptr(void)
{
	int threadptr;
#if XCHAL_HAVE_THREADPTR
	__asm__ __volatile__(
		"rur.threadptr %0" : "=a"(threadptr));
#else
	threadptr = *virtual_thread_ptr;
#endif
	return threadptr;
}

static inline int cpu_read_vecbase(void)
{
	int vecbase;

	__asm__ __volatile__("rsr.vecbase %0"
		: "=a"(vecbase));
	return vecbase;
}

static inline int cpu_read_excsave2(void)
{
	int excsave2;

	__asm__ __volatile__("rsr.excsave2 %0"
		: "=a"(excsave2));
	return excsave2;
}

static inline int cpu_read_excsave3(void)
{
	int excsave3;

	__asm__ __volatile__("rsr.excsave3 %0"
		: "=a"(excsave3));
	return excsave3;
}

static inline int cpu_read_excsave4(void)
{
	int excsave4;

	__asm__ __volatile__("rsr.excsave4 %0"
		: "=a"(excsave4));
	return excsave4;
}

static inline int cpu_read_excsave5(void)
{
	int excsave5;

	__asm__ __volatile__("rsr.excsave5 %0"
		: "=a"(excsave5));
	return excsave5;
}

#endif /* __ARCH_LIB_CPU_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/cpu.h"

#endif /* __SOF_LIB_CPU_H__ */
