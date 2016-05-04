/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_WAIT__
#define __INCLUDE_WAIT__

#include <stdint.h>
#include <errno.h>
#include <reef/debug.h>
#include <reef/work.h>
#include <reef/timer.h>
#include <reef/interrupt.h>
#include <reef/trace.h>
#include <platform/interrupt.h>

#define trace_wait() \
	tracev_event(TRACE_CLASS_WAIT, "WFI"); \
	tracev_value(interrupt_get_enabled()); \
	tracev_value(platform_interrupt_get_enabled());

typedef struct {
	uint32_t complete;
	struct work work;
	uint32_t timeout;
} completion_t;

void arch_wait_for_interrupt(int level);

static inline void wait_for_interrupt(int level)
{
	trace_wait();
	arch_interrupt_global_enable(0x0000ec04);
	arch_wait_for_interrupt(level);
}

static uint32_t _wait_cb(void *data, uint32_t delay)
{
	volatile completion_t *wc = (volatile completion_t*)data;

	wc->timeout = 1;
	return 0;
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
	work_init(&comp->work, _wait_cb, comp, WORK_ASYNC);
}

/* simple interrupt based wait for completion */
static inline void wait_for_completion(completion_t *comp)
{
	/* check for completion after every wake from IRQ */
	while (comp->complete == 0)
		wait_for_interrupt(0);
}


/* simple interrupt based wait for completion with timeout */
static inline int wait_for_completion_timeout(completion_t *comp)
{
	volatile completion_t *c = (volatile completion_t *)comp;

	work_schedule_default(&comp->work, comp->timeout);
	comp->timeout = 0;

	/* check for completion after every wake from IRQ */
	while (c->complete == 0 && c->timeout == 0) {
		wait_for_interrupt(0);
		interrupt_enable_sync();
	}

	/* did we timeout */
	if (c->timeout == 0) {
		/* no timeout so cancel work and return 0 */
		work_cancel_default(&comp->work);
		return 0;
	} else {
		/* timeout */
		return -ETIME;
	}
}

#endif
