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
#include <stdlib.h>

static struct schedulers *testbench_schedulers_ptr; /* Initialized as NULL */

struct schedulers **arch_schedulers_get(void)
{
	return &testbench_schedulers_ptr;
}

int schedule_task_init(struct task *task, uint16_t type, uint16_t priority,
		       enum task_state (*run)(void *data), void *data,
		       uint16_t core, uint32_t flags)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;
	int ret = 0;

	if (type >= SOF_SCHEDULE_COUNT) {
		ret = -EINVAL;
		goto out;
	}

	task->type = type;
	task->priority = priority;
	task->core = core;
	task->flags = flags;
	task->state = SOF_TASK_STATE_INIT;
	task->run = run;
	task->data = data;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (type == sch->type && sch->ops->schedule_task_init)
			return sch->ops->schedule_task_init(sch->data, task);
	}

out:
	return ret;
}

static void scheduler_register(struct schedule_data *scheduler)
{
	struct schedulers **sch = arch_schedulers_get();

	if (!*sch) {
		/* init schedulers list */
		*sch = malloc(sizeof(**sch));
		list_init(&(*sch)->list);
	}

	list_item_append(&scheduler->list, &(*sch)->list);
}

void scheduler_init(int type, const struct scheduler_ops *ops, void *data)
{
	struct schedule_data *sch;

	sch = malloc(sizeof(*sch));
	list_init(&sch->list);
	sch->type = type;
	sch->ops = ops;
	sch->data = data;

	scheduler_register(sch);
}
