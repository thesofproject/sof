/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/**
 * @file rtos/task.h
 * @brief Core task descriptor, task operations, and task execution state machine.
 * @ingroup scheduling_api rtos_api
 */

#ifndef __ZEPHYR_RTOS_TASK_H__
#define __ZEPHYR_RTOS_TASK_H__

#include <rtos/panic.h>
#include <sof/list.h>
#include <stdbool.h>
#include <stdint.h>
#include <rtos/kernel.h>
#include <sof/lib/perf_cnt.h>

/** \addtogroup scheduling_api
 *  @{
 */

struct comp_dev;
struct sof;
struct schedule_data;

/** @brief Predefined Low-Latency task priority levels. */
#define SOF_TASK_PRI_HIGH	0	/**< Priority level 0 - high (hard real-time). */
#define SOF_TASK_PRI_MED	4	/**< Priority level 4 - medium. */
#define SOF_TASK_PRI_LOW	9	/**< Priority level 9 - low (background/idle). */

/** @brief Predefined EDF task deadlines. */
#define SOF_TASK_DEADLINE_IDLE		UINT64_MAX	/**< Idle deadline; run only when nothing else is ready. */
#define SOF_TASK_DEADLINE_ALMOST_IDLE	(SOF_TASK_DEADLINE_IDLE - 1) /**< Almost idle deadline. */
#define SOF_TASK_DEADLINE_NOW		0		/**< Immediate deadline; run as soon as possible. */

/** @brief Task counter initial value. */
#define SOF_TASK_SKIP_COUNT		0xFFFFu

/**
 * @brief Lifecycle and execution states of an SOF task.
 */
enum task_state {
	/** @brief Task allocated and initialized, not yet scheduled. */
	SOF_TASK_STATE_INIT = 0,
	/** @brief Task queued in scheduler run list awaiting tick or trigger. */
	SOF_TASK_STATE_QUEUED,
	/** @brief Task waiting for preconditions, audio data, or buffer space. */
	SOF_TASK_STATE_PENDING,
	/** @brief Task actively executing in thread or timer/DMA handler. */
	SOF_TASK_STATE_RUNNING,
	/** @brief Task preempted by higher priority thread or ISR. */
	SOF_TASK_STATE_PREEMPTED,
	/** @brief Task execution finished; removed from active scheduling. */
	SOF_TASK_STATE_COMPLETED,
	/** @brief Task resources released. */
	SOF_TASK_STATE_FREE,
	/** @brief Task cancelled, pending removal from scheduler. */
	SOF_TASK_STATE_CANCEL,
	/** @brief Return code from run callback requesting rescheduling in next period. */
	SOF_TASK_STATE_RESCHEDULE,
};

/**
 * @brief Operations and callbacks associated with a scheduling task.
 */
struct task_ops {
	enum task_state (*run)(void *data);	/**< Task's main execution callback function. */
	void (*complete)(void *data);		/**< Optional callback executed on task completion. */
	uint64_t (*get_deadline)(void *data);	/**< Optional callback returning current task deadline timestamp. */
};

/**
 * @brief Core task structure managed by SOF schedulers.
 */
struct task {
	uint64_t start;		/**< Scheduler/domain-specific start state (e.g. absolute next-run tick timestamp in LL). */
	const struct sof_uuid_entry *uid; /**< UUID identifying the task or associated component. */
	uint16_t type;		/**< Scheduler type identifier (SOF_SCHEDULE_*). */
	int16_t priority;	/**< Priority of the task (lower values run first). */
	uint16_t core;		/**< Execution core ID affinity. */
	uint16_t flags;		/**< Custom task flags. */
	struct schedule_data *sch;	/**< Pointer to scheduler instance bound to this task. */
	enum task_state state;	/**< Current task lifecycle state. */
	void *data;		/**< Custom context pointer passed to task operations. */
	struct list_item list;	/**< List node linking task into scheduler run queue. */
	void *priv_data;	/**< Scheduler-specific private task context. */
	struct task_ops ops;	/**< Task operations callbacks table. */
	struct k_work_delayable z_delayed_work; /**< Zephyr delayable work item (used by legacy EDF). */
	uint32_t cycles_sum;	/**< Cumulative execution cycles. */
	uint32_t cycles_max;	/**< Maximum cycle count recorded in a single run. */
	uint32_t cycles_cnt;	/**< Number of completed execution runs. */
#if CONFIG_PERFORMANCE_COUNTERS
	struct perf_cnt_data pcd; /**< Performance counter telemetry data. */
#endif
};

/**
 * @brief Check whether a task is active (queued, running, pending, or rescheduling).
 * @param[in] task Task to check.
 * @return true if task is in an active execution state, false otherwise.
 */
static inline bool task_is_active(struct task *task)
{
	if (!task)
		return false;

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

/**
 * @brief Execute a task's run callback.
 * @param[in,out] task Task to execute.
 * @return Resulting task state returned by the run callback.
 */
static inline enum task_state task_run(struct task *task)
{
	assert(task->ops.run);

	return task->ops.run(task->data);
}

/**
 * @brief Execute a task's complete callback, if registered.
 * @param[in,out] task Task whose completion callback should be invoked.
 */
static inline void task_complete(struct task *task)
{
	if (task->ops.complete)
		task->ops.complete(task->data);
}

/**
 * @brief Retrieve a task's deadline timestamp.
 * @param[in] task Task to query.
 * @return Current deadline timestamp.
 */
static inline uint64_t task_get_deadline(struct task *task)
{
	assert(task->ops.get_deadline);

	return task->ops.get_deadline(task->data);
}

/**
 * @brief Start the main SOF background task.
 * @param[in,out] sof Global SOF context pointer.
 * @return 0 on success, negative error code on failure.
 */
int task_main_start(struct sof *sof);

/**
 * @brief Signal completion of system startup.
 * @return 0 on success, negative error code on failure.
 */
int start_complete(void);

/** @}*/

#endif /* __ZEPHYR_RTOS_TASK_H__ */
