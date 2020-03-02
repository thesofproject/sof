// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/*
 * System Agent - Simple FW Monitor that can notify host drivers in the event
 * of any FW errors. The SA checks if the DSP is still responsive and verifies
 * the stability of the system by checking time elapsed between every timer
 * tick. If the core exceeds the threshold by over 5% then the SA will emit
 * error trace. However if it will be exceeded by over 100% the panic will be
 * called.
 */

#include <sof/drivers/timer.h>
#include <sof/lib/agent.h>
#include <sof/lib/alloc.h>
#include <sof/lib/clk.h>
#include <sof/lib/uuid.h>
#include <sof/debug/panic.h>
#include <sof/platform.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <ipc/trace.h>
#include <user/trace.h>
#include <stddef.h>
#include <stdint.h>

#define trace_sa(__e, ...) \
	trace_event_atomic(TRACE_CLASS_SA, __e, ##__VA_ARGS__)
#define trace_sa_error(__e, ...) \
	trace_error(TRACE_CLASS_SA, __e, ##__VA_ARGS__)

/* c63c4e75-8f61-4420-9319-1395932efa9e */
DECLARE_SOF_UUID("agent-work", agent_work_task_uuid, 0xc63c4e75, 0x8f61, 0x4420,
		 0x93, 0x19, 0x13, 0x95, 0x93, 0x2e, 0xfa, 0x9e);

static enum task_state validate(void *data)
{
	struct sa *sa = data;
	uint64_t current;
	uint64_t delta;

	current = platform_timer_get(timer_get());
	delta = current - sa->last_check;

	perf_cnt_stamp(TRACE_CLASS_SA, &sa->pcd, true);

	/* panic timeout */
	if (delta > sa->panic_timeout)
		panic(SOF_IPC_PANIC_IDLE);

	/* warning timeout */
	if (delta > sa->warn_timeout)
		trace_sa_error("validate(), ll drift detected, delta = "
			       "%u", delta);

	/* update last_check to current */
	sa->last_check = current;

	return SOF_TASK_STATE_RESCHEDULE;
}

void sa_init(struct sof *sof, uint64_t timeout)
{
	uint64_t ticks;

	trace_sa("sa_init(), timeout = %u", timeout);

	sof->sa = rzalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM,
			  sizeof(*sof->sa));

	/* set default timeouts */
	ticks = clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) * timeout / 1000;

	/* TODO: change values after minimal drifts will be assured */
	sof->sa->panic_timeout = 2 * ticks;	/* 100% delay */
	sof->sa->warn_timeout = ticks + ticks / 20;	/* 5% delay */

	trace_sa("sa_init(), ticks = %u, sof->sa->warn_timeout = %u, sof->sa->panic_timeout = %u",
		 ticks, sof->sa->warn_timeout, sof->sa->panic_timeout);

	schedule_task_init_ll(&sof->sa->work, SOF_UUID(agent_work_task_uuid),
			      SOF_SCHEDULE_LL_TIMER,
			      SOF_TASK_PRI_HIGH, validate, sof->sa, 0, 0);

	schedule_task(&sof->sa->work, 0, timeout);

	/* set last check time to now to give time for boot completion */
	sof->sa->last_check = platform_timer_get(timer_get());
}
