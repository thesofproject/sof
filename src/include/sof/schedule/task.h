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
#include <zephyr/kernel.h>
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
	struct k_work_delayable z_delayed_work;
#endif
#if defined(CONFIG_SCHEDULE_LOG_CYCLE_STATISTICS) || defined(__ZEPHYR__)
	uint32_t cycles_sum;
	uint32_t cycles_max;
	uint32_t cycles_cnt;
#endif
};

static inline bool task_is_active(struct task *task)
{
	switch (task->state) {
	case SOF_TASK_STATE_QUEUED:
	case SOF_TASK_STATE_PENDING:
	case SOF_TASK_STATE_RUNNING:
	case SOF_TASK_STATE_PREEMPTED:
	case SOF_TASK_STATE_RESCHEDULE:
		return true;
	default:
		return false;
	}
}

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
