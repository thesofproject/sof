// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/**
 * \file arch/xtensa/init.c
 * \brief Xtensa initialization functions
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <config.h>
#include <xtos-structs.h>
#include <sof/memory.h>
#include <sof/drivers/interrupt.h>
#include <sof/mailbox.h>
#include <arch/init.h>
#include <sof/init.h>
#include <sof/lock.h>
#include <sof/notifier.h>
#include <sof/panic.h>
#include <sof/schedule/schedule.h>
#include <sof/task.h>
#include <sof/idc.h>
#include <sof/cpu.h>
#include <stdint.h>

#if DEBUG_LOCKS
/** \brief Debug lock. */
uint32_t lock_dbg_atomic;

/** \brief Debug locks per user. */
uint32_t lock_dbg_user[DBG_LOCK_USERS] = {0};
#endif

/** \brief Core context for master core. */
static struct core_context master_core_ctx;

/** \brief Core context pointers for all the cores. */
struct core_context *core_ctx_ptr[PLATFORM_CORE_COUNT];

#if CONFIG_SMP
/** \brief Xtos core data for master core. */
struct xtos_core_data master_core_data;

/** \brief Xtos core data pointers for all the cores. */
struct xtos_core_data *core_data_ptr[PLATFORM_CORE_COUNT];
#endif

/**
 * \brief Initializes core specific data.
 */
static void initialize_pointers_per_core(void)
{
#if CONFIG_SMP
	int core = arch_cpu_get_id();
	struct xtos_core_data *core_data = core_data_ptr[core];
	xtos_structures_pointers *p;

	if (core == PLATFORM_MASTER_CORE_ID) {
		master_core_data.thread_data_ptr = &master_core_ctx.td;
		core_ctx_ptr[PLATFORM_MASTER_CORE_ID] = &master_core_ctx;
	}

	cpu_write_threadptr((int)core_ctx_ptr[core]);

	p = &core_data->thread_data_ptr->xtos_ptrs;
	p->xtos_enabled = &core_data->xtos_int_data.xtos_enabled;
	p->xtos_intstruct = &core_data->xtos_int_data;
	p->xtos_interrupt_table =
		&core_data->xtos_int_data.xtos_interrupt_table.array[0];
	p->xtos_interrupt_mask_table =
		&core_data->xtos_int_data.xtos_interrupt_mask_table[0];
#else
	core_ctx_ptr[PLATFORM_MASTER_CORE_ID] = &master_core_ctx;
	cpu_write_threadptr((int)core_ctx_ptr[PLATFORM_MASTER_CORE_ID]);
#endif
}

/**
 * \brief Initializes architecture.
 * \param[in,out] sof Firmware main context.
 * \return Error status.
 */
int arch_init(struct sof *sof)
{
	initialize_pointers_per_core();
	register_exceptions();
	arch_assign_tasks();
	return 0;
}

int slave_core_init(struct sof *sof)
{
	int err;

	/* init architecture */
	trace_point(TRACE_BOOT_ARCH);
	err = arch_init(sof);
	if (err < 0)
		panic(SOF_IPC_PANIC_ARCH);

	trace_point(TRACE_BOOT_SYS_NOTIFIER);
	init_system_notify(sof);

	/* interrupts need to be initialized before any usage */
	trace_point(TRACE_BOOT_PLATFORM_IRQ);
	platform_interrupt_init();

	trace_point(TRACE_BOOT_PLATFORM_SCHED);
	scheduler_init();

	/* initialize IDC mechanism */
	trace_point(TRACE_BOOT_PLATFORM_IDC);
	err = idc_init();
	if (err < 0)
		return err;

	trace_point(TRACE_BOOT_PLATFORM);

	/* should not return */
	err = do_task_slave_core(sof);

	return err;
}
