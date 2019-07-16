// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/*
 * Generic audio task.
 */

#include <sof/audio/component.h>
#include <sof/drivers/ipc.h>
#include <sof/lib/agent.h>
#include <sof/lib/wait.h>
#include <sof/platform.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <errno.h>
#include <stdint.h>

#if STATIC_PIPE
#include <sof/audio/pipeline.h>
#include <sof/debug/panic.h>
#include <ipc/trace.h>
#endif

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
