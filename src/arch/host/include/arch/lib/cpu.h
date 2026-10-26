/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Rander Wang <rander.wang@linux.intel.com>
 */

#ifdef __SOF_LIB_CPU_H__

#ifndef __ARCH_LIB_CPU_H__
#define __ARCH_LIB_CPU_H__

int __attribute__((weak)) host_cpu_num_cores(void);
int __attribute__((weak)) host_cpu_enable_core(int id);
void __attribute__((weak)) host_cpu_disable_core(int id);
int __attribute__((weak)) host_cpu_is_core_enabled(int id);
int __attribute__((weak)) host_cpu_enabled_cores(void);

static inline int arch_cpu_num_cores(void)
{
	if (host_cpu_num_cores)
		return host_cpu_num_cores();
#ifdef CONFIG_CORE_COUNT
	return CONFIG_CORE_COUNT;
#else
	return 1;
#endif
}

static inline int arch_cpu_enable_core(int id)
{
	if (host_cpu_enable_core)
		return host_cpu_enable_core(id);
	return 0;
}

static inline void arch_cpu_disable_core(int id)
{
	if (host_cpu_disable_core)
		host_cpu_disable_core(id);
}

static inline int arch_cpu_is_core_enabled(int id)
{
	if (host_cpu_is_core_enabled)
		return host_cpu_is_core_enabled(id);
	return 1;
}

static inline int arch_cpu_enabled_cores(void)
{
	if (host_cpu_enabled_cores)
		return host_cpu_enabled_cores();
#ifdef CONFIG_CORE_COUNT
	return (1 << CONFIG_CORE_COUNT) - 1;
#else
	return 1;
#endif
}

int arch_cpu_get_id(void);
void host_cpu_set_id(int id);

static inline int arch_cpu_restore_secondary_cores(void)
{
	return 0;
}

static inline int arch_cpu_secondary_cores_prepare_d0ix(void)
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
