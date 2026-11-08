// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Szkudlinski
 */

#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/debug/telemetry/telemetry.h>
#include <sof/llext_manager.h>
#include <rtos/task.h>
#include <rtos/userspace_helper.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/trace/trace.h>
#include <rtos/wait.h>
#include <rtos/interrupt.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/clock.h>
#include <zephyr/sys/sem.h>
#include <zephyr/sys/mutex.h>
#include <sof/lib/memory.h>
#include <sof/schedule/schedule.h>
#include <ipc4/base_fw.h>

#include <sof/schedule/zephyr_dp_schedule.h>

#include <zephyr/kernel/thread.h>

#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(dp_schedule, CONFIG_SOF_LOG_LEVEL);
SOF_DEFINE_REG_UUID(dp_sched);

/* unused with Zephyr, generates no output */
DECLARE_TR_CTX(dp_tr, SOF_UUID(dp_sched_uuid), LOG_LEVEL_INFO);

/* Kernel part of the DP scheduler: the scheduler locks, their access grants, the LL
 * tick system call and the scheduler initialization. The rest is in
 * src/schedule/zephyr_dp_schedule.c
 */

#define DP_LOCK_INIT(i, _)	Z_SEM_INITIALIZER(dp_lock[i], 1, 1)
#define DP_LOCK_INIT_LIST	LISTIFY(CONFIG_MP_MAX_NUM_CPUS, DP_LOCK_INIT, (,))

/* User threads don't need access to this array. Access is performed from
 * the kernel space via a syscall. Array must be placed in special section
 * to be qualified as initialized by the gen_kobject_list.py script.
 */
STRUCT_SECTION_ITERABLE_ARRAY(k_sem, dp_lock, CONFIG_MP_MAX_NUM_CPUS) = { DP_LOCK_INIT_LIST };

void scheduler_dp_grant(k_tid_t thread_id, uint16_t core)
{
	if (thread_id)
		k_thread_access_grant(thread_id, &dp_lock[core]);
}

/* dummy LL task - to start LL on secondary cores */
static enum task_state scheduler_dp_ll_tick_dummy(void *data)
{
	return SOF_TASK_STATE_RESCHEDULE;
}

/*
 * function called after every LL tick
 *
 * This function checks if the queued DP tasks are ready to processing (meaning
 *    the module run by the task has enough data at all sources and enough free space
 *    on all sinks)
 *
 *    if the task becomes ready, a deadline is set allowing Zephyr to schedule threads
 *    in right order
 *
 * TODO: currently there's a limitation - DP module must be surrounded by LL modules.
 * it simplifies algorithm - there's no need to browse through DP chains calculating
 * deadlines for each module in function of all modules execution status.
 * Now is simple - modules deadline is its start + tick time.
 *
 * example:
 *  Lets assume we do have a pipeline:
 *
 *  LL1 -> DP1 -> LL2 -> DP2 -> LL3 -> DP3 -> LL4
 *
 *  all LLs starts in 1ms tick
 *
 *  for simplification lets assume
 *   - all LLs are on primary core, all DPs on secondary (100% CPU is for DP)
 *   - context switching requires 0 cycles
 *
 *  DP1 - starts every 1ms, needs 0.5ms to finish processing
 *  DP2 - starts every 2ms, needs 0.6ms to finish processing
 *  DP3 - starts every 10ms, needs 0.3ms to finish processing
 *
 * TICK0
 *	only LL1 is ready to run
 *		LL1 processing (producing data chunk for DP1)
 *
 * TICK1
 *	LL1 is ready to run
 *	DP1 is ready tu run (has data from LL1) set deadline to TICK2
 *	  LL1 processing (producing second data chunk for DP1)
 *	  DP1 processing for 0.5ms (consuming first data chunk, producing data chunk for LL2)
 *	CPU is idle for 0.5ms
 *
 * TICK2
 *	LL1 is ready to run
 *	DP1 is ready tu run set deadline to TICK3
 *	LL2 is ready to run
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing 50% data chunk for DP2)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *	CPU is idle for 0.5ms
 *
 * TICK3
 *	LL1 is ready to run
 *	DP1 is ready tu run set deadline to TICK4
 *	LL2 is ready to run
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing rest of data chunk for DP2)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *	CPU is idle for 0.5ms
 *
 * TICK4
 *	LL1 is ready to run
 *	DP1 is ready tu run set deadline to TICK5
 *	LL2 is ready to run
 *	DP2 is ready to run set deadline to TICK6
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing 50% of second data chunk for DP2)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *		DP2 processing for 0.5ms (no data produced as DP2 has 0.1ms to go)
 *	100% CPU used
 *
 *	!!!!!! Note here - DP1 must do before DP2 as it MUST finish in this tick. DP2 can wait
 *		>>>>>>> this is what we call EDF - EARIEST DEADLINE FIRST <<<<<<
 *
 * TICK5
 *	LL1 is ready to run
 *	DP1 is ready tu run set deadline to TICK6
 *	LL2 is ready to run
 *	DP2 is in progress, deadline is set to TICK6
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing rest of second data chunk for DP2)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *		DP2 processing for 0.1ms (producing TWO data chunks for LL3)
 *	CPU is idle for 0.4ms (60% used)
 *
 * TICK6
 *	LL1 is ready to run
 *	DP1 is ready tu run set deadline to TICK7
 *	LL2 is ready to run
 *	DP2 is ready to run set deadline to TICK8
 *	LL3 is ready to run
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing 50% of second data chunk for DP2)
 *		LL3 processing (producing 10% of first data chunk for DP3)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *		DP2 processing for 0.5ms (no data produced as DP2 has 0.1ms to go)
 *	100% CPU used
 *
 *
 *
 *   (........ 9 more cycles - LL3 procuces 100% of data for DP3......)
 *
 *
 * TICK15
 *	LL1 is ready to run
 *	DP1 is ready tu run set deadline to TICK16
 *	LL2 is ready to run
 *	DP2 is ready to run set deadline to TICK17
 *	LL3 is ready to run
 *	DP3 is ready to run set deadline to TICK25
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing 50% of data chunk for DP2)
 *		LL3 processing (producing 10% of second data chunk for DP3)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *		DP2 processing for 0.5ms (no data produced as DP2 has 0.1ms to go)
 *	100% CPU used -
 *	!!! note that DP3 is ready but has no chance to get CPU in this cycle
 *
 * TICK16
 *	LL1 is ready to run set deadline to TICK17
 *	DP1 is ready tu run
 *	LL2 is ready to run
 *	DP2 is in progress, deadline is set to TICK17
 *	LL3 is ready to run
 *	DP3 is in progress, deadline is set to TICK25
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing rest of data chunk for DP2)
 *		LL3 processing (producing 10% of second data chunk for DP3)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *		DP2 processing for 0.1ms (producing data)
 *		DP3 processing for 0.2ms (producing 10 data chunks for LL4)
 *	90% CPU used
 *
 * TICK17
 *	LL1 is ready to run
 *	DP1 is ready tu run
 *	LL2 is ready to run
 *	DP2 is ready to run
 *	LL3 is ready to run
 *	LL4 is ready to run
 *			!! NOTE that DP3 is not ready - it will be ready again in TICK25
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing rest of data chunk for DP2)
 *		LL3 processing (producing next 10% of second data chunk for DP3)
 *		LL4 processing (consuming 10% of data prepared by DP3)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *		DP2 processing for 0.5ms (no data produced as DP2 has 0.1ms to go)
 *		100% CPU used
 *
 *
 * Now - pipeline is in stable state, CPU used almost in 100% (it would be 100% if DP3
 * needed 1.2ms for processing - but the example would be too complicated)
 */

void z_impl_scheduler_dp_ll_tick(void)
{
	unsigned int lock_key;
#if CONFIG_SOF_USERSPACE_LL
	struct scheduler_dp_data *dp_sch = scheduler_get_user_data(SOF_SCHEDULE_DP);
#else
	struct scheduler_dp_data *dp_sch = scheduler_get_data(SOF_SCHEDULE_DP);
#endif

	if (!dp_sch)
		return;

	/* remember current timestamp as "NOW" */
	dp_sch->last_ll_tick_timestamp = k_cycle_get_32();

	lock_key = scheduler_dp_lock(cpu_get_id());
	scheduler_dp_recalculate(dp_sch);
	scheduler_dp_unlock(lock_key);
}

#ifdef CONFIG_USERSPACE
#include <zephyr/internal/syscall_handler.h>
#include <sof/userspace/syscall_caller.h>
void z_vrfy_scheduler_dp_ll_tick(void)
{
	SOF_SYSCALL_CALLER_CHECK(SOF_SYSUSER_LL);

	z_impl_scheduler_dp_ll_tick();
}
#include <zephyr/syscalls/scheduler_dp_ll_tick_mrsh.c>
#endif

void z_impl_scheduler_dp_check_deadline(uint16_t core, uint32_t deadline_clock_ticks)
{
	if (deadline_clock_ticks &&
	    (int32_t)(k_cycle_get_32() - deadline_clock_ticks) > 0)
		telemetry_notify_dp_deadline_miss(core);
}

#ifdef CONFIG_USERSPACE
void z_vrfy_scheduler_dp_check_deadline(uint16_t core, uint32_t deadline_clock_ticks)
{
	K_OOPS(K_SYSCALL_VERIFY(core < CONFIG_MP_MAX_NUM_CPUS));
	z_impl_scheduler_dp_check_deadline(core, deadline_clock_ticks);
}
#include <zephyr/syscalls/scheduler_dp_check_deadline_mrsh.c>
#endif

APP_SYSUSER_DATA static struct scheduler_ops schedule_dp_ops = {
	.schedule_task		= scheduler_dp_task_shedule,
#if CONFIG_SOF_USERSPACE_APPLICATION
	.schedule_task_cancel	= scheduler_dp_task_cancel,
#else
	.schedule_task_cancel	= scheduler_dp_task_stop,
#endif
	.schedule_task_free	= scheduler_dp_task_free,
};

/* Runs on each core */
__cold int scheduler_dp_init(void)
{
	assert_can_be_cold();

	struct scheduler_dp_data *dp_sch = sof_heap_alloc(sof_sys_user_heap_get(),
							  SOF_MEM_FLAG_KERNEL, sizeof(*dp_sch), 0);
	if (!dp_sch)
		return -ENOMEM;

	memset(dp_sch, 0, sizeof(*dp_sch));
	list_init(&dp_sch->tasks);

	scheduler_init(SOF_SCHEDULE_DP, &schedule_dp_ops, dp_sch);

	/* init src of DP tick */
	return schedule_task_init_ll(&dp_sch->ll_tick_src,
				     SOF_UUID(dp_sched_uuid),
				     SOF_SCHEDULE_LL_TIMER,
				     0, scheduler_dp_ll_tick_dummy, dp_sch,
				     cpu_get_id(), 0);
}
