// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/*
 * Simple wait for event completion and signaling with timeouts.
 */

#include <stdint.h>
#include <errno.h>
#include <arch/wait.h>
#include <sof/io.h>
#include <sof/debug.h>
#include <sof/wait.h>
#include <sof/schedule.h>
#include <sof/timer.h>
#include <sof/interrupt.h>
#include <sof/trace.h>
#include <sof/lock.h>
#include <sof/clk.h>
#include <platform/clk.h>
#include <platform/interrupt.h>
#include <sof/drivers/timer.h>
#include <platform/platform.h>

#define DEFAULT_TRY_TIMES 8

/* simple interrupt based wait for completion with timeout */
int wait_for_completion_timeout(completion_t *comp)
{
	volatile completion_t *c = (volatile completion_t *)comp;

	schedule_task(&comp->work, comp->timeout, 0, 0);
	comp->timeout = 0;

	/* check for completion after every wake from IRQ */
	while (!c->complete && !c->timeout)
		wait_for_interrupt(0);

	/* did we complete */
	if (c->complete) {
		/* no timeout so cancel work and return 0 */
		schedule_task_cancel(&comp->work);

		return 0;
	}

	/* timeout */
	trace_error_value(c->timeout);
	trace_error_value(c->complete);

	return -ETIME;
}

int poll_for_completion_delay(completion_t *comp, uint64_t us)
{
	uint64_t tick = clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) *
						us / 1000;
	uint32_t tries = DEFAULT_TRY_TIMES;
	uint64_t delta = tick / tries;

	if (!delta) {
		delta = us;
		tries = 1;
	}

	while (!wait_is_completed(comp)) {
		if (!tries--) {
			trace_error(TRACE_CLASS_WAIT, "ewt");
			return -EIO;
		}

		wait_delay(delta);
	}

	return 0;
}

int poll_for_register_delay(uint32_t reg, uint32_t mask,
			    uint32_t val, uint64_t us)
{
	uint64_t tick = clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) *
						us / 1000;
	uint32_t tries = DEFAULT_TRY_TIMES;
	uint64_t delta = tick / tries;

	if (!delta) {
		delta = us;
		tries = 1;
	}

	while ((io_reg_read(reg) & mask) != val) {
		if (!tries--) {
			trace_error(TRACE_CLASS_WAIT, "ewt");
			return -EIO;
		}
		wait_delay(delta);
	}
	return 0;
}
