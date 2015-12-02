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

#include <reef/list.h>
#include <stdint.h>

/* work queue pending window size. TODO: fine tune */
#define REEF_WORK_WINDOW	100

struct work_queue;

struct work {
	void (*cb)(void*);
	void *cb_data;
	struct list_head list;
	uint32_t count;
	uint32_t pending;
};

#define work_init(work, x, xd) \
	work->cb = x; \
	work->cb_data = xd;

/* schedule work on work queue */
void work_schedule(struct work_queue *queue, struct work *w, int timeout);

/* schedule work on default system work queue */
void work_schedule_default(struct work *work, int timeout);

/* init system workq */
void init_system_workq(void);

#endif
