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

struct sof;

#define SOF_TASK_PRI_HIGH	0	/* priority level 0 - high */
#define SOF_TASK_PRI_MED	4	/* priority level 4 - medium */
#define SOF_TASK_PRI_LOW	9	/* priority level 9 - low */
#define SOF_TASK_PRI_IDLE	INT16_MAX	/* lowest possible priority */
#define SOF_TASK_PRI_ALMOST_IDLE	(SOF_TASK_PRI_IDLE - 1)

#define SOF_TASK_PRI_IPC	SOF_TASK_PRI_HIGH
#define SOF_TASK_PRI_IDC	SOF_TASK_PRI_HIGH

/* task default stack size in bytes */
#define SOF_TASK_DEFAULT_STACK_SIZE	2048

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
	SOF_TASK_STATE_RESCHEDULE,
};

struct task {
	uint64_t start;
	uint16_t type;
	uint16_t priority;
	uint16_t core;
	uint16_t flags;
	enum task_state state;
	void *data;
	enum task_state (*run)(void *data);
	void (*complete)(void *data);
	struct list_item list;
	void *private;
};

enum task_state task_main_master_core(void *data);

enum task_state task_main_slave_core(void *data);

void task_main_init(void);

void task_main_free(void);

int task_main_start(struct sof *sof);

#endif /* __SOF_SCHEDULE_TASK_H__ */
