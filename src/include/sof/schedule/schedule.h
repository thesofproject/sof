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
 * for errors. Only the scheduler_free operation is allowed to not return any
 * status.
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
	int (*schedule_task)(void *data, struct task *task, uint64_t start,
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
	int (*schedule_task_running)(void *data, struct task *task);

	/**
	 * Sets task into completed state along with executing additional
	 * scheduler specific operations.
	 * @param data Private data of selected scheduler.
	 * @param task Task to be set into completed state.
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * This operation is optional.
	 */
	int (*schedule_task_complete)(void *data, struct task *task);

	/**
	 * Reschedules already scheduled task with new start time.
	 * @param data Private data of selected scheduler.
	 * @param task Task to be rescheduled.
	 * @param start New start time of given task (in microseconds).
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * This operation is optional.
	 */
	int (*reschedule_task)(void *data, struct task *task, uint64_t start);

	/**
	 * Cancels previously scheduled task.
	 * @param data Private data of selected scheduler.
	 * @param task Task to be canceled.
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * This operation is mandatory.
	 */
	int (*schedule_task_cancel)(void *data, struct task *task);

	/**
	 * Frees task's resources.
	 * @param data Private data of selected scheduler.
	 * @param task Task to be freed.
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * This operation is mandatory.
	 */
	int (*schedule_task_free)(void *data, struct task *task);

	/**
	 * Frees scheduler's resources.
	 * @param data Private data of selected scheduler.
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * This operation is optional.
	 */
	void (*scheduler_free)(void *data);
};

/** \brief Holds information about scheduler. */
struct schedule_data {
	struct list_item list;			/**< list of schedulers */
	int type;				/**< SOF_SCHEDULE_ type */
	const struct scheduler_ops *ops;	/**< scheduler operations */
	void *data;				/**< pointer to private data */
};

/** \brief Holds list of all registered schedulers. */
struct schedulers {
	struct list_item list;	/**< list of schedulers */
};

/**
 * Retrieves registered schedulers.
 * @return List of registered schedulers.
 */
struct schedulers **arch_schedulers_get(void);

/**
 * Retrieves scheduler's data.
 * @param type SOF_SCHEDULE_ type.
 * @return Pointer to scheduler's data.
 */
static inline void *scheduler_get_data(uint16_t type)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (type == sch->type)
			return sch->data;
	}

	return NULL;
}

/** See scheduler_ops::schedule_task_running */
static inline int schedule_task_running(struct task *task)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (task->type == sch->type) {
			/* optional operation */
			if (!sch->ops->schedule_task_running)
				return 0;

			return sch->ops->schedule_task_running(sch->data, task);
		}
	}

	return -ENODEV;
}

/** See scheduler_ops::schedule_task_complete */
static inline int schedule_task_complete(struct task *task)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (task->type == sch->type) {
			/* optional operation */
			if (!sch->ops->schedule_task_complete)
				return 0;

			return sch->ops->schedule_task_complete(sch->data,
								task);
		}
	}

	return -ENODEV;
}

/** See scheduler_ops::schedule_task */
static inline int schedule_task(struct task *task, uint64_t start,
				uint64_t period)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (task->type == sch->type && sch->ops->schedule_task)
			return sch->ops->schedule_task(sch->data, task, start,
						       period);
	}

	return -ENODEV;
}

/** See scheduler_ops::reschedule_task */
static inline int reschedule_task(struct task *task, uint64_t start)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (task->type == sch->type) {
			/* optional operation */
			if (!sch->ops->reschedule_task)
				return 0;

			return sch->ops->reschedule_task(sch->data, task,
							 start);
		}
	}

	return -ENODEV;
}

/** See scheduler_ops::schedule_task_cancel */
static inline int schedule_task_cancel(struct task *task)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (task->type == sch->type && sch->ops->schedule_task_cancel)
			return sch->ops->schedule_task_cancel(sch->data, task);
	}

	return -ENODEV;
}

/** See scheduler_ops::schedule_task_free */
static inline int schedule_task_free(struct task *task)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (task->type == sch->type && sch->ops->schedule_task_free)
			return sch->ops->schedule_task_free(sch->data, task);
	}

	return -ENODEV;
}

/** See scheduler_ops::scheduler_free */
static inline void schedule_free(void)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (sch->ops->scheduler_free)
			sch->ops->scheduler_free(sch->data);
	}
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
		       const struct sof_uuid_entry *uid, uint16_t type,
		       uint16_t priority, enum task_state (*run)(void *data),
		       void *data, uint16_t core, uint32_t flags);

/**
 * Initializes scheduler
 * @param type SOF_SCHEDULE_ type.
 * @param ops Pointer to scheduler's operations.
 * @param data Scheduler's private data.
 */
void scheduler_init(int type, const struct scheduler_ops *ops, void *data);

/** @}*/

#endif /* __SOF_SCHEDULE_SCHEDULE_H__ */
