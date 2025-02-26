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
#include <rtos/task.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <ipc4/base_fw.h>

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
	SOF_SCHEDULE_DP,	/**< DataProcessing scheduler
				  *  Scheduler based on Zephyr peemptive threads
				  *  TODO: DP will become the Zephyr EDF scheduler type
				  *  and will be unified with SOF_SCHEDULE_EDF for Zephyr builds
				  *  current implementation of Zephyr based EDF is depreciated now
				  */
	SOF_SCHEDULE_TWB,	/**< Tasks With Budget scheduler based on Zephyr peemptive threads
				  * for each SOF task that has pre-allocated MCPS budget
				  * renewed with every system tick.
				  */
	SOF_SCHEDULE_COUNT	/**< indicates number of scheduler types */
};

/** \brief Scheduler free available flags */
#define SOF_SCHEDULER_FREE_IRQ_ONLY	BIT(0) /**< Free function disables only
						 *  interrupts
						 */
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
	 * Schedules task with given scheduling parameters.
	 * @param data Private data of selected scheduler.
	 * @param task Task to be scheduled.
	 * @param start Start time of given task (in microseconds).
	 * @param period Scheduling period of given task (in microseconds).
	 * @param before Task to be scheduled before
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * This operation is optional.
	 */
	int (*schedule_task_before)(void *data, struct task *task, uint64_t start,
				    uint64_t period, struct task *before);

	/**
	 * Schedules task with given scheduling parameters.
	 * @param data Private data of selected scheduler.
	 * @param task Task to be scheduled.
	 * @param start Start time of given task (in microseconds).
	 * @param period Scheduling period of given task (in microseconds).
	 * @param after Task to be scheduled after
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * This operation is optional.
	 */
	int (*schedule_task_after)(void *data, struct task *task, uint64_t start,
				   uint64_t period, struct task *after);

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
	 * @param flags Function specific flags.
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * This operation is optional.
	 */
	void (*scheduler_free)(void *data, uint32_t flags);

	/**
	 * Restores scheduler's resources.
	 * @param data Private data of selected scheduler.
	 * @return 0 if succeeded, error code otherwise.
	 *
	 * This operation is optional.
	 */
	int (*scheduler_restore)(void *data);
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

/** See scheduler_ops::schedule_task */
static inline int schedule_task(struct task *task, uint64_t start,
				uint64_t period)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	if (!task)
		return -EINVAL;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (task->type == sch->type)
			return sch->ops->schedule_task(sch->data, task, start,
						       period);
	}

	return -ENODEV;
}

/** See scheduler_ops::schedule_task_before */
static inline int schedule_task_before(struct task *task, uint64_t start,
				       uint64_t period, struct task *before)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	if (!task || !before)
		return -EINVAL;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (task->type == sch->type) {
			if (sch->ops->schedule_task_before)
				return sch->ops->schedule_task_before(sch->data, task, start,
								      period, before);

			return sch->ops->schedule_task(sch->data, task, start,
						       period);
		}
	}

	return -ENODEV;
}

/** See scheduler_ops::schedule_task_after */
static inline int schedule_task_after(struct task *task, uint64_t start,
				      uint64_t period, struct task *after)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	if (!task || !after)
		return -EINVAL;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (task->type == sch->type) {
			if (sch->ops->schedule_task_after)
				return sch->ops->schedule_task_after(sch->data, task, start,
								     period, after);

			return sch->ops->schedule_task(sch->data, task, start,
						       period);
		}
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
		if (task->type == sch->type)
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
		if (task->type == sch->type)
			return sch->ops->schedule_task_free(sch->data, task);
	}

	return -ENODEV;
}

/** See scheduler_ops::scheduler_free */
static inline void schedule_free(uint32_t flags)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (sch->ops->scheduler_free)
			sch->ops->scheduler_free(sch->data, flags);
	}
}

/** See scheduler_ops::scheduler_restore */
static inline int schedulers_restore(void)
{
	struct schedulers *schedulers = *arch_schedulers_get();
	struct schedule_data *sch;
	struct list_item *slist;

	assert(schedulers);

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (sch->ops->scheduler_restore)
			return sch->ops->scheduler_restore(sch->data);
	}

	return 0;
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

/**
 * Extract scheduler's task information from tasks
 * @param scheduler_props Structure to be filled
 * @param data_off_size Pointer to the current size of the scheduler_props, to be updated
 * @param tasks Scheduler's task list
 */
void scheduler_get_task_info(struct scheduler_props *scheduler_props,
			     uint32_t *data_off_size, struct list_item *tasks);

/** @}*/

#endif /* __SOF_SCHEDULE_SCHEDULE_H__ */
