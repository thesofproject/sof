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

int schedule_task_init(struct task *task,
		       const struct sof_uuid_entry *uid, uint16_t type,
		       uint16_t priority, enum task_state (*run)(void *data),
		       void *data, uint16_t core, uint32_t flags)
{
	if (type >= SOF_SCHEDULE_COUNT)
		return -EINVAL;

	task->uid = uid;
	task->type = SOF_SCHEDULE_EDF; /* Note: Force EDF scheduler */
	task->priority = priority;
	task->core = core;
	task->flags = flags;
	task->state = SOF_TASK_STATE_INIT;
	task->ops.run = run;
	task->data = data;

	return 0;
}

static void scheduler_register(struct schedule_data *scheduler)
{
	struct schedulers **sch = arch_schedulers_get();

	if (!*sch) {
		/* init schedulers list */
		*sch = calloc(1, sizeof(**sch));
		list_init(&(*sch)->list);
	}

	list_item_append(&scheduler->list, &(*sch)->list);
}

static void scheduler_unregister(struct schedule_data *scheduler)
{
	struct schedulers **sch = arch_schedulers_get();

	list_item_del(&scheduler->list);

	if (list_is_empty(&(*sch)->list))
		free(*sch);
}

void scheduler_init(int type, const struct scheduler_ops *ops, void *data)
{
	struct schedule_data *sch;

	sch = calloc(1, sizeof(*sch));
	list_init(&sch->list);
	sch->type = SOF_SCHEDULE_EDF; /* Note: Force EDF scheduler */
	sch->ops = ops;
	sch->data = data;

	scheduler_register(sch);
}

void scheduler_free(void *data)
{
	struct schedulers **schedulers = arch_schedulers_get();
	struct list_item *slist;
	struct schedule_data *sch;

	list_for_item(slist, &(*schedulers)->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (sch->data == data) {
			/* found the scheduler, free it */
			scheduler_unregister(sch);
			free(sch);
			break;
		}
	}
}
