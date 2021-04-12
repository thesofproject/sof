// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/*
 * Generic audio task.
 */

#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
#include <sof/lib/alloc.h>
#include <sof/lib/agent.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
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

typedef enum task_state (*task_main)(void *);

/* 37f1d41f-252d-448d-b9c4-1e2bee8e1bf1 */
DECLARE_SOF_UUID("main-task", main_task_uuid, 0x37f1d41f, 0x252d, 0x448d,
		 0xb9, 0xc4, 0x1e, 0x2b, 0xee, 0x8e, 0x1b, 0xf1);

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

enum task_state task_main_primary_core(void *data)
{
	struct ipc *ipc = ipc_get();

	/* main audio processing loop */
	while (1) {
		/* sleep until next IPC or DMA */
		wait_for_interrupt(0);

		if (!ipc->pm_prepare_D3)
			ipc_send_queued_msg();

	}

	return SOF_TASK_STATE_COMPLETED;
}

void task_main_init(void)
{
	struct task **main_task = task_main_get();
	int cpu = cpu_get_id();
	int ret;
	task_main main_main = cpu == PLATFORM_PRIMARY_CORE_ID ?
		&task_main_primary_core : &task_main_secondary_core;
	struct task_ops ops = {
		.run = main_main,
		.get_deadline = task_main_deadline,
	};

	*main_task = rzalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM,
			     sizeof(**main_task));

	ret = schedule_task_init_edf(*main_task, SOF_UUID(main_task_uuid),
				     &ops, NULL, cpu, 0);
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

	/* init pipeline position offsets */
	pipeline_posn_init(sof);

	/* let host know DSP boot is complete */
	ret = platform_boot_complete(0);
	if (ret < 0)
		return ret;

	/* task initialized in edf_scheduler_init */
	schedule_task(*task_main_get(), 0, UINT64_MAX);

	/* something bad happened */
	return -EIO;
}
