/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_SCHEDULE_TASK_H__
#define __SOF_SCHEDULE_TASK_H__

#include <arch/schedule/task.h>
#include <sof/list.h>
#include <stdint.h>

struct scheduler_ops;
struct sof;

#define SOF_TASK_PRI_HIGH	0	/* priority level 0 - high */
#define SOF_TASK_PRI_MED	4	/* priority level 4 - medium */
#define SOF_TASK_PRI_LOW	9	/* priority level 9 - low */

#define SOF_TASK_PRI_COUNT	10	/* range of priorities (0-9) */

#define SOF_TASK_PRI_IPC	SOF_TASK_PRI_LOW
#define SOF_TASK_PRI_IDC	SOF_TASK_PRI_LOW

/* task states */
enum task_state {
	SOF_TASK_STATE_INIT = 0,
	SOF_TASK_STATE_QUEUED,
	SOF_TASK_STATE_PENDING,
	SOF_TASK_STATE_RUNNING,
	SOF_TASK_STATE_PREEMPTED,
	SOF_TASK_STATE_COMPLETED,
	SOF_TASK_STATE_FREE,
	SOF_TASK_STATE_CANCEL,
};

struct task {
	uint16_t type;
	uint64_t start;
	uint16_t priority;
	enum task_state state;
	uint16_t core;
	uint32_t flags;
	void *data;
	uint64_t (*func)(void *data);
	struct list_item list;
	struct list_item irq_list;	/* list for assigned irq level */
	const struct scheduler_ops *ops;
	void *private;
};

int do_task_master_core(struct sof *sof);

static inline int allocate_tasks(void)
{
	return arch_allocate_tasks();
}

static inline int run_task(struct task *task)
{
	return arch_run_task(task);
}

#endif /* __SOF_SCHEDULE_TASK_H__ */
