// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Tomasz Lauda <tomasz.lauda@linux.intel.com>

/**
 * \file arch/xtensa/task.c
 * \brief Arch task implementation file
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *          Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/wait.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <ipc/topology.h>
#include <config.h>
#include <xtensa/corebits.h>
#include <xtensa/xtruntime-frames.h>
#include <xtos-structs.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

enum task_state task_main_slave_core(void *data)
{
#if CONFIG_SMP
	/* main audio processing loop */
	while (1) {
		/* sleep until next IDC or DMA */
		trace_event(TRACE_CLASS_IRQ, "wait_for_interrupt in do_task_slave_core");
		wait_for_interrupt(0);
	}
#endif

	return SOF_TASK_STATE_COMPLETED;
}

struct task **task_main_get(void)
{
	struct core_context *ctx = (struct core_context *)cpu_read_threadptr();

	return &ctx->main_task;
}

volatile void *task_context_get(void)
{
	struct core_context *ctx = (struct core_context *)cpu_read_threadptr();

	return ctx->td.xtos_active_task;
}

void task_context_set(void *task_ctx)
{
	struct core_context *ctx = (struct core_context *)cpu_read_threadptr();

	ctx->td.xtos_active_task = task_ctx;
}

int task_context_init(struct task *task, void *entry)
{
	struct edf_task_pdata *edf_pdata = edf_sch_get_pdata(task);
	xtos_task_context *ctx;
	UserFrame *sp;

	/* allocate task context */
	ctx = rzalloc(RZONE_SYS_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*ctx));
	if (!ctx)
		return -ENOMEM;

	/* allocate stack */
	ctx->stack_base = rballoc(RZONE_BUFFER, SOF_MEM_CAPS_RAM,
				  SOF_TASK_DEFAULT_STACK_SIZE);
	if (!ctx->stack_base)
		return -ENOMEM;

	ctx->stack_size = SOF_TASK_DEFAULT_STACK_SIZE;

	bzero(ctx->stack_base, ctx->stack_size);

	/* set initial stack pointer */
	sp = ctx->stack_base + ctx->stack_size - sizeof(UserFrame);

	/* entry point */
	sp->pc = (uint32_t)entry;

	/* a1 is pointer to stack */
	sp->a1 = (uint32_t)sp;

	/* PS_WOECALL4_ABI - window overflow and increment enable
	 * PS_UM - user vector mode enable
	 */
	sp->ps = PS_WOECALL4_ABI | PS_UM;

	/* a6 is first parameter */
	sp->a6 = (uint32_t)task;

	ctx->stack_pointer = sp;

	/* flush for slave core */
	if (cpu_is_slave(task->core))
		task_context_cache(ctx, CACHE_WRITEBACK_INV);

	edf_pdata->ctx = ctx;

	return 0;
}

void task_context_free(struct task *task)
{
	struct edf_task_pdata *edf_pdata = edf_sch_get_pdata(task);
	xtos_task_context *ctx = edf_pdata->ctx;

	rfree(ctx->stack_base);
	ctx->stack_size = 0;
	ctx->stack_pointer = NULL;

	rfree(ctx);
	edf_pdata->ctx = NULL;
}

void task_context_cache(void *task_ctx, int cmd)
{
	xtos_task_context *ctx = task_ctx;

	switch (cmd) {
	case CACHE_WRITEBACK_INV:
		dcache_writeback_invalidate_region(ctx->stack_base,
						   ctx->stack_size);
		dcache_writeback_invalidate_region(ctx, sizeof(*ctx));
		break;
	case CACHE_INVALIDATE:
		dcache_invalidate_region(ctx, sizeof(*ctx));
		dcache_invalidate_region(ctx->stack_base, ctx->stack_size);
		break;
	}
}
