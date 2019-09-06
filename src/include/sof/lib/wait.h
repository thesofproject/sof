/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/*
 * Simple wait for event completion and signaling with timeouts.
 */

#ifndef __SOF_LIB_WAIT_H__
#define __SOF_LIB_WAIT_H__

#include <arch/lib/wait.h>
#include <sof/drivers/timer.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <config.h>
#include <stdint.h>

typedef struct {
	uint32_t complete;
	struct task work;
	uint64_t timeout;
} completion_t;

static inline void wait_for_interrupt(int level)
{
	tracev_event(TRACE_CLASS_WAIT, "WFE");
#if CONFIG_DEBUG_LOCKS
	if (lock_dbg_atomic)
		trace_error_atomic(TRACE_CLASS_WAIT, "atm");
#endif
	arch_wait_for_interrupt(level);
	tracev_event(TRACE_CLASS_WAIT, "WFX");
}

static enum task_state _wait_cb(void *data)
{
	volatile completion_t *wc = (volatile completion_t *)data;

	wc->timeout = 1;

	return SOF_TASK_STATE_COMPLETED;
}

static inline uint32_t wait_is_completed(completion_t *comp)
{
	volatile completion_t *c = (volatile completion_t *)comp;

	return c->complete;
}

static inline void wait_completed(completion_t *comp)
{
	volatile completion_t *c = (volatile completion_t *)comp;

	c->complete = 1;
}

static inline void wait_init(completion_t *comp)
{
	volatile completion_t *c = (volatile completion_t *)comp;

	c->complete = 0;

	schedule_task_init(&comp->work, SOF_SCHEDULE_LL, SOF_TASK_PRI_MED,
			   _wait_cb, comp, 0, 0);
}

static inline void wait_clear(completion_t *comp)
{
	volatile completion_t *c = (volatile completion_t *)comp;

	c->complete = 0;
}

/* simple interrupt based wait for completion */
static inline void wait_for_completion(completion_t *comp)
{
	/* check for completion after every wake from IRQ */
	while (comp->complete == 0) {
		tracev_event(TRACE_CLASS_WAIT, "wait_for_completion");
		wait_for_interrupt(0);
	}
}

/**
 * \brief Waits at least passed number of clocks.
 * \param[in] number_of_clks Minimum number of clocks to wait.
 */
void wait_delay(uint64_t number_of_clks);

int wait_for_completion_timeout(completion_t *comp);
int poll_for_completion_delay(completion_t *comp, uint64_t us);
int poll_for_register_delay(uint32_t reg, uint32_t mask,
			    uint32_t val, uint64_t us);

#endif /* __SOF_LIB_WAIT_H__ */
