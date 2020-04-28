// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/schedule/task.h>
#include <stdint.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/lib/wait.h>

#include <kernel.h>
#include <sys_clock.h>

struct k_work_q ll_workq;
K_THREAD_STACK_DEFINE(ll_workq_stack, 8192);

static void ll_work_handler(struct k_work *work)
{
	struct task *task = CONTAINER_OF(work, struct task, z_delayed_work);

	task->state = SOF_TASK_STATE_RUNNING;

	if (task->ops.run)
		task->ops.run(task->data);

	task->state = SOF_TASK_STATE_COMPLETED;
}

/* schedule task */
static int schedule_ll_task(struct task *task, uint64_t start,
			      uint64_t period)
{
	k_timeout_t start_time = K_USEC(start);

	k_delayed_work_submit_to_queue(&ll_workq,
				       &task->z_delayed_work,
					   start_time);

	task->state = SOF_TASK_STATE_QUEUED;
	return 0;
}

static int schedule_ll_task_cancel(struct task *task)
{
	int ret = 0;

	if (task->state == SOF_TASK_STATE_QUEUED) {
		ret = k_delayed_work_cancel(&task->z_delayed_work);

		/* delete task */
		task->state = SOF_TASK_STATE_CANCEL;
	}

	return ret;
}

static int schedule_ll_task_free(struct task *task)
{
	task->state = SOF_TASK_STATE_FREE;
	task->ops.run = NULL;
	task->data = NULL;

	return 0;
}

struct scheduler_ops schedule_ll_ops = {
	.schedule_task		= schedule_ll_task,
	.schedule_task_running	= NULL,
	.schedule_task_complete = NULL,
	.reschedule_task	= NULL,
	.schedule_task_cancel	= schedule_ll_task_cancel,
	.schedule_task_free	= schedule_ll_task_free,
	.scheduler_free		= NULL,
	.scheduler_run		= NULL,
};

int scheduler_init_ll(struct ll_schedule_domain *domain)
{
	trace_ll("ll_scheduler_init()");

	k_work_q_start(&ll_workq,
		       ll_workq_stack,
		       K_THREAD_STACK_SIZEOF(ll_workq_stack),
		       -1);
	k_thread_name_set(&ll_workq.thread, "ll_workq");

	/* TODO: we are scheduling work on timers atm, TOD DMA */
	switch (domain->type) {
	case SOF_SCHEDULE_LL_TIMER:
		scheduler_init(SOF_SCHEDULE_LL_TIMER, &schedule_ll_ops, NULL);
		break;
	case SOF_SCHEDULE_LL_DMA:
		scheduler_init(SOF_SCHEDULE_LL_DMA, &schedule_ll_ops, NULL);
		break;
	default:
		// panic;
		break;
	}

	return 0;
}

int schedule_task_init_ll(struct task *task,
			  uint32_t uid, uint16_t type, uint16_t priority,
			  enum task_state (*run)(void *data), void *data,
			  uint16_t core, uint32_t flags)
{
	int ret = 0;

	ret = schedule_task_init(task, uid, SOF_SCHEDULE_EDF, 0, run, data,
					 core, flags);
	if (ret < 0)
		return ret;
	task->ops.run = run;

	k_delayed_work_init(&task->z_delayed_work, ll_work_handler);

	return 0;
}

struct ll_schedule_domain *timer_domain_init(struct timer *timer, int clk,
					     uint64_t timeout)
{
	struct ll_schedule_domain *d;

	d = k_malloc(sizeof(*d));
	d->type = SOF_SCHEDULE_LL_TIMER;

	return d;
}

struct ll_schedule_domain *dma_single_chan_domain_init(struct dma *dma_array,
						       uint32_t num_dma,
						       int clk)
{
	struct ll_schedule_domain *d;

	d = k_malloc(sizeof(*d));
	d->type = SOF_SCHEDULE_LL_DMA;

	return d;
}
