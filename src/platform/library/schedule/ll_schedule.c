// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#define _GNU_SOURCE

#include <sof/audio/component.h>
#include <rtos/task.h>
#include <sof/schedule/schedule.h>
#include <platform/lib/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <rtos/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>

 /* scheduler testbench definition */

/* 9f130ed8-2bbf-421c-836a-d5269147c9e7 */
SOF_DEFINE_UUID("ll_sched_lib", ll_sched_lib_uuid, 0x9f130ed8, 0x2bbf, 0x421c,
		0x83, 0x6a, 0xd5, 0x26, 0x91, 0x47, 0xc9, 0xe7);

DECLARE_TR_CTX(ll_tr, SOF_UUID(ll_sched_lib_uuid), LOG_LEVEL_INFO);

/* list of all tasks */
static struct list_item sched_list;

void schedule_ll_run_tasks(void)
{
	struct list_item *tlist, *tlist_;
	struct task *task;

	/* list empty then return */
	if (list_is_empty(&sched_list))
		fprintf(stdout, "LL scheduler thread exit - list empty\n");

	/* iterate through the task list */
	list_for_item_safe(tlist, tlist_, &sched_list) {
		task = container_of(tlist, struct task, list);

		/* only run queued tasks */
		if (task->state == SOF_TASK_STATE_QUEUED) {
			task->state = SOF_TASK_STATE_RUNNING;

			task->ops.run(task->data);

			/* only re-queue if not cancelled */
			if (task->state == SOF_TASK_STATE_RUNNING)
				task->state = SOF_TASK_STATE_QUEUED;

		}
	}
}

/* schedule new LL task */
static int schedule_ll_task(void *data, struct task *task, uint64_t start,
			    uint64_t period)
{
	/* add task to list */
	list_item_prepend(&task->list, &sched_list);
	task->state = SOF_TASK_STATE_QUEUED;
	task->start = 0;

	return 0;
}

static void ll_scheduler_free(void *data, uint32_t flags)
{
	free(data);
}

/* TODO: scheduler free and cancel APIs can merge as part of Zephyr */
static int schedule_ll_task_cancel(void *data, struct task *task)
{
	/* delete task */
	task->state = SOF_TASK_STATE_CANCEL;
	list_item_del(&task->list);

	return 0;
}

/* TODO: scheduler free and cancel APIs can merge as part of Zephyr */
static int schedule_ll_task_free(void *data, struct task *task)
{
	task->state = SOF_TASK_STATE_FREE;
	list_item_del(&task->list);

	return 0;
}

static struct scheduler_ops schedule_ll_ops = {
	.schedule_task		= schedule_ll_task,
	.schedule_task_running	= NULL,
	.reschedule_task	= NULL,
	.schedule_task_cancel	= schedule_ll_task_cancel,
	.schedule_task_free	= schedule_ll_task_free,
	.scheduler_free		= ll_scheduler_free,
};

int schedule_task_init_ll(struct task *task,
			  const struct sof_uuid_entry *uid, uint16_t type,
			  uint16_t priority, enum task_state (*run)(void *data),
			  void *data, uint16_t core, uint32_t flags)
{
	return schedule_task_init(task, uid, SOF_SCHEDULE_LL_TIMER, 0, run,
				  data, core, flags);
}

/* initialize scheduler */
int scheduler_init_ll(struct ll_schedule_domain *domain)
{
	tr_info(&ll_tr, "ll_scheduler_init()");

	list_init(&sched_list);
	scheduler_init(SOF_SCHEDULE_LL_TIMER, &schedule_ll_ops, NULL);

	return 0;
}
