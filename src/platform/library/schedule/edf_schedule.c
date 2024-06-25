// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

#include <sof/audio/component.h>
#include <rtos/task.h>
#include <stdint.h>
#include <sof/schedule/edf_schedule.h>
#include <rtos/wait.h>
#include <stdlib.h>

 /* scheduler testbench definition */

/* 5dbc3672-e290-43d8-91f8-81aafe453d5b */
SOF_DEFINE_UUID("edf_sched_lib", edf_sched_lib_uuid, 0x5dbc3672, 0xe290, 0x43d8,
		0x91, 0xf8, 0x81, 0xaa, 0xfe, 0x45, 0x3d, 0x5b);

DECLARE_TR_CTX(edf_tr, SOF_UUID(edf_sched_lib_uuid), LOG_LEVEL_INFO);

struct edf_schedule_data {
	struct list_item list; /* list of tasks in priority queue */
	uint32_t clock;
};

static struct edf_schedule_data *sch;

static int schedule_edf_task_complete(struct task *task)
{
	list_item_del(&task->list);
	task->state = SOF_TASK_STATE_COMPLETED;

	return 0;
}

/* schedule task */
static int schedule_edf_task(void *data, struct task *task, uint64_t start,
			      uint64_t period)
{
	struct edf_schedule_data *sched = data;
	(void)period;
	list_item_prepend(&task->list, &sched->list);
	task->state = SOF_TASK_STATE_QUEUED;

	if (task->ops.run)
		task->ops.run(task->data);

	schedule_edf_task_complete(task);

	return 0;
}

static void edf_scheduler_free(void *data, uint32_t flags)
{
	free(data);
}

static int schedule_edf_task_cancel(void *data, struct task *task)
{
	if (task->state == SOF_TASK_STATE_QUEUED) {
		/* delete task */
		task->state = SOF_TASK_STATE_CANCEL;
		list_item_del(&task->list);
	}

	return 0;
}

static int schedule_edf_task_free(void *data, struct task *task)
{
	task->state = SOF_TASK_STATE_FREE;
	task->ops.run = NULL;
	task->data = NULL;

	free(edf_sch_get_pdata(task));
	edf_sch_set_pdata(task, NULL);

	return 0;
}

static struct scheduler_ops schedule_edf_ops = {
	.schedule_task		= schedule_edf_task,
	.schedule_task_running	= NULL,
	.reschedule_task	= NULL,
	.schedule_task_cancel	= schedule_edf_task_cancel,
	.schedule_task_free	= schedule_edf_task_free,
	.scheduler_free		= edf_scheduler_free,
};

int schedule_task_init_edf(struct task *task, const struct sof_uuid_entry *uid,
			   const struct task_ops *ops, void *data,
			   uint16_t core, uint32_t flags)
{
	struct edf_task_pdata *edf_pdata;
	int ret = 0;

	ret = schedule_task_init(task, uid, SOF_SCHEDULE_EDF, 0, ops->run,
				 data, core, flags);
	if (ret < 0)
		return ret;

	edf_pdata = malloc(sizeof(*edf_pdata));
	edf_sch_set_pdata(task, edf_pdata);

	task->ops.complete = ops->complete;

	return 0;
}

/* initialize scheduler */
int scheduler_init_edf(void)
{
	tr_info(&edf_tr, "edf_scheduler_init()");
	sch = malloc(sizeof(*sch));
	list_init(&sch->list);

	scheduler_init(SOF_SCHEDULE_EDF, &schedule_edf_ops, sch);

	return 0;
}
