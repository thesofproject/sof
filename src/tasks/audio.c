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
#include <platform/interrupt.h>
#include <platform/shim.h>
#include <stdint.h>
#include <stdlib.h>

static int ticks = 0;
static int ipc = 1000;

static void do_cmd(void)
{
	dbg();
}

static void do_notify(void)
{
	dbg();
}

/* test code to check working IRQ */
static void irq_handler(void *arg)
{
	uint32_t isr;

	ipc++;
	dbg_val(ipc);

	/* Interrupt arrived, check src */
	isr = shim_read(SHIM_ISRD);

	if (isr & SHIM_ISRD_DONE) {

		/* Mask Done interrupt before return */
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) | SHIM_IMRD_DONE);
		do_notify();
	}

	if (isr & SHIM_ISRD_BUSY) {
		
		/* Mask Busy interrupt before return */
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) | SHIM_IMRD_BUSY);
		do_cmd();
	}

	interrupt_clear(IRQ_NUM_EXT_IA);
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

	interrupt_register(IRQ_NUM_EXT_IA, irq_handler, NULL);
	interrupt_enable(IRQ_NUM_EXT_IA);

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
