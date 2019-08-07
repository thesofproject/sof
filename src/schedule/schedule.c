// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

/* Generic scheduler */

#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/cpu.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdint.h>

static const struct scheduler_ops *schedulers[SOF_SCHEDULE_COUNT] = {
	&schedule_edf_ops,		/* SOF_SCHEDULE_EDF */
	&schedule_ll_ops		/* SOF_SCHEDULE_LL */
};

int schedule_task_init(struct task *task, uint16_t type, uint16_t priority,
		       enum task_state (*func)(void *data), void *data,
		       uint16_t core, uint32_t flags)
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
	task->flags = flags;
	task->state = SOF_TASK_STATE_INIT;
	task->func = func;
	task->data = data;
	task->ops = schedulers[task->type];

	if (task->ops->schedule_task_init)
		ret = task->ops->schedule_task_init(task);

out:
	return ret;
}

int scheduler_init(struct sof *sof)
{
	struct schedule_data **sch = arch_schedule_get_data();
	int i = 0;
	int ret = 0;

	/* init scheduler_data */
	*sch = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(**sch));

	for (i = 0; i < SOF_SCHEDULE_COUNT; i++) {
		if (schedulers[i]->scheduler_init) {
			ret = schedulers[i]->scheduler_init(sof);
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
