// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

/**
 * \file arch/xtensa/cpu.c
 * \brief Xtensa CPU implementation file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <arch/alloc.h>
#include <arch/atomic.h>
#include <arch/cpu.h>
#include <arch/idc.h>
#include <platform/platform.h>
#include <sof/idc.h>
#include <sof/lock.h>
#include <sof/notifier.h>
#include <sof/schedule/schedule.h>

/* cpu tracing */
#define trace_cpu(__e) trace_event(TRACE_CLASS_CPU, __e)
#define trace_cpu_error(__e) trace_error(TRACE_CLASS_CPU, __e)
#define tracev_cpu(__e) tracev_event(TRACE_CLASS_CPU, __e)

static uint32_t active_cores_mask = 0x1;
static spinlock_t lock = { 0 };

void arch_cpu_enable_core(int id)
{
	struct idc_msg power_up = {
		IDC_MSG_POWER_UP, IDC_MSG_POWER_UP_EXT, id };
	uint32_t flags;

	spin_lock_irq(&lock, flags);

	if (!arch_cpu_is_core_enabled(id)) {
		/* allocate resources for core */
		alloc_core_context(id);

		/* enable IDC interrupt for the the slave core */
		idc_enable_interrupts(id, arch_cpu_get_id());

		/* send IDC power up message */
		arch_idc_send_msg(&power_up, IDC_NON_BLOCKING);

		active_cores_mask |= (1 << id);
	}

	spin_unlock_irq(&lock, flags);
}

void arch_cpu_disable_core(int id)
{
	struct idc_msg power_down = {
		IDC_MSG_POWER_DOWN, IDC_MSG_POWER_DOWN_EXT, id };
	uint32_t flags;

	spin_lock_irq(&lock, flags);

	if (arch_cpu_is_core_enabled(id)) {
		arch_idc_send_msg(&power_down, IDC_NON_BLOCKING);

		active_cores_mask ^= (1 << id);
	}

	spin_unlock_irq(&lock, flags);
}

int arch_cpu_is_core_enabled(int id)
{
	return active_cores_mask & (1 << id);
}

void cpu_power_down_core(void)
{
	arch_interrupt_global_disable();

	idc_free();

	schedule_free();

	free_system_notify();

	/* free entire sys heap, an instance dedicated for this core */
	free_heap(RZONE_SYS);

	dcache_writeback_invalidate_all();

	/* arch_wait_for_interrupt() not used, because it will cause panic.
	 * This code is executed on irq lvl > 0, which is expected.
	 * Core will be put into reset by host anyway.
	 */
	while (1)
		asm volatile("waiti 0");
}
