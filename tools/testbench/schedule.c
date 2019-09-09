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

static struct schedule_data *sch;

struct schedulers **arch_schedulers_get(void)
{
	return NULL;
}

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

	if (task->type == sch->type && sch->ops->schedule_task_init)
		return sch->ops->schedule_task_init(sch->data, task);
	else
		return -ENOENT;
}

void scheduler_init(int type, const struct scheduler_ops *ops, void *data)
{
	sch = malloc(sizeof(*sch));
	list_init(&sch->list);
	sch->type = type;
	sch->ops = ops;
	sch->data = data;
}
