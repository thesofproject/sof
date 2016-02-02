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
#define AUDIO_WORK_MSECS	25

struct audio_data {
	struct work audio_work;
	uint32_t count;
	struct pipeline *p;
};

/* TODO only run this work when we have active audio pipelines */
uint32_t work_handler(void *data)
{
	struct audio_data *pdata = (struct audio_data*)data;

	dbg_val_at(++pdata->count, 20);

	/* process our audio pipelines */
	pipeline_do_work(pdata->p);

	/* TODO add support to scale clocks/wait time here */
	return AUDIO_WORK_MSECS;
}

int do_task(void)
{
	struct audio_data pdata;

	/* init default audio components */
	sys_comp_init();
	sys_comp_dai_init();
	sys_comp_host_init();
	sys_comp_mixer_init();
	sys_comp_mux_init();
	sys_comp_switch_init();
	sys_comp_volume_init();

	/* init static pipeline */
	pdata.p = init_static_pipeline();
	if (pdata.p == NULL)
		panic(PANIC_TASK);

	/* schedule our audio work */
	work_init((&pdata.audio_work), work_handler, &pdata);
	work_schedule_default(&pdata.audio_work, AUDIO_WORK_MSECS);

	/* let host know DSP boot is complete */
	platform_boot_complete(0);

	/* main audio IPC processing loop */
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
