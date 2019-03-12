/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/**
 * \file arch/xtensa/smp/init.c
 * \brief Xtensa SMP initialization functions
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <xtos-structs.h>
#include <platform/memory.h>
#include <sof/interrupt.h>
#include <platform/interrupt.h>
#include <sof/mailbox.h>
#include <arch/cpu.h>
#include <arch/init.h>
#include <sof/init.h>
#include <sof/lock.h>
#include <sof/notifier.h>
#include <sof/panic.h>
#include <sof/schedule.h>
#include <sof/task.h>
#include <platform/idc.h>
#include <stdint.h>

#if DEBUG_LOCKS
/** \brief Debug lock. */
uint32_t lock_dbg_atomic = 0;

/** \brief Debug locks per user. */
uint32_t lock_dbg_user[DBG_LOCK_USERS] = {0};
#endif

/** \brief Core context for master core. */
static struct core_context master_core_ctx;

/** \brief Core context pointers for all the cores. */
struct core_context *core_ctx_ptr[PLATFORM_CORE_COUNT];

/** \brief Xtos core data for master core. */
struct xtos_core_data master_core_data;

/** \brief Xtos core data pointers for all the cores. */
struct xtos_core_data *core_data_ptr[PLATFORM_CORE_COUNT];

/**
 * \brief Initializes core specific data.
 */
static void initialize_pointers_per_core(void)
{
	int core = arch_cpu_get_id();
	struct xtos_core_data *core_data = core_data_ptr[core];

	if (core == PLATFORM_MASTER_CORE_ID) {
		master_core_data.thread_data_ptr = &master_core_ctx.td;
		core_ctx_ptr[PLATFORM_MASTER_CORE_ID] = &master_core_ctx;
	}

	cpu_write_threadptr((int)core_ctx_ptr[core]);

	xtos_structures_pointers *p = &core_data->thread_data_ptr->xtos_ptrs;
	p->xtos_enabled = &core_data->xtos_int_data.xtos_enabled;
	p->xtos_intstruct = &core_data->xtos_int_data;
	p->xtos_interrupt_table = &core_data->xtos_int_data.xtos_interrupt_table.array[0];
	p->xtos_interrupt_mask_table = &core_data->xtos_int_data.xtos_interrupt_mask_table[0];
	p->xtos_stack_for_interrupt_2 = core_data->xtos_stack_for_interrupt_2;
	p->xtos_stack_for_interrupt_3 = core_data->xtos_stack_for_interrupt_3;
	p->xtos_stack_for_interrupt_4 = core_data->xtos_stack_for_interrupt_4;
	p->xtos_stack_for_interrupt_5 = core_data->xtos_stack_for_interrupt_5;
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

	trace_point(TRACE_BOOT_SYS_NOTE);
	init_system_notify(sof);

	trace_point(TRACE_BOOT_SYS_SCHED);
	scheduler_init();

	platform_interrupt_init();

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
