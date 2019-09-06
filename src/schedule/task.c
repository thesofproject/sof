// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/*
 * Generic audio task.
 */

#include <sof/audio/component.h>
#include <sof/debug/panic.h>
#include <sof/drivers/ipc.h>
#include <sof/lib/alloc.h>
#include <sof/lib/agent.h>
#include <sof/lib/cpu.h>
#include <sof/lib/wait.h>
#include <sof/platform.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdint.h>

#if STATIC_PIPE
#include <sof/audio/pipeline.h>
#include <ipc/trace.h>
#endif

typedef enum task_state (*task_main)(void *);

static void sys_module_init(void)
{
	intptr_t *module_init = (intptr_t *)(&_module_init_start);

	for (; module_init < (intptr_t *)&_module_init_end; ++module_init)
		((void(*)(void))(*module_init))();
}

extern int __dsp_printf(char *format, ...);

enum task_state task_main_master_core(void *data)
{
	struct sof *sof = data;

	/* main audio processing loop */
	while (1) {
		/* sleep until next IPC or DMA */
		sa_enter_idle(sof);
		__dsp_printf("task_main_master_core() idle\n");
		wait_for_interrupt(0);

		/* now process any IPC messages to host */
		ipc_process_msg_queue();
	}

	return SOF_TASK_STATE_COMPLETED;
}

void task_main_init(struct sof *sof)
{
	struct task **main_task = task_main_get();
	int cpu = cpu_get_id();
	task_main main_main = cpu == PLATFORM_MASTER_CORE_ID ?
		&task_main_master_core : &task_main_slave_core;

	*main_task = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(**main_task));

	assert(!schedule_task_init(*main_task, SOF_SCHEDULE_EDF,
				   SOF_TASK_PRI_IDLE, main_main, sof, cpu,
				   SOF_SCHEDULE_FLAG_IDLE));
}

void task_main_free(void)
{
	schedule_task_free(*task_main_get());
}

int task_main_start(struct sof *sof)
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

	/* task initialized in edf_scheduler_init */
	schedule_task(*task_main_get(), 0, UINT64_MAX);

	/* something bad happened */
	return -EIO;
}
