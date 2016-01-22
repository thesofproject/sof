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
#include <reef/ipc.h>
#include <platform/interrupt.h>
#include <platform/shim.h>
#include <reef/audio/pipeline.h>
#include <reef/work.h>
#include <reef/debug.h>
#include <reef/trace.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

/* not accurate on Qemu yet since Qemu clock is not aligned with firmware yet. */
// TODO: align Qemu clock with DSP.
#define AUDIO_WORK_MSECS	500

static struct work audio_work;

/* TODO only run this work when we have active audio pipelines */
uint32_t work_handler(void *data)
{
	/* process our audio pipelines */
	pipeline_do_work();

	/* TODO add support to scale clocks/wait time here */
	return AUDIO_WORK_MSECS;
}

int do_task(void)
{
	sys_comp_init();

	/* init default audio components */
	sys_comp_dai_init();
	sys_comp_host_init();
	sys_comp_mixer_init();
	sys_comp_mux_init();
	sys_comp_switch_init();
	sys_comp_volume_init();

	/* init static pipeline */
	init_static_pipeline();

	/* schedule our audio work */
	work_init((&audio_work), work_handler, NULL);
	work_schedule_default(&audio_work, AUDIO_WORK_MSECS);

	while (1) {
		// TODO: combine irq_syn into WFI()
		wait_for_interrupt(0);
		interrupt_enable_sync();

		/* now process any IPC messages from host */
		ipc_process_msg_queue();
	}

	/* something bad happened */
	return -EIO;
}
