/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * Generic audio task.
 */

#include <reef/task.h>
#include <reef/wait.h>
#include <reef/debug.h>
#include <reef/timer.h>
#include <reef/interrupt.h>
#include <stdint.h>
#include <stdlib.h>

static int ticks = 0;

/* test code to check working IRQ */
static void irq_handler(void *arg)
{
	dbg();
}

/* test code to check working IRQ */
static void timer_handler(void *arg)
{
	ticks++;
	dbg_val(ticks);
}

int do_task(int argc, char *argv[])
{
	int ret = 0;

	timer_register(0, timer_handler, NULL);
	timer_set(0, 10000000);
	timer_enable(0);

	while (1)
	{
	//	ipc_process_queue();

		wait_for_interrupt(0);

		timer_set(0, 10000000);
		timer_enable(0);
	}

	/* something bad happened */
	return ret;
}
