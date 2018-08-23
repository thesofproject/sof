/*
 * Copyright (c) 2017, Intel Corporation
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
 *
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
#include <sof/clock.h>
#include <sof/trace.h>
#include <platform/timer.h>
#include <platform/platform.h>
#include <platform/clk.h>

#define trace_sa(__e)	trace_event_atomic(TRACE_CLASS_SA, __e)
#define trace_sa_value(__e)	trace_value_atomic(__e)

/*
 * Notify the SA that we are about to enter idle state (WFI).
 */
void sa_enter_idle(struct sof *sof)
{
	struct sa *sa = sof->sa;

	sa->last_idle = drivers_timer_get(platform_timer);
}

static uint64_t validate(void *data, uint64_t delay)
{
	struct sa *sa = data;
	uint64_t current;
	uint64_t delta;

	current = drivers_timer_get(platform_timer);
	delta = current - sa->last_idle;

	/* were we last idle longer than timeout */
	if (delta > sa->ticks) {
		trace_sa("tim");
		trace_sa_value(delta);
		panic(SOF_IPC_PANIC_IDLE);
	}

	return PLATFORM_IDLE_TIME;
}

void sa_init(struct sof *sof)
{
	struct sa *sa;

	trace_sa("ini");

	sa = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*sa));
	sof->sa = sa;

	/* set default tick timout */
	sa->ticks = clock_us_to_ticks(PLATFORM_WORKQ_CLOCK, PLATFORM_IDLE_TIME);
	trace_sa_value(sa->ticks);

	/* set lst idle time to now to give time for boot completion */
	sa->last_idle = drivers_timer_get(platform_timer) + sa->ticks;
	work_init(&sa->work, validate, sa, WORK_ASYNC);
	work_schedule_default(&sa->work, PLATFORM_IDLE_TIME);
}
