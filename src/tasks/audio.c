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
#include <reef/audio/pipeline.h>
#include <reef/work.h>
#include <reef/debug.h>
#include <stdint.h>
#include <stdlib.h>

/* not accurate on Qemu yet since Qemu clock is not aligned with firmware yet. */
// TODO: align Qemu clock with DSP.
#define AUDIO_WORK_MSECS	125

static int ticks = 0;

static struct work audio_work;

uint32_t work_handler(void *data)
{
	ticks++;
	dbg_val(ticks);

	return AUDIO_WORK_MSECS;
}

int do_task(int argc, char *argv[])
{
	int ret = 0;

	/* init static pipeline */
	init_static_pipeline();
	
	/* schedule our audio work */
	work_init((&audio_work), work_handler, NULL);
	work_schedule_default(&audio_work, AUDIO_WORK_MSECS);

	while (1) {
	//	ipc_process_queue();

		// TODO: combine irq_syn into WFI()
		wait_for_interrupt(0);
		interrupt_enable_sync();
	}

	/* something bad happened */
	return ret;
}
