/*
 * Copyright (c) 2018, Intel Corporation
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
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *	   Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 */

#include <sof/audio/component.h>
#include <sof/task.h>
#include <stdint.h>
#include <sof/wait.h>

/* scheduler testbench definition */

struct schedule_data {
	spinlock_t lock;
	struct list_item list; /* list of tasks in priority queue */
	uint32_t clock;
};

static struct schedule_data *sch;

void schedule_task_complete(struct task *task)
{
	list_item_del(&task->list);
	task->state = TASK_STATE_COMPLETED;
}

/* schedule task */
void schedule_task(struct task *task, uint64_t start, uint64_t deadline)
{
	task->deadline = deadline;
	list_item_prepend(&task->list, &sch->list);
	task->state = TASK_STATE_QUEUED;

	if (task->func)
		task->func(task->data);

	schedule_task_complete(task);
}

/* initialize scheduler */
int scheduler_init(struct sof *sof)
{
	trace_pipe("ScI");

	sch = malloc(sizeof(*sch));
	list_init(&sch->list);
	spinlock_init(&sch->lock);

	return 0;
}

void schedule_task_init(struct task *task, void(*func)(void *),
	void *data)
{
	task->core = 0;
	task->state = TASK_STATE_INIT;
	task->func = func;
	task->data = data;
}

void schedule_task_free(struct task *task)
{
	task->state = TASK_STATE_FREE;
	task->func = NULL;
	task->data = NULL;
}

void schedule_task_config(struct task *task, uint16_t priority,
	uint16_t core)
{
	task->priority = priority;
	task->core = core;
}

/* The following definitions are to satisfy libsof linker errors */

void schedule(void)
{
}

void schedule_task_idle(struct task *task, uint64_t deadline)
{
}

/* testbench work definition */

void work_schedule_default(struct work *w, uint64_t timeout)
{
}

void work_cancel_default(struct work *work)
{
}
