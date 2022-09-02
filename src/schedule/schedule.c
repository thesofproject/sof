// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

/* Generic scheduler */

#include <rtos/alloc.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdint.h>

LOG_MODULE_REGISTER(schedule, CONFIG_SOF_LOG_LEVEL);

/* 3dee06de-f25a-4e10-ae1f-abc9573873ea */
DECLARE_SOF_UUID("schedule", sch_uuid, 0x3dee06de, 0xf25a, 0x4e10,
		 0xae, 0x1f, 0xab, 0xc9, 0x57, 0x38, 0x73, 0xea);

DECLARE_TR_CTX(sch_tr, SOF_UUID(sch_uuid), LOG_LEVEL_INFO);

int schedule_task_init(struct task *task,
		       const struct sof_uuid_entry *uid, uint16_t type,
		       uint16_t priority, enum task_state (*run)(void *data),
		       void *data, uint16_t core, uint32_t flags)
{
	if (type >= SOF_SCHEDULE_COUNT) {
		tr_err(&sch_tr, "schedule_task_init(): invalid task type");
		return -EINVAL;
	}

	task->uid = uid;
	task->type = type;
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
		*sch = rzalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM,
			       sizeof(**sch));
		list_init(&(*sch)->list);
	}

	list_item_append(&scheduler->list, &(*sch)->list);
}

void scheduler_init(int type, const struct scheduler_ops *ops, void *data)
{
	struct schedule_data *sch;

	if (!ops || !ops->schedule_task || !ops->schedule_task_cancel ||
	    !ops->schedule_task_free)
		return;

	sch = rzalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM, sizeof(*sch));
	list_init(&sch->list);
	sch->type = type;
	sch->ops = ops;
	sch->data = data;

	scheduler_register(sch);
}
