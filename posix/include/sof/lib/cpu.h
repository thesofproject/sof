/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file xtos/include/sof/lib/cpu.h
 * \brief CPU header file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __SOF_LIB_CPU_H__
#define __SOF_LIB_CPU_H__
#ifndef __ZEPHYR__

#include <platform/lib/cpu.h>

#if !defined(__ASSEMBLER__) && !defined(LINKER)

#include <arch/lib/cpu.h>
#include <stdbool.h>

/* let the compiler optimise when in single core mode */
#if CONFIG_CORE_COUNT == 1

static inline int cpu_get_id(void)
{
	return 0;
}

static inline bool cpu_is_primary(int id)
{
	return 1;
}

static inline bool cpu_is_me(int id)
{
	return 1;
}

#else

static inline int cpu_get_id(void)
{
	return arch_cpu_get_id();
}

static inline bool cpu_is_primary(int id)
{
	return id == PLATFORM_PRIMARY_CORE_ID;
}

static inline bool cpu_is_me(int id)
{
	return id == cpu_get_id();
}
#endif

static inline int cpu_enable_core(int id)
{
	return arch_cpu_enable_core(id);
}

static inline void cpu_disable_core(int id)
{
	arch_cpu_disable_core(id);
}

static inline int cpu_is_core_enabled(int id)
{
	return arch_cpu_is_core_enabled(id);
}

static inline int cpu_enabled_cores(void)
{
	return arch_cpu_enabled_cores();
}

static inline int cpu_restore_secondary_cores(void)
{
	return arch_cpu_restore_secondary_cores();
}

static inline int cpu_secondary_cores_prepare_d0ix(void)
{
	return arch_cpu_secondary_cores_prepare_d0ix();
}

#endif
#endif
#endif /* __SOF_LIB_CPU_H__ */
