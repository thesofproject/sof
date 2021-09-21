/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Rander Wang <rander.wang@linux.intel.com>
 */

#ifdef __SOF_LIB_CPU_H__

#ifndef __ARCH_LIB_CPU_H__
#define __ARCH_LIB_CPU_H__

static inline int arch_cpu_enable_core(int id)
{
	return 0;
}

static inline void arch_cpu_disable_core(int id)
{
}

static inline int arch_cpu_is_core_enabled(int id)
{
	return 0;
}

static inline int arch_cpu_enabled_cores(void)
{
	return 1;
}

static inline int arch_cpu_get_id(void)
{
	return 0;
}

static inline int arch_cpu_restore_secondary_cores(void)
{
	return 0;
}

static inline void cpu_write_threadptr(int threadptr)
{
}

static inline int cpu_read_threadptr(void)
{
	return 0;
}

#endif /* __ARCH_LIB_CPU_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/cpu.h"

#endif /* __SOF_LIB_CPU_H__ */
