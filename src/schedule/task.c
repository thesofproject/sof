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
#include <sof/lib/memory.h>
#include <sof/lib/wait.h>
#include <sof/platform.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stddef.h>
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

static uint64_t task_main_deadline(void *data)
{
	return SOF_TASK_DEADLINE_IDLE;
}

enum task_state task_main_master_core(void *data)
{
	struct ipc *ipc = ipc_get();

	/* main audio processing loop */
	while (1) {
		/* sleep until next IPC or DMA */
		wait_for_interrupt(0);

		if (!ipc->pm_prepare_D3)
			ipc_send_queued_msg();

		platform_shared_commit(ipc, sizeof(*ipc));
	}

	return SOF_TASK_STATE_COMPLETED;
}

void task_main_init(void)
{
	struct task **main_task = task_main_get();
	int cpu = cpu_get_id();
	int ret;
	task_main main_main = cpu == PLATFORM_MASTER_CORE_ID ?
		&task_main_master_core : &task_main_slave_core;
	struct task_ops ops = {
		.run = main_main,
		.get_deadline = task_main_deadline,
	};

	*main_task = rzalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM,
			     sizeof(**main_task));

	ret = schedule_task_init_edf(*main_task, &ops, NULL, cpu, 0);
	assert(!ret);
}

void task_main_free(void)
{
	schedule_task_free(*task_main_get());
}

int task_main_start(struct sof *sof)
{
	int ret;

	/* init default audio components */
	sys_comp_init(sof);

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
