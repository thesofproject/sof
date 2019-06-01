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

#include <sof/sof.h>
#include <sof/agent.h>
#include <sof/debug.h>
#include <sof/panic.h>
#include <sof/alloc.h>
#include <sof/clk.h>
#include <sof/trace.h>
#include <platform/timer.h>
#include <platform/platform.h>
#include <platform/clk.h>
#include <sof/drivers/timer.h>

#define trace_sa(__e, ...) \
	trace_event_atomic(TRACE_CLASS_SA, __e, ##__VA_ARGS__)
#define trace_sa_value(__e, ...) \
	trace_value_atomic(__e, ##__VA_ARGS__)

/*
 * Notify the SA that we are about to enter idle state (WFI).
 */
void sa_enter_idle(struct sof *sof)
{
	struct sa *sa = sof->sa;

	sa->last_idle = platform_timer_get(platform_timer);
}

static uint64_t validate(void *data)
{
	struct sa *sa = data;
	uint64_t current;
	uint64_t delta;

	current = platform_timer_get(platform_timer);
	delta = current - sa->last_idle;

	/* were we last idle longer than timeout */
	if (delta > sa->ticks) {
		trace_sa("validate(), idle longer than timeout, delta = %u",
			delta);
		panic(SOF_IPC_PANIC_IDLE);
	}

	return PLATFORM_IDLE_TIME;
}

void sa_init(struct sof *sof)
{
	struct sa *sa;

	trace_sa("sa_init()");

	sa = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*sa));
	sof->sa = sa;

	/* set default tick timeout */
	sa->ticks = clock_ms_to_ticks(PLATFORM_WORKQ_CLOCK, 1) *
		PLATFORM_IDLE_TIME / 1000;
	trace_sa("sa_init(), sa->ticks = %u", sa->ticks);

	/* set lst idle time to now to give time for boot completion */
	sa->last_idle = platform_timer_get(platform_timer) + sa->ticks;

	schedule_task_init(&sa->work, SOF_SCHEDULE_LL, SOF_TASK_PRI_HIGH,
			   validate, sa, 0, 0);

	schedule_task(&sa->work, PLATFORM_IDLE_TIME, 0, 0);
}
