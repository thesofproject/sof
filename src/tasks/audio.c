// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/*
 * Generic audio task.
 */

#include <sof/task.h>
#include <sof/wait.h>
#include <sof/debug.h>
#include <sof/drivers/timer.h>
#include <sof/drivers/interrupt.h>
#include <sof/ipc.h>
#include <sof/agent.h>
#include <sof/idc.h>
#include <sof/platform.h>
#include <sof/audio/pipeline.h>
#include <sof/schedule/schedule.h>
#include <sof/debug.h>
#include <sof/trace.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

static void sys_module_init(void)
{
	intptr_t *module_init = (intptr_t *)(&_module_init_start);

	for (; module_init < (intptr_t *)&_module_init_end; ++module_init)
		((void(*)(void))(*module_init))();
}

int do_task_master_core(struct sof *sof)
{
	int ret;

	/* init default audio components */
	sys_comp_init();

	/* init self-registered modules */
	sys_module_init();

#if STATIC_PIPE
	/* init static pipeline */
	ret = init_static_pipeline(sof->ipc);
	if (ret < 0)
		panic(SOF_IPC_PANIC_TASK);
#endif
	/* let host know DSP boot is complete */
	ret = platform_boot_complete(0);
	if (ret < 0)
		return ret;

	/* main audio IPC processing loop */
	while (1) {
		/* sleep until next IPC or DMA */
		sa_enter_idle(sof);
		wait_for_interrupt(0);

		/* now process any IPC messages to host */
		ipc_process_msg_queue();

		/* schedule any idle tasks */
		schedule();
	}

	/* something bad happened */
	return -EIO;
}

int do_task_slave_core(struct sof *sof)
{
	/* main audio IDC processing loop */
	while (1) {
		/* sleep until next IDC */
		wait_for_interrupt(0);

		/* schedule any idle tasks */
		schedule();
	}

	/* something bad happened */
	return -EIO;
}
