// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

/* Generic scheduler */
#include <sof/schedule.h>
#include <sof/edf_schedule.h>
#include <sof/ll_schedule.h>

static const struct scheduler_ops *schedulers[SOF_SCHEDULE_COUNT] = {
	&schedule_edf_ops,              /* SOF_SCHEDULE_EDF */
	&schedule_ll_ops		/* SOF_SCHEDULE_LL */
};

int schedule_task_init(struct task *task, uint16_t type, uint16_t priority,
		       uint64_t (*func)(void *data), void *data, uint16_t core,
		       uint32_t xflags)
{
	int ret = 0;

	if (type >= SOF_SCHEDULE_COUNT) {
		trace_schedule_error("schedule_task_init() error: "
				     "invalid task type");
		ret = -EINVAL;
		goto out;
	}

	task->type = type;
	task->priority = priority;
	task->core = core;
	task->state = SOF_TASK_STATE_INIT;
	task->func = func;
	task->data = data;
	task->ops = schedulers[task->type];

	if (task->ops->schedule_task_init)
		ret = task->ops->schedule_task_init(task, xflags);
out:
	return ret;
}

void schedule_task_free(struct task *task)
{
	if (task->ops->schedule_task_free)
		task->ops->schedule_task_free(task);
}

void schedule_task(struct task *task, uint64_t start, uint64_t deadline,
		   uint32_t flags)
{
	if (task->ops->schedule_task)
		task->ops->schedule_task(task, start, deadline, flags);
}

void reschedule_task(struct task *task, uint64_t start)
{
	if (task->ops->reschedule_task)
		task->ops->reschedule_task(task, start);
}

int schedule_task_cancel(struct task *task)
{
	if (task->ops->schedule_task_cancel)
		return task->ops->schedule_task_cancel(task);
	return 0;
}

void schedule_task_running(struct task *task)
{
	if (task->ops->schedule_task_running)
		task->ops->schedule_task_running(task);
}

void schedule_task_complete(struct task *task)
{
	if (task->ops->schedule_task_complete)
		task->ops->schedule_task_complete(task);
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
