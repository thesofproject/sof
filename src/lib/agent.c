// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/*
 * System Agent - Simple FW Monitor that can notify host drivers in the event
 * of any FW errors. The SA assumes that each core will enter the idle state
 * from time to time (within a period of PLATFORM_IDLE_TIME). If the core does
 * not enter the idle loop through looping forever or scheduling some work
 * continuously then the SA will emit trace and panic().
 */

#include <sof/drivers/timer.h>
#include <sof/lib/agent.h>
#include <sof/lib/alloc.h>
#include <sof/lib/clk.h>
#include <sof/debug/panic.h>
#include <sof/platform.h>
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
#define trace_sa_value(__e, ...) \
	trace_value_atomic(__e, ##__VA_ARGS__)
#define trace_sa_error(__e, ...) \
	trace_error(TRACE_CLASS_SA, __e, ##__VA_ARGS__)

struct sa *sa;

/*
 * Notify the SA that we are about to enter idle state (WFI).
 */
void sa_enter_idle(struct sof *sof)
{
	struct sa *sa = sof->sa;

	sa->last_idle = platform_timer_get(platform_timer);
}

static enum task_state validate(void *data)
{
	struct sa *sa = data;
	uint64_t current;
	uint64_t delta;

	current = platform_timer_get(platform_timer);
	delta = current - sa->last_idle;

	/* were we last idle longer than timeout */

	if (delta > sa->ticks && sa->is_active) {
		trace_sa("validate(), idle longer than timeout, delta = %u",
			delta);
		panic(SOF_IPC_PANIC_IDLE);
	}

	return SOF_TASK_STATE_RESCHEDULE;
}

void sa_init(struct sof *sof)
{

	trace_sa("sa_init()");

	sa = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*sa));
	sof->sa = sa;

	/* set default tick timeout */
	sa->ticks = clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) *
		PLATFORM_IDLE_TIME / 1000;

	trace_sa("sa_init(), sa->ticks = %u", sa->ticks);

	/* set lst idle time to now to give time for boot completion */
	sa->last_idle = platform_timer_get(platform_timer) + sa->ticks;
	sa->is_active = true;

	schedule_task_init(&sa->work, SOF_SCHEDULE_LL_TIMER, SOF_TASK_PRI_HIGH,
			   validate, NULL, sa, 0, 0);

	schedule_task(&sa->work, PLATFORM_IDLE_TIME, PLATFORM_IDLE_TIME);
}

void sa_disable(void)
{
	sa->is_active = false;
}

void sa_enable(void)
{
	sa->is_active = true;
	sa->last_idle = platform_timer_get(platform_timer);
}
