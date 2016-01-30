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
#include <reef/work.h>
#include <reef/timer.h>
#include <reef/interrupt.h>

typedef struct {
	volatile uint32_t c;
	struct work work;
	uint32_t timeout;
} completion_t;

void wait_for_interrupt(int level);

static inline void wait_completed(completion_t *comp)
{
	comp->c = 1;
}

static inline void wait_init(completion_t *comp)
{
	comp->c = 0;
}

/* simple interrupt based wait for completion */
static inline void wait_for_completion(completion_t *comp)
{
	/* check for completion after every wake from IRQ */
	while (comp->c == 0)
		wait_for_interrupt(0);
}

static uint32_t _wait_cb(void *data)
{
	completion_t *wc = (completion_t*)data;

	wc->timeout = 1;
	return 0;
}

/* simple interrupt based wait for completion with timeout */
static inline int wait_for_completion_timeout(completion_t *comp)
{
	wait_init(comp);
	work_init(&comp->work, _wait_cb, comp);
	work_schedule_default(&comp->work, comp->timeout);
	comp->timeout = 0;

	/* check for completion after every wake from IRQ */
	while (comp->c == 0 && comp->timeout == 0) {
		wait_for_interrupt(0);
		interrupt_enable_sync();
	}

	/* did we timeout */
	if (comp->timeout == 0) {
		/* no timeout so cancel work and return 0 */
		work_cancel_default(&comp->work);
		return 0;
	} else {
		/* timeout */
		return -ETIME;
	}
}

#endif
