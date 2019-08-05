// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/schedule/task.h>
#include <stdint.h>
#include <sof/lib/wait.h>
#include "testbench/edf_schedule.h"
#include "testbench/ll_schedule.h"

static const struct scheduler_ops *schedulers[SOF_SCHEDULE_COUNT] = {
	&schedule_edf_ops,              /* SOF_SCHEDULE_EDF */
	&schedule_ll_ops		/* SOF_LL_TASK */
};

/* testbench work definition */
int schedule_task_init(struct task *task, uint16_t type, uint16_t priority,
		       enum task_state (*func)(void *data), void *data,
		       uint16_t core, uint32_t flags)
{
	task->type = type;
	task->priority = priority;
	task->core = core;
	task->state = SOF_TASK_STATE_INIT;
	task->func = func;
	task->data = data;

	if (schedulers[task->type]->schedule_task_init)
		return schedulers[task->type]->schedule_task_init(task);
	else
		return -ENOENT;
}

int scheduler_init(void)
{
	int i = 0;
	int ret = 0;

	for (i = 0; i < SOF_SCHEDULE_COUNT; i++) {
		if (schedulers[i]->scheduler_init) {
			ret = schedulers[i]->scheduler_init();
			if (ret < 0)
				goto out;
		}
	}
out:
	return ret;
}

void schedule_free(void)
{
	int i;

	for (i = 0; i < SOF_SCHEDULE_COUNT; i++) {
		if (schedulers[i]->scheduler_free)
			schedulers[i]->scheduler_free();
	}
}

void schedule(void)
{
	int i = 0;

	for (i = 0; i < SOF_SCHEDULE_COUNT; i++) {
		if (schedulers[i]->scheduler_run)
			schedulers[i]->scheduler_run();
	}
}
