// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

/**
 * \file
 * \brief Xtensa CPU implementation file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <sof/common.h>
#include <sof/drivers/idc.h>
#include <rtos/interrupt.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/mm_heap.h>
#include <sof/lib/notifier.h>
#include <sof/lib/pm_runtime.h>
#include <rtos/wait.h>
#include <sof/schedule/schedule.h>
#include <sof/sof.h>
#include <sof/trace/trace.h>
#include <ipc/trace.h>
#include <user/trace.h>
#include <xtos-structs.h>
#include <stdint.h>

extern struct core_context *core_ctx_ptr[CONFIG_CORE_COUNT];
extern struct xtos_core_data *core_data_ptr[CONFIG_CORE_COUNT];

static uint32_t active_cores_mask = BIT(PLATFORM_PRIMARY_CORE_ID);

#if CONFIG_NO_SECONDARY_CORE_ROM
extern void *shared_vecbase_ptr;
extern uint8_t _WindowOverflow4[];

/**
 * \brief This function will allocate memory for shared secondary cores
 *	  dynamic vectors and set global pointer shared_vecbase_ptr
 */
static void alloc_shared_secondary_cores_objects(void)
{
	uint8_t *dynamic_vectors;

	dynamic_vectors = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, 0, SOF_DYNAMIC_VECTORS_SIZE);
	if (dynamic_vectors == NULL)
		panic(SOF_IPC_PANIC_MEM);

	shared_vecbase_ptr = dynamic_vectors;
	dcache_writeback_region(&shared_vecbase_ptr,
				sizeof(shared_vecbase_ptr));
}

/**
 * \brief This function will copy dynamic vectors from _WindowOverflow4
 *	  to shared shared_vecbase_ptr used in alternate reset vector
 */
static void unpack_dynamic_vectors(void)
{
	void *dyn_vec_start_addr = _WindowOverflow4;

	memcpy_s(shared_vecbase_ptr, SOF_DYNAMIC_VECTORS_SIZE,
		 dyn_vec_start_addr, SOF_DYNAMIC_VECTORS_SIZE);
	dcache_writeback_invalidate_region(shared_vecbase_ptr,
					   SOF_DYNAMIC_VECTORS_SIZE);
}
#endif

int arch_cpu_enable_core(int id)
{
	struct idc_msg power_up = {
		IDC_MSG_POWER_UP, IDC_MSG_POWER_UP_EXT, id };
	int ret;

	if (!arch_cpu_is_core_enabled(id)) {
		/* Turn on stack memory for core */
		pm_runtime_get(CORE_MEMORY_POW, id);

		/* Power up secondary core */
		pm_runtime_get(PM_RUNTIME_DSP, PWRD_BY_TPLG | id);

		/* allocate resources for core */
		cpu_alloc_core_context(id);

		/* enable IDC interrupt for the secondary core */
		idc_enable_interrupts(id, cpu_get_id());

#if CONFIG_NO_SECONDARY_CORE_ROM
		/* unpack dynamic vectors if it is the first secondary core */
		if (active_cores_mask == BIT(PLATFORM_PRIMARY_CORE_ID)) {
			alloc_shared_secondary_cores_objects();
			unpack_dynamic_vectors();
		}
#endif
		/* send IDC power up message */
		ret = idc_send_msg(&power_up, IDC_POWER_UP);
		if (ret < 0)
			return ret;

		active_cores_mask |= (1 << id);
	}

	return 0;
}

void arch_cpu_disable_core(int id)
{
	struct idc_msg power_down = {
		IDC_MSG_POWER_DOWN, IDC_MSG_POWER_DOWN_EXT, id };

	if (arch_cpu_is_core_enabled(id)) {
		idc_send_msg(&power_down, IDC_POWER_DOWN);

		active_cores_mask ^= (1 << id);
#if CONFIG_NO_SECONDARY_CORE_ROM
		/* free shared dynamic vectors it was the last secondary core */
		if (active_cores_mask == BIT(PLATFORM_PRIMARY_CORE_ID)) {
			rfree(shared_vecbase_ptr);
			shared_vecbase_ptr = NULL;
		}
#endif
	}
}

int arch_cpu_is_core_enabled(int id)
{
	return active_cores_mask & BIT(id);
}

int arch_cpu_enabled_cores(void)
{
	return active_cores_mask;
}

void cpu_alloc_core_context(int core)
{
	struct core_context *core_ctx;

	core_ctx = rzalloc_core_sys(core, sizeof(*core_ctx));
	dcache_writeback_invalidate_region(core_ctx, sizeof(*core_ctx));

	core_data_ptr[core] = rzalloc_core_sys(core,
					       sizeof(*core_data_ptr[core]));
	core_data_ptr[core]->thread_data_ptr = &core_ctx->td;
	dcache_writeback_invalidate_region(core_data_ptr[core],
					   sizeof(*core_data_ptr[core]));

	dcache_writeback_invalidate_region(core_data_ptr,
					   sizeof(core_data_ptr));

	core_ctx_ptr[core] = core_ctx;
	dcache_writeback_invalidate_region(core_ctx_ptr,
					   sizeof(core_ctx_ptr));

	/* share pointer to sof context */
	dcache_writeback_region(sof_get(), sizeof(*sof_get()));
}

void cpu_power_down_core(uint32_t flags)
{
	arch_interrupt_global_disable();

	/* Power down with memory on is performed by secondary cores during
	 * d0 -> d0ix before they are disabled by primary core.
	 */
	if (flags & CPU_POWER_DOWN_MEMORY_ON) {
		/* disable idc interrupts */
		idc_free(IDC_FREE_IRQ_ONLY);

		/* disable scheduler interrupts */
		schedule_free(SOF_SCHEDULER_FREE_IRQ_ONLY);

		/* data writeback/invalidate */
		dcache_writeback_invalidate_all();

		/* after writeback/invalidate secondary core is prepared for
		 * powered off - prepare_d0ix_core_mask flag can be disabled
		 */
		platform_pm_runtime_prepare_d0ix_dis(cpu_get_id());
	} else {
		idc_free(0);

		schedule_free(0);

		free_system_notify();

		/* free entire sys heap, an instance dedicated for this core */
		free_heap(SOF_MEM_ZONE_SYS);

		dcache_writeback_invalidate_all();

		/* Turn off stack memory for core */
		pm_runtime_put(CORE_MEMORY_POW, cpu_get_id());

		pm_runtime_put(PM_RUNTIME_DSP, PWRD_BY_TPLG | cpu_get_id());
	}

	trace_point(0);

	/* arch_wait_for_interrupt() not used, because it will cause panic.
	 * This code is executed on irq lvl > 0, which is expected.
	 * Core will be put into reset by host anyway.
	 */
	while (1)
		arch_wait_for_interrupt(0);
}

int arch_cpu_restore_secondary_cores(void)
{
	struct idc_msg power_up = { IDC_MSG_POWER_UP, IDC_MSG_POWER_UP_EXT };
	int ret, id;

	for (id = 0; id < CONFIG_CORE_COUNT; id++) {
		if (arch_cpu_is_core_enabled(id) && id != PLATFORM_PRIMARY_CORE_ID) {
			power_up.core = id;

			/* Power up secondary core */
			pm_runtime_get(PM_RUNTIME_DSP, id);

			/* enable IDC interrupt for the secondary core */
			idc_enable_interrupts(id, cpu_get_id());

			/* send IDC power up message */
			ret = idc_send_msg(&power_up, IDC_POWER_UP);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

int arch_cpu_secondary_cores_prepare_d0ix(void)
{
	struct idc_msg prepare_msg = { IDC_MSG_PREPARE_D0ix,
				       IDC_MSG_PREPARE_D0ix_EXT };
	int ret, id;

	for (id = 0; id < CONFIG_CORE_COUNT; id++) {
		if (arch_cpu_is_core_enabled(id) && id != PLATFORM_PRIMARY_CORE_ID) {
			prepare_msg.core = id;

			/* send IDC prepare message to all enabled secondary
			 * cores.
			 */
			ret = idc_send_msg(&prepare_msg, IDC_BLOCKING);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}
