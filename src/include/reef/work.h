/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 */

#ifndef __INCLUDE_WORK__
#define __INCLUDE_WORK__

#include <stdint.h>
#include <reef/list.h>
#include <reef/timer.h>

/* work queue pending window size. TODO: fine tune */
#define REEF_WORK_WINDOW	100

struct work_queue;

struct work {
	uint32_t (*cb)(void*);	/* returns reschedule timeout in msecs */
	void *cb_data;
	struct list_head list;
	uint32_t count;
	uint32_t pending;
};

struct work_queue_timesource {
	int timer;
	int clk;
	void (*timer_set)(int timer, uint32_t ticks);
	void (*timer_clear)(int timer);
	uint32_t (*timer_get)(int timer);
};

#define work_init(w, x, xd) \
	(w)->cb = x; \
	(w)->cb_data = xd;

/* schedule/cancel work on work queue */
void work_schedule(struct work_queue *queue, struct work *w, int timeout);
void work_cancel(struct work_queue *queue, struct work *work);

/* schedule/cancel work on default system work queue */
void work_schedule_default(struct work *work, int timeout);
void work_cancel_default(struct work *work);

/* create new work queue */
struct work_queue *work_new_queue(const struct work_queue_timesource *ts);

/* init system workq */
void init_system_workq(const struct work_queue_timesource *ts);

#endif
