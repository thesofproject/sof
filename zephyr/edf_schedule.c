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

#include <zephyr/kernel.h>
#include <zephyr/sys_clock.h>

static struct k_work_q edf_workq;
static K_THREAD_STACK_DEFINE(edf_workq_stack, CONFIG_STACK_SIZE_EDF);

/*
 * since only IPC is using the EDF scheduler - we schedule the work in the
 * next timer_domain time slice
 */
#define EDF_SCHEDULE_DELAY	0

static void edf_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct task *task = CONTAINER_OF(dwork, struct task, z_delayed_work);

	task->state = SOF_TASK_STATE_RUNNING;

	task->state = task_run(task);

	if (task->state == SOF_TASK_STATE_RESCHEDULE) {
		uint64_t deadline = task_get_deadline(task);
		uint64_t now = k_uptime_ticks();
		k_timeout_t timeout = K_MSEC(0);

		if (deadline > now)
			timeout = K_TICKS(deadline - now);

		k_work_reschedule_for_queue(&edf_workq,
					    &task->z_delayed_work,
					    timeout);
		task->state = SOF_TASK_STATE_QUEUED;
	} else {
		task_complete(task);
		task->state = SOF_TASK_STATE_COMPLETED;
	}
}

/* schedule task */
static int schedule_edf_task(void *data, struct task *task, uint64_t start,
			     uint64_t period)
{
	/* start time is microseconds from now */
	k_timeout_t start_time = K_USEC(start + EDF_SCHEDULE_DELAY);

	k_work_reschedule_for_queue(&edf_workq,
				    &task->z_delayed_work,
				    start_time);

	task->state = SOF_TASK_STATE_QUEUED;
	return 0;
}

static int schedule_edf_task_cancel(void *data, struct task *task)
{
	if (task->state == SOF_TASK_STATE_QUEUED) {
		k_work_cancel_delayable(&task->z_delayed_work);

		/* delete task */
		task->state = SOF_TASK_STATE_CANCEL;
	}

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
	.schedule_task_cancel	= schedule_edf_task_cancel,
	.schedule_task_free	= schedule_edf_task_free,
};

int scheduler_init_edf(void)
{
	struct k_thread *thread = &edf_workq.thread;

	scheduler_init(SOF_SCHEDULE_EDF, &schedule_edf_ops, NULL);

	k_work_queue_start(&edf_workq,
		       edf_workq_stack,
		       K_THREAD_STACK_SIZEOF(edf_workq_stack),
		       EDF_ZEPHYR_PRIORITY, NULL);

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
	int ret;

	ret = schedule_task_init(task, uid, SOF_SCHEDULE_EDF, 0, ops->run, data,
				 core, flags);
	if (ret < 0)
		return ret;

	task->ops = *ops;

	k_work_init_delayable(&task->z_delayed_work, edf_work_handler);

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
