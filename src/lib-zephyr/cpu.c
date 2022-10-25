// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Tomasz Leman <tomasz.m.leman@intel.com>

/**
 * \file
 * \brief Zephyr RTOS CPU implementation file
 * \authors Tomasz Leman <tomasz.m.leman@intel.com>
 */

#include <sof/init.h>
#include <sof/lib/cpu.h>
#include <sof/lib/pm_runtime.h>

/* Zephyr includes */
#include <soc.h>
#include <version.h>
#include <zephyr/kernel.h>

#if CONFIG_MULTICORE && CONFIG_SMP

extern K_KERNEL_STACK_ARRAY_DEFINE(z_interrupt_stacks, CONFIG_MP_NUM_CPUS,
				   CONFIG_ISR_STACK_SIZE);

static atomic_t start_flag;
static atomic_t ready_flag;

static int w_core_enable_mask = 0x1; /*Core 0 is always active*/

/* Zephyr kernel_internal.h interface */
extern void smp_timer_init(void);

static FUNC_NORETURN void secondary_init(void *arg)
{
	struct k_thread dummy_thread;

	/*
	 * This is an open-coded version of zephyr/kernel/smp.c
	 * smp_init_top(). We do this so that we can call SOF
	 * secondary_core_init() for each core.
	 */

	atomic_set(&ready_flag, 1);
	z_smp_thread_init(arg, &dummy_thread);
	smp_timer_init();

	secondary_core_init(sof_get());

#ifdef CONFIG_THREAD_STACK_INFO
	dummy_thread.stack_info.start = (uintptr_t)z_interrupt_stacks +
		arch_curr_cpu()->id * Z_KERNEL_STACK_LEN(CONFIG_ISR_STACK_SIZE);
	dummy_thread.stack_info.size = Z_KERNEL_STACK_LEN(CONFIG_ISR_STACK_SIZE);
#endif

	z_smp_thread_swap();

	CODE_UNREACHABLE; /* LCOV_EXCL_LINE */
}

int cpu_enable_core(int id)
{
	pm_runtime_get(PM_RUNTIME_DSP, PWRD_BY_TPLG | id);

	/* only called from single core, no RMW lock */
	__ASSERT_NO_MSG(cpu_get_id() == PLATFORM_PRIMARY_CORE_ID);

	w_core_enable_mask |= BIT(id);

	return 0;
}

int cpu_enable_secondary_core(int id)
{
	/*
	 * This is an open-coded version of zephyr/kernel/smp.c
	 * z_smp_start_cpu(). We do this, so we can use a customized
	 * secondary_init() for SOF.
	 */

	if (arch_cpu_active(id))
		return 0;

#if ZEPHYR_VERSION(3, 0, 99) <= ZEPHYR_VERSION_CODE
	z_init_cpu(id);
#endif

	atomic_clear(&start_flag);
	atomic_clear(&ready_flag);

	arch_start_cpu(id, z_interrupt_stacks[id], CONFIG_ISR_STACK_SIZE,
		       secondary_init, &start_flag);

	while (!atomic_get(&ready_flag))
		k_busy_wait(100);

	atomic_set(&start_flag, 1);

	return 0;
}

void cpu_disable_core(int id)
{
	/* TODO: call Zephyr API */

	/* only called from single core, no RMW lock */
	__ASSERT_NO_MSG(cpu_get_id() == PLATFORM_PRIMARY_CORE_ID);

	w_core_enable_mask &= ~BIT(id);
}

int cpu_is_core_enabled(int id)
{
	return w_core_enable_mask & BIT(id);
}

int cpu_enabled_cores(void)
{
	return w_core_enable_mask;
}

void cpu_power_down_core(uint32_t flags)
{
	/* TODO: use Zephyr version */
}

int cpu_restore_secondary_cores(void)
{
	/* TODO: use Zephyr API */
	return 0;
}

int cpu_secondary_cores_prepare_d0ix(void)
{
	/* TODO: use Zephyr API */
	return 0;
}

#endif /* CONFIG_MULTICORE && CONFIG_SMP */
