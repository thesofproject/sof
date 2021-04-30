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

/*
 * since only IPC is using the EDF scheduler - we schedule the work in the
 * next timer_domain time slice
 */
#define EDF_SCHEDULE_DELAY	0

static void edf_work_handler(struct k_work *work)
{
	struct task *task = CONTAINER_OF(work, struct task, z_delayed_work);

	task->state = SOF_TASK_STATE_RUNNING;
	task_run(task);

	task_complete(task);
	task->state = SOF_TASK_STATE_COMPLETED;
}

/* schedule task */
static int schedule_edf_task(void *data, struct task *task, uint64_t start,
			     uint64_t period)
{
	/* start time is microseconds from now */
	k_timeout_t start_time = K_USEC(start + EDF_SCHEDULE_DELAY);

	k_delayed_work_submit_to_queue(&edf_workq,
				       &task->z_delayed_work,
					   start_time);

	task->state = SOF_TASK_STATE_QUEUED;
	return 0;
}

static int schedule_edf_task_cancel(void *data, struct task *task)
{
	int ret = 0;

	if (task->state == SOF_TASK_STATE_QUEUED) {
		ret = k_delayed_work_cancel(&task->z_delayed_work);

		/* delete task */
		task->state = SOF_TASK_STATE_CANCEL;
	}

	return ret;
}

static int schedule_edf_task_complete(void *data, struct task *task)
{
	task_complete(task);
	return 0;
}

static int schedule_edf_task_running(void *data, struct task *task)
{
	task->state = SOF_TASK_STATE_RUNNING;
	return 0;
}

static int schedule_edf_task_free(void *data, struct task *task)
{
	task->state = SOF_TASK_STATE_FREE;
	task->ops.run = NULL;
	task->data = NULL;

	return 0;
}

static struct scheduler_ops schedule_edf_ops = {
	.schedule_task		= schedule_edf_task,
	.schedule_task_running	= schedule_edf_task_running,
	.schedule_task_complete = schedule_edf_task_complete,
	.schedule_task_cancel	= schedule_edf_task_cancel,
	.schedule_task_free	= schedule_edf_task_free,
};

int scheduler_init_edf(void)
{
	struct k_thread *thread = &edf_workq.thread;

	scheduler_init(SOF_SCHEDULE_EDF, &schedule_edf_ops, NULL);

	k_work_q_start(&edf_workq,
		       edf_workq_stack,
		       K_THREAD_STACK_SIZEOF(edf_workq_stack),
		       1);

	k_thread_suspend(thread);

	k_thread_cpu_mask_clear(thread);
	k_thread_cpu_mask_enable(thread, PLATFORM_PRIMARY_CORE_ID);
	k_thread_name_set(thread, "edf_workq");

	k_thread_resume(thread);

	return 0;
}

int schedule_task_init_edf(struct task *task, const struct sof_uuid_entry *uid,
			   const struct task_ops *ops,
			   void *data, uint16_t core, uint32_t flags)
{
	int ret = 0;

	ret = schedule_task_init(task, uid, SOF_SCHEDULE_EDF, 0, ops->run, data,
				 core, flags);
	if (ret < 0)
		return ret;

	task->ops = *ops;

	k_delayed_work_init(&task->z_delayed_work, edf_work_handler);

	return 0;
}

int schedule_task_init_edf_with_budget(struct task *task,
				       const struct sof_uuid_entry *uid,
				       const struct task_ops *ops,
				       void *data, uint16_t core,
				       uint32_t flags, uint32_t cycles_budget)
{
	return schedule_task_init_edf(task, uid, ops, data, core, flags);
}
