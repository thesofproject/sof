/*
 * Copyright (c) 2019, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 */

#include <sof/audio/component.h>
#include <sof/task.h>
#include <stdint.h>
#include <sof/edf_schedule.h>
#include <sof/wait.h>

 /* scheduler testbench definition */

struct edf_schedule_data {
	spinlock_t lock; /* schedule lock */
	struct list_item list; /* list of tasks in priority queue */
	uint32_t clock;
};

static struct edf_schedule_data *sch;

static void schedule_edf_task_complete(struct task *task);
static void schedule_edf_task(struct task *task, uint64_t start,
			      uint64_t deadline, uint32_t flags);
static int schedule_edf_task_init(struct task *task, uint32_t xflags);
static int edf_scheduler_init(void);
static void edf_scheduler_free(void);
static void schedule_edf(void);
static int schedule_edf_task_cancel(struct task *task);
static void schedule_edf_task_free(struct task *task);

static void schedule_edf_task_complete(struct task *task)
{
	list_item_del(&task->list);
	task->state = SOF_TASK_STATE_COMPLETED;
}

/* schedule task */
static void schedule_edf_task(struct task *task, uint64_t start,
			      uint64_t deadline, uint32_t flags)
{
	(void)deadline;
	list_item_prepend(&task->list, &sch->list);
	task->state = SOF_TASK_STATE_QUEUED;

	if (task->func)
		task->func(task->data);

	schedule_edf_task_complete(task);
}

static int schedule_edf_task_init(struct task *task, uint32_t xflags)
{
	struct edf_task_pdata *edf_pdata;
	(void)xflags;

	edf_pdata = malloc(sizeof(*edf_pdata));
	edf_sch_set_pdata(task, edf_pdata);

	return 0;
}

/* initialize scheduler */
static int edf_scheduler_init(void)
{
	trace_edf_sch("edf_scheduler_init()");
	sch = malloc(sizeof(*sch));
	list_init(&sch->list);
	spinlock_init(&sch->lock);

	return 0;
}

static void edf_scheduler_free(void)
{
	free(sch);
}

/* The following definitions are to satisfy libsof linker errors */
static void schedule_edf(void)
{
}

static int schedule_edf_task_cancel(struct task *task)
{
	if (task->state == SOF_TASK_STATE_QUEUED) {
		/* delete task */
		task->state = SOF_TASK_STATE_CANCEL;
		list_item_del(&task->list);
	}

	return 0;
}

static void schedule_edf_task_free(struct task *task)
{
	task->state = SOF_TASK_STATE_FREE;
	task->func = NULL;
	task->data = NULL;

	free(edf_sch_get_pdata(task));
	edf_sch_set_pdata(task, NULL);
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
	.scheduler_free		= edf_scheduler_free,
	.scheduler_run		= schedule_edf
};
