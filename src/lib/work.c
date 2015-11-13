/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 */

#include <reef/work.h>
#include <reef/timer.h>

static void worker(void *data)
{
	struct work *work = (struct work *)data;

}
 
void work_schedule(struct work *work, int timeout)
{
	struct work *_w;

#if 0	

	struct timer_work *slot;
	uint32_t count;
	int i;

	count = timer_get_system();
	slot->data = data;
	timer_work->num_work++;
	slot->count = count + timeout; // TODO convert from ms
	slot->work = work;

	//if (timer->work
#endif
}

