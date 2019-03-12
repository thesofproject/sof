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

/* Generic scheduler */
#include <sof/schedule.h>
#include <sof/edf_schedule.h>
#include <sof/ll_schedule.h>

static const struct scheduler_ops *schedulers[SOF_SCHEDULE_COUNT] = {
	&schedule_edf_ops,              /* SOF_SCHEDULE_EDF */
	&schedule_ll_ops		/* SOF_SCHEDULE_LL */
};

int schedule_task_init(struct task *task, uint16_t type, uint16_t priority,
		       uint64_t (*func)(void *data), void *data, uint16_t core,
		       uint32_t xflags)
{
	int ret = 0;

	if (type >= SOF_SCHEDULE_COUNT) {
		trace_schedule_error("schedule_task_init() error: "
				     "invalid task type");
		ret = -EINVAL;
		goto out;
	}

	task->type = type;
	task->priority = priority;
	task->core = core;
	task->state = SOF_TASK_STATE_INIT;
	task->func = func;
	task->data = data;
	task->ops = schedulers[task->type];

	if (task->ops->schedule_task_init)
		ret = task->ops->schedule_task_init(task, xflags);
out:
	return ret;
}

void schedule_task_free(struct task *task)
{
	if (task->ops->schedule_task_free)
		task->ops->schedule_task_free(task);
}

void schedule_task(struct task *task, uint64_t start, uint64_t deadline,
		   uint32_t flags)
{
	if (task->ops->schedule_task)
		task->ops->schedule_task(task, start, deadline, flags);
}

void reschedule_task(struct task *task, uint64_t start)
{
	if (task->ops->reschedule_task)
		task->ops->reschedule_task(task, start);
}

int schedule_task_cancel(struct task *task)
{
	if (task->ops->schedule_task_cancel)
		return task->ops->schedule_task_cancel(task);
	return 0;
}

void schedule_task_running(struct task *task)
{
	if (task->ops->schedule_task_running)
		task->ops->schedule_task_running(task);
}

void schedule_task_complete(struct task *task)
{
	if (task->ops->schedule_task_complete)
		task->ops->schedule_task_complete(task);
}

int scheduler_init(void)
{
	struct schedule_data **sch = arch_schedule_get_data();
	int i = 0;
	int ret = 0;

	/* init scheduler_data */
	*sch = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(**sch));

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
