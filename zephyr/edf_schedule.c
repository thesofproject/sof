// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/schedule/task.h>
#include <stdint.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/lib/wait.h>

#include <kernel.h>
#include <sys_clock.h>

struct k_work_q edf_workq;
K_THREAD_STACK_DEFINE(edf_workq_stack, 8192);

static void edf_work_handler(struct k_work *work)
{
	struct task *task = CONTAINER_OF(work, struct task, z_delayed_work);

	task->state = SOF_TASK_STATE_RUNNING;

	if (task->ops.run)
		task->ops.run(task->data);

	task->state = SOF_TASK_STATE_COMPLETED;
}

/* schedule task */
static int schedule_edf_task(struct task *task, uint64_t start,
			      uint64_t period)
{
	k_timeout_t start_time = K_USEC(start);

	k_delayed_work_submit_to_queue(&edf_workq,
				       &task->z_delayed_work,
					   start_time);

	task->state = SOF_TASK_STATE_QUEUED;
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

static int schedule_edf_task_free(struct task *task)
{
	task->state = SOF_TASK_STATE_FREE;
	task->ops.run = NULL;
	task->data = NULL;

	return 0;
}

const struct scheduler_ops schedule_edf_ops = {
	.schedule_task		= schedule_edf_task,
//	.schedule_task_running	= schedule_edf_task_running,
//	.schedule_task_complete = schedule_edf_task_complete,
	.reschedule_task	= NULL,
	.schedule_task_cancel	= schedule_edf_task_cancel,
	.schedule_task_free	= schedule_edf_task_free,
//	.scheduler_free		= scheduler_free_edf,
//	.scheduler_run		= schedule_edf
};

int scheduler_init_edf(void)
{
	trace_edf_sch("edf_scheduler_init()");

	k_work_q_start(&edf_workq,
		       edf_workq_stack,
		       K_THREAD_STACK_SIZEOF(edf_workq_stack),
		       -1);
	k_thread_name_set(&edf_workq.thread, "edf_workq");

	return 0;
}

int schedule_task_init_edf(struct task *task, uint32_t uid,
			   const struct task_ops *ops,
			   void *data, uint16_t core, uint32_t flags)
{
	int ret = 0;

	ret = schedule_task_init(task, uid, SOF_SCHEDULE_EDF, 0, ops->run, data,
				 core, flags);
	if (ret < 0)
		return ret;

	task->ops.complete = ops->complete;
	task->ops.get_deadline = ops->get_deadline;

	k_delayed_work_init(&task->z_delayed_work, edf_work_handler);

	return 0;
}
