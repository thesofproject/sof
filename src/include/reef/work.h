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

struct work {
	void (*cb)(void*);
	void *cb_data;
	struct list_head list;
	uint32_t count;
};

#define work_init(work, x, xd) \
	work->cb = x; \
	work->cb_data = xd;

void work_schedule(struct work *work, int timeout);

#endif
