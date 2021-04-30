/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_SCHEDULE_TASK_H__
#define __SOF_SCHEDULE_TASK_H__

#include <arch/schedule/task.h>
#include <sof/debug/panic.h>
#include <sof/list.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __ZEPHYR__
#include <kernel.h>
#endif

struct comp_dev;
struct sof;

/** \brief Predefined LL task priorities. */
#define SOF_TASK_PRI_HIGH	0	/* priority level 0 - high */
#define SOF_TASK_PRI_MED	4	/* priority level 4 - medium */
#define SOF_TASK_PRI_LOW	9	/* priority level 9 - low */

/** \brief Predefined EDF task deadlines. */
#define SOF_TASK_DEADLINE_IDLE		UINT64_MAX
#define SOF_TASK_DEADLINE_ALMOST_IDLE	(SOF_TASK_DEADLINE_IDLE - 1)
#define SOF_TASK_DEADLINE_NOW		0

/** \brief Task counter initial value. */
#define SOF_TASK_SKIP_COUNT		0xFFFFu

/** \brief Task states. */
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

/** \brief Task operations. */
struct task_ops {
	enum task_state (*run)(void *data);	/**< task's main operation */
	void (*complete)(void *data);		/**< executed on completion */
	uint64_t (*get_deadline)(void *data);	/**< returns current deadline */
};

/** \brief Task used by schedulers. */
struct task {
	uint64_t start;		/**< start time in [ms] since now (LL only) */
	const struct sof_uuid_entry *uid; /**< Uuid */
	uint16_t type;		/**< type of the task (LL or EDF) */
	uint16_t priority;	/**< priority of the task (used by LL) */
	uint16_t core;		/**< execution core */
	uint16_t flags;		/**< custom flags */
	enum task_state state;	/**< current state */
	void *data;		/**< custom data passed to all ops */
	struct list_item list;	/**< used by schedulers to hold tasks */
	void *priv_data;	/**< task private data */
	struct task_ops ops;	/**< task operations */
#ifdef __ZEPHYR__
	struct k_delayed_work z_delayed_work;
#endif
};

/** \brief Task type registered by pipelines. */
struct pipeline_task {
	struct task task;		/**< parent structure */
	bool registrable;		/**< should task be registered on irq */
	struct comp_dev *sched_comp;	/**< pipeline scheduling component */
};

#define pipeline_task_get(t) container_of(t, struct pipeline_task, task)

static inline enum task_state task_run(struct task *task)
{
	assert(task->ops.run);

	return task->ops.run(task->data);
}

static inline void task_complete(struct task *task)
{
	if (task->ops.complete)
		task->ops.complete(task->data);
}

static inline uint64_t task_get_deadline(struct task *task)
{
	assert(task->ops.get_deadline);

	return task->ops.get_deadline(task->data);
}

enum task_state task_main_primary_core(void *data);

enum task_state task_main_secondary_core(void *data);

void task_main_init(void);

void task_main_free(void);

int task_main_start(struct sof *sof);

#endif /* __SOF_SCHEDULE_TASK_H__ */
