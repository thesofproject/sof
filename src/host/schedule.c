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
#include "host/edf_schedule.h"
#include "host/ll_schedule.h"

static const struct scheduler_ops *schedulers[SOF_SCHEDULE_COUNT] = {
	&schedule_edf_ops,              /* SOF_SCHEDULE_EDF */
	&schedule_ll_ops		/* SOF_LL_TASK */
};

/* testbench work definition */
int schedule_task_init(struct task *task, uint16_t type, uint16_t priority,
		       uint64_t (*func)(void *data), void *data, uint16_t core,
		       uint32_t xflags)
{
	task->type = type;
	task->priority = priority;
	task->core = core;
	task->state = SOF_TASK_STATE_INIT;
	task->func = func;
	task->data = data;

	return schedulers[task->type]->schedule_task_init(task, xflags);
}

void schedule_task(struct task *task, uint64_t start, uint64_t deadline,
		   uint32_t flags)
{
	if (schedulers[task->type]->schedule_task)
		schedulers[task->type]->schedule_task(task, start, deadline,
			flags);
}

void schedule_task_free(struct task *task)
{
	if (schedulers[task->type]->schedule_task_free)
		schedulers[task->type]->schedule_task_free(task);
}

void reschedule_task(struct task *task, uint64_t start)
{
	if (schedulers[task->type]->reschedule_task)
		schedulers[task->type]->reschedule_task(task, start);
}

int schedule_task_cancel(struct task *task)
{
	int ret = 0;

	if (schedulers[task->type]->schedule_task_cancel)
		ret = schedulers[task->type]->schedule_task_cancel(task);

	return ret;
}

int scheduler_init(void)
{
	int i = 0;
	int ret = 0;

	for (i = 0; i < SOF_SCHEDULE_COUNT; i++) {
		if (schedulers[i]->scheduler_init) {
			ret = schedulers[i]->scheduler_init();
			if (ret < 0)
				goto out;
		}
	}
out:
	return ret;
}

void schedule_free(void)
{
	int i;

	for (i = 0; i < SOF_SCHEDULE_COUNT; i++) {
		if (schedulers[i]->scheduler_free)
			schedulers[i]->scheduler_free();
	}
}

void schedule(void)
{
	int i = 0;

	for (i = 0; i < SOF_SCHEDULE_COUNT; i++) {
		if (schedulers[i]->scheduler_run)
			schedulers[i]->scheduler_run();
	}
}
