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

typedef volatile uint32_t completion_t;

void wait_for_interrupt(int level);

static inline void wait_completed(completion_t *c)
{
	*c = 1;
}

static inline void wait_init(completion_t *c)
{
	*c = 0;
}

/* simple interrupt based wait for completion */
static inline void wait_for_completion(completion_t *c)
{
	/* check for completion after every wake from IRQ */
	while (*c == 0)
		wait_for_interrupt(0);
}

/* only used internally */
struct _wait_completion {
	completion_t c;
};

static uint32_t _wait_cb(void *data)
{
	struct _wait_completion *wc = (struct _wait_completion*)data;

	wait_completed(&wc->c);
	return 0;
}

/* simple interrupt based wait for completion with timeout */
static inline int wait_for_completion_timeout(completion_t *c, int timeout)
{
	struct work work;
	struct _wait_completion timed_out;

	wait_init(&timed_out.c);
	work_init(&work, _wait_cb, &timed_out);
	work_schedule_default(&work, timeout);

	/* check for completion after every wake from IRQ */
	while (*c == 0 && timed_out.c == 0)
		wait_for_interrupt(0);

	/* did we timeout */
	if (timed_out.c == 0) {

		/* no timeout so cancel work and return 0 */
		work_cancel_default(&work);
		return 0;
	} else {

		/* timeout */
		return -ETIME;
	}
}

#endif
