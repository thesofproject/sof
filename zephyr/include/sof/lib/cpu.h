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

/**
 * \brief Id of primary DSP core
 *
 * SOF IPC protocols make a distinction between primary
 * and secondary cores. In Zephyr, primary core id concept
 * is not present in public OS interface, but in implementation
 * zero is the boot core (see z_smp_init() in Zephyr).
 */
#define PLATFORM_PRIMARY_CORE_ID	0

#if !defined(__ASSEMBLER__) && !defined(LINKER)

#include <stdbool.h>

#include <zephyr/arch/arch_inlines.h>

#if CONFIG_PM

#include <zephyr/pm/pm.h>

void cpu_notify_state_entry(enum pm_state state);

void cpu_notify_state_exit(enum pm_state state);

#endif /* CONFIG_PM */

/*
 * cpu_get_id() is exposed as a Zephyr system call so that user-mode
 * threads (e.g. user-space LL pipelines) can query the current core
 * id. The underlying arch_proc_id() reads a privileged special
 * register (e.g. Xtensa PRID) which would fault if executed directly
 * from user mode. In supervisor context the generated wrapper inlines
 * the z_impl_cpu_get_id() body, so there is no overhead there.
 */
#if defined(CONFIG_USERSPACE) && defined(CONFIG_SOF_FULL_ZEPHYR_APPLICATION)
__syscall int cpu_get_id(void);
#endif

/* let the compiler optimise when in single core mode */
#if CONFIG_MULTICORE && CONFIG_SMP

#if defined(CONFIG_USERSPACE) && defined(CONFIG_SOF_FULL_ZEPHYR_APPLICATION)
static inline int z_impl_cpu_get_id(void)
{
	return arch_proc_id();
}
#else
static inline int cpu_get_id(void)
{
	return arch_proc_id();
}
#define z_impl_cpu_get_id cpu_get_id
#endif

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

#if defined(CONFIG_USERSPACE) && defined(CONFIG_SOF_FULL_ZEPHYR_APPLICATION)
static inline int z_impl_cpu_get_id(void) { return 0; };
#else
static inline int cpu_get_id(void) { return 0; };
#define z_impl_cpu_get_id cpu_get_id
#endif

static inline bool cpu_is_primary(int id) { return 1; };

static inline bool cpu_is_me(int id) { return 1; };

static inline int cpu_enable_core(int id) { return 0; };

static inline void cpu_disable_core(int id) { };

static inline int cpu_is_core_enabled(int id) { return 1; };

static inline int cpu_enabled_cores(void) { return 1; };

static inline int cpu_restore_secondary_cores(void) { return 0; };

static inline int cpu_secondary_cores_prepare_d0ix(void) { return 0; };

#endif /* CONFIG_MULTICORE && CONFIG_SMP */

#if defined(CONFIG_USERSPACE) && defined(CONFIG_SOF_FULL_ZEPHYR_APPLICATION)
#include <zephyr/syscalls/cpu.h>
#endif

#endif

#endif /* __SOF_LIB_CPU_H__ */
