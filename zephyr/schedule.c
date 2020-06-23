// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

/* Generic scheduler */
#include <sof/schedule/schedule.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/lib/alloc.h>
#include <ipc/topology.h>

#if 0

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

#endif

struct scheduler_wrap {
	struct scheduler_ops *ops;
	void *data;
};

static struct scheduler_wrap sched[SOF_SCHEDULE_COUNT];

int schedule_task_init(struct task *task,
		       uint32_t uid, uint16_t type, uint16_t priority,
		       enum task_state (*run)(void *data), void *data,
		       uint16_t core, uint32_t flags)
{
	if (type >= SOF_SCHEDULE_COUNT) {
		return -EINVAL;
	}

	task->uid = uid;
	task->type = type;
	task->priority = priority;
	task->core = core;
	task->flags = flags;
	task->state = SOF_TASK_STATE_INIT;
	task->data = data;
	task->sops = sched[type].ops;

	return 0;
}

void scheduler_init(int type, struct scheduler_ops *ops, void *data)
{
	//if (type < 0 || > type >= SOF_SCHEDULE_COUNT)
	//	panic();
	sched[type].ops = ops;
	sched[type].data = data;
}

void *scheduler_get_data(uint16_t type)
{
	return sched[type].data;
}
