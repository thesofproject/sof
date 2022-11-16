/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Leman <tomasz.m.leman@intel.com>
 */

/**
 * \file zephyr/include/sof/lib/cpu.h
 * \brief CPU header file
 * \authors Tomasz Leman <tomasz.m.leman@intel.com>
 */

#ifndef __SOF_LIB_CPU_H__
#define __SOF_LIB_CPU_H__

#include <platform/lib/cpu.h>

#if !defined(__ASSEMBLER__) && !defined(LINKER)

#include <stdbool.h>

#include <zephyr/arch/arch_inlines.h>

#if CONFIG_PM

#include <zephyr/pm/pm.h>

void cpu_notify_state_entry(enum pm_state state);

void cpu_notify_state_exit(enum pm_state state);

#endif /* CONFIG_PM */

/* let the compiler optimise when in single core mode */
#if CONFIG_MULTICORE && CONFIG_SMP

static inline int cpu_get_id(void)
{
	return arch_proc_id();
}

static inline bool cpu_is_primary(int id)
{
	return id == PLATFORM_PRIMARY_CORE_ID;
}

static inline bool cpu_is_me(int id)
{
	return id == cpu_get_id();
}

int cpu_enable_core(int id);

void cpu_disable_core(int id);

int cpu_is_core_enabled(int id);

int cpu_enabled_cores(void);

void cpu_power_down_core(uint32_t flags);

int cpu_restore_secondary_cores(void);

int cpu_secondary_cores_prepare_d0ix(void);
#else

static inline int cpu_get_id(void) { return 0; };

static inline bool cpu_is_primary(int id) { return 1; };

static inline bool cpu_is_me(int id) { return 1; };

static inline int cpu_enable_core(int id) { return 0; };

static inline void cpu_disable_core(int id) { };

static inline int cpu_is_core_enabled(int id) { return 1; };

static inline int cpu_enabled_cores(void) { return 1; };

static inline int cpu_restore_secondary_cores(void) { return 0; };

static inline int cpu_secondary_cores_prepare_d0ix(void) { return 0; };

#endif /* CONFIG_MULTICORE && CONFIG_SMP */

#endif

#endif /* __SOF_LIB_CPU_H__ */
