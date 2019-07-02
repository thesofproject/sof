// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/task.h>
#include <stdint.h>
#include <sof/wait.h>
#include <sof/edf_schedule.h>
#include <sof/ll_schedule.h>

static const struct scheduler_ops *schedulers[SOF_SCHEDULE_COUNT] = {
	&schedule_edf_ops,              /* SOF_SCHEDULE_EDF */
	&schedule_ll_ops		/* SOF_LL_TASK */
};

/* testbench work definition */
int schedule_task_init(struct task *task, uint16_t type, uint16_t priority,
		       uint64_t (*func)(void *data), void *data, uint16_t core,
		       uint32_t xflags)
{
	task->type = type;
	task->priority = priority;
	task->core = core;
	task->state = SOF_TASK_STATE_INIT;
	task->func = func;
	task->data = data;

	if (schedulers[task->type]->schedule_task_init)
		return schedulers[task->type]->schedule_task_init(task, xflags);
	else
		return -ENOENT;
}

void schedule_task(struct task *task, uint64_t start, uint64_t deadline,
		   uint32_t flags)
{
	if (schedulers[task->type]->schedule_task)
		schedulers[task->type]->schedule_task(task, start, deadline,
			flags);
}

void schedule_task_free(struct task *task)
{
	if (schedulers[task->type]->schedule_task_free)
		schedulers[task->type]->schedule_task_free(task);
}

void reschedule_task(struct task *task, uint64_t start)
{
	if (schedulers[task->type]->reschedule_task)
		schedulers[task->type]->reschedule_task(task, start);
}

int schedule_task_cancel(struct task *task)
{
	int ret = 0;

	if (schedulers[task->type]->schedule_task_cancel)
		ret = schedulers[task->type]->schedule_task_cancel(task);

	return ret;
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
