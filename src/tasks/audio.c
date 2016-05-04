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

struct audio_data {
	struct pipeline *p;
};

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
