// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/task.h>
#include <stdint.h>
#include <sof/edf_schedule.h>
#include <sof/wait.h>

#include <kernel.h>

struct k_work_q edf_workq;
K_THREAD_STACK_DEFINE(edf_workq_stack, 8192);

static void edf_work_handler(struct k_work *work)
{
	struct task *task = CONTAINER_OF(work, struct task, z_delayed_work);

	task->state = SOF_TASK_STATE_RUNNING;

	if (task->func)
		task->func(task->data);

	task->state = SOF_TASK_STATE_COMPLETED;
}

/* schedule task */
static void schedule_edf_task(struct task *task, uint64_t start,
			      uint64_t deadline, uint32_t flags)
{
	k_delayed_work_submit_to_queue_us(&edf_workq,
					  &task->z_delayed_work,
					  start);

	task->state = SOF_TASK_STATE_QUEUED;
}

static int schedule_edf_task_init(struct task *task, uint32_t xflags)
{
	k_delayed_work_init(&task->z_delayed_work, edf_work_handler);

	return 0;
}

/* initialize scheduler */
static int edf_scheduler_init(void)
{
	trace_edf_sch("edf_scheduler_init()");

	k_work_q_start(&edf_workq,
		       edf_workq_stack,
		       K_THREAD_STACK_SIZEOF(edf_workq_stack),
		       -1);
	k_thread_name_set(&edf_workq.thread, "edf_workq");

	return 0;
}

static int schedule_edf_task_cancel(struct task *task)
{
	int ret = 0;

	if (task->state == SOF_TASK_STATE_QUEUED) {
		ret = k_delayed_work_cancel(&task->z_delayed_work);

		/* delete task */
		task->state = SOF_TASK_STATE_CANCEL;
	}

	return ret;
}

static void schedule_edf_task_free(struct task *task)
{
	task->state = SOF_TASK_STATE_FREE;
	task->func = NULL;
	task->data = NULL;
}

struct scheduler_ops schedule_edf_ops = {
	.schedule_task		= schedule_edf_task,
	.schedule_task_init	= schedule_edf_task_init,
	.schedule_task_running	= NULL,
	.schedule_task_complete = NULL,
	.reschedule_task	= NULL,
	.schedule_task_cancel	= schedule_edf_task_cancel,
	.schedule_task_free	= schedule_edf_task_free,
	.scheduler_init		= edf_scheduler_init,
	.scheduler_free		= NULL,
	.scheduler_run		= NULL,
};
