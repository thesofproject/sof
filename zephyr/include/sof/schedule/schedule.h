/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 */

/* Generic schedule api header */
#ifndef __SOF_SCHEDULE_SCHEDULE_H__
#define __SOF_SCHEDULE_SCHEDULE_H__

#include <sof/common.h>
#include <sof/list.h>
#include <sof/schedule/task.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/** \addtogroup schedule_api Schedule API
 *  @{
 */

/** \name Trace macros
 *  @{
 */

/** \brief Trace info message from generic schedule functions */
#define trace_schedule(format, ...) \
	trace_event(TRACE_CLASS_EDF, format, ##__VA_ARGS__)

/** \brief Trace error message from generic schedule functions */
#define trace_schedule_error(format, ...) \
	trace_error(TRACE_CLASS_EDF, format, ##__VA_ARGS__)

/** \brief Trace verbose message from generic schedule functions */
#define tracev_schedule(format, ...) \
	tracev_event(TRACE_CLASS_EDF, format, ##__VA_ARGS__)

/** @}*/

/** \brief Type of scheduler, comes from topology. */
enum {
	SOF_SCHEDULE_EDF = 0,	/**< EDF, schedules based on task's deadline */
	SOF_SCHEDULE_LL_TIMER,	/**< Low latency timer, schedules immediately
				  *  on selected timer's tick
				  */
	SOF_SCHEDULE_LL_DMA,	/**< Low latency DMA, schedules immediately
				  *  on scheduling component's DMA interrupt
				  */
	SOF_SCHEDULE_COUNT	/**< indicates number of scheduler types */
};

/**
 * Scheduler operations.
 *
 * Almost all schedule operations must return 0 for success and negative values
 * for errors. Only scheduler_free and scheduler_run operations are allowed
 * to not return any status.
 */
struct scheduler_ops {
	/**
	 * Schedules task with given scheduling parameters.
	 * @param data Private data of selected scheduler.
	 * @param task Task to be scheduled.
	 * @param start Start time of given task (in microseconds).
	 * @param period Scheduling period of given task (in microseconds).
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * This operation is mandatory.
	 */
	int (*schedule_task)(struct task *task, uint64_t start,
			     uint64_t period);

	/**
	 * Sets task into running state along with executing additional
	 * scheduler specific operations.
	 * @param data Private data of selected scheduler.
	 * @param task Task to be set into running state.
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * This operation is optional.
	 */
	int (*schedule_task_running)(struct task *task);

	/**
	 * Sets task into completed state along with executing additional
	 * scheduler specific operations.
	 * @param data Private data of selected scheduler.
	 * @param task Task to be set into completed state.
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * This operation is optional.
	 */
	int (*schedule_task_complete)(struct task *task);

	/**
	 * Reschedules already scheduled task with new start time.
	 * @param data Private data of selected scheduler.
	 * @param task Task to be rescheduled.
	 * @param start New start time of given task (in microseconds).
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * This operation is optional.
	 */
	int (*reschedule_task)(struct task *task, uint64_t start);

	/**
	 * Cancels previously scheduled task.
	 * @param data Private data of selected scheduler.
	 * @param task Task to be canceled.
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * This operation is mandatory.
	 */
	int (*schedule_task_cancel)(struct task *task);

	/**
	 * Frees task's resources.
	 * @param data Private data of selected scheduler.
	 * @param task Task to be freed.
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * This operation is mandatory.
	 */
	int (*schedule_task_free)(struct task *task);

	/**
	 * Frees scheduler's resources.
	 * @param data Private data of selected scheduler.
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * This operation is optional.
	 */
	void (*scheduler_free)(void);

	/**
	 * Starts scheduler (not all schedulers require to be manually started).
	 * @param data Private data of selected scheduler.
	 *
	 * This operation is optional.
	 */
	void (*scheduler_run)(void);
};

/** See scheduler_ops::schedule_task_running */
static inline int schedule_task_running(struct task *task)
{
	if (task->sops->schedule_task_running)
		task->sops->schedule_task_running(task);
	return 0;
}

/** See scheduler_ops::schedule_task_complete */
static inline int schedule_task_complete(struct task *task)
{
	if (task->sops->schedule_task_complete)
		task->sops->schedule_task_complete(task);
	return 0;
}

/** See scheduler_ops::schedule_task */
static inline int schedule_task(struct task *task, uint64_t start,
				uint64_t period)
{
	if (task->sops->schedule_task)
		task->sops->schedule_task(task, start, period);
	return 0;
}

/** See scheduler_ops::reschedule_task */
static inline int reschedule_task(struct task *task, uint64_t start)
{
	if (task->sops->reschedule_task)
		task->sops->reschedule_task(task, start);
	return 0;
}

/** See scheduler_ops::schedule_task_cancel */
static inline int schedule_task_cancel(struct task *task)
{
	if (task->sops->schedule_task_cancel)
		task->sops->schedule_task_cancel(task);
	return 0;
}

/** See scheduler_ops::schedule_task_free */
static inline int schedule_task_free(struct task *task)
{
	if (task->sops->schedule_task_free)
		task->sops->schedule_task_free(task);
	return 0;
}

/** See scheduler_ops::scheduler_free */
static inline void schedule_free(void)
{
}

/** See scheduler_ops::scheduler_run */
static inline void schedule(void)
{

		//	sch->ops->scheduler_run(sch->data);
}

/**
 * Initializes scheduling task.
 * @param task Task to be initialized.
 * @param uid Statically assigned task uuid.
 * @param type SOF_SCHEDULE_ type.
 * @param priority Task's priority.
 * @param run Pointer to task's execution function.
 * @param data Pointer to task's execution data.
 * @param core Core on which task should be run.
 * @param flags Special scheduling flags.
 * @return 0 if succeeded, error code otherwise.
 */
int schedule_task_init(struct task *task,
		       uint32_t uid, uint16_t type, uint16_t priority,
		       enum task_state (*run)(void *data), void *data,
		       uint16_t core, uint32_t flags);

/**
 * Initializes scheduler
 * @param type SOF_SCHEDULE_ type.
 * @param ops Pointer to scheduler's operations.
 * @param data Scheduler's private data.
 */
void scheduler_init(int type, struct scheduler_ops *ops, void *data);

void *scheduler_get_data(uint16_t type);

/** @}*/

#endif /* __SOF_SCHEDULE_SCHEDULE_H__ */
