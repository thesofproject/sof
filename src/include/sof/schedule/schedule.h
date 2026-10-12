/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 */

/**
 * @file sof/schedule/schedule.h
 * @brief Generic scheduler interface, scheduler operations, and task management.
 * @ingroup scheduling_api
 */

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

/** \addtogroup scheduling_api
 *  @{
 */

enum {
	/**
	 * @brief Earliest Deadline First (EDF) scheduler.
	 * @deprecated Deprecated for modern audio pipeline processing. In Zephyr-based builds,
	 *             EDF is implemented as a delayed workqueue (edf_workq) retained for
	 *             host IPC processing and legacy KPB draining tasks until migration.
	 *             Audio pipeline components should use @ref SOF_SCHEDULE_DP for preemptible
	 *             deadline-driven tasks or @ref SOF_SCHEDULE_LL_TIMER / @ref SOF_SCHEDULE_LL_DMA
	 *             for hard real-time tasks.
	 */
	SOF_SCHEDULE_EDF = 0,
	/**
	 * @brief Low Latency timer domain scheduler.
	 *
	 * Executes tasks immediately upon expiration of the primary hardware timer tick.
	 */
	SOF_SCHEDULE_LL_TIMER,
	/**
	 * @brief Low Latency DMA interrupt domain scheduler.
	 *
	 * Executes tasks immediately upon scheduling component's DMA transfer completion interrupt.
	 */
	SOF_SCHEDULE_LL_DMA,
	/**
	 * @brief Data Processing (DP) worker thread scheduler.
	 *
	 * Preemptible Zephyr worker thread per SOF task. Evaluates task readiness
	 * after each Low-Latency (LL) tick, setting absolute deadlines in Zephyr EDF.
	 */
	SOF_SCHEDULE_DP,
	/**
	 * @brief Tasks With Budget (TWB) scheduler.
	 *
	 * Preemptible Zephyr thread per SOF task with a pre-allocated cycle budget
	 * renewed every system tick. Drops thread priority on budget exhaustion to
	 * prevent system starvation.
	 */
	SOF_SCHEDULE_TWB,
	/**
	 * @brief Indicates total number of scheduler types.
	 */
	SOF_SCHEDULE_COUNT
};

/** @brief Scheduler free available flags. */
#define SOF_SCHEDULER_FREE_IRQ_ONLY	BIT(0) /**< Free function disables interrupts only, keeping data structures intact. */

/**
 * @brief Scheduler operations interface table.
 *
 * Schedulers implement this interface to plug into the unified SOF scheduling abstraction.
 * Almost all schedule operations must return 0 for success and negative values for errors.
 * Only the optional scheduler_free operation returns void.
 */
struct scheduler_ops {
	/**
	 * @brief Schedule a task with the given timing parameters.
	 * @param[in,out] data Private state of the selected scheduler.
	 * @param[in,out] task Task descriptor to be scheduled.
	 * @param[in] start Start timing parameter (scheduler-specific, e.g. start delay or absolute tick for LL).
	 * @param[in] period Repetition period in microseconds for periodic schedulers (such as LL, where 0 denotes
	 *                   one-shot). Schedulers such as DP and TWB ignore this parameter as rescheduling is
	 *                   governed by the task's returned execution state.
	 * @return 0 on success, negative error code on failure.
	 *
	 * This operation is mandatory.
	 */
	int (*schedule_task)(void *data, struct task *task, uint64_t start,
			     uint64_t period);

	/**
	 * @brief Schedule a task to execute before a specified reference task.
	 * @param[in,out] data Private state of the selected scheduler.
	 * @param[in,out] task Task descriptor to be scheduled.
	 * @param[in] start Start timing parameter (scheduler-specific).
	 * @param[in] period Repetition period in microseconds (scheduler-specific).
	 * @param[in] before Reference task before which this task must be enqueued.
	 * @return 0 on success, negative error code on failure.
	 *
	 * This operation is optional.
	 */
	int (*schedule_task_before)(void *data, struct task *task, uint64_t start,
				    uint64_t period, struct task *before);

	/**
	 * @brief Schedule a task to execute after a specified reference task.
	 * @param[in,out] data Private state of the selected scheduler.
	 * @param[in,out] task Task descriptor to be scheduled.
	 * @param[in] start Start timing parameter (scheduler-specific).
	 * @param[in] period Repetition period in microseconds (scheduler-specific).
	 * @param[in] after Reference task after which this task must be enqueued.
	 * @return 0 on success, negative error code on failure.
	 *
	 * This operation is optional.
	 */
	int (*schedule_task_after)(void *data, struct task *task, uint64_t start,
				   uint64_t period, struct task *after);

	/**
	 * @brief Transition a task to running state and perform scheduler-specific execution hooks.
	 * @param[in,out] data Private state of the selected scheduler.
	 * @param[in,out] task Task transitioning to running state.
	 * @return 0 on success, negative error code on failure.
	 *
	 * This operation is optional.
	 */
	int (*schedule_task_running)(void *data, struct task *task);

	/**
	 * @brief Reschedule an already scheduled task with an updated start delay.
	 * @param[in,out] data Private state of the selected scheduler.
	 * @param[in,out] task Task descriptor to reschedule.
	 * @param[in] start New start delay in microseconds.
	 * @return 0 on success, negative error code on failure.
	 *
	 * This operation is optional.
	 */
	int (*reschedule_task)(void *data, struct task *task, uint64_t start);

	/**
	 * @brief Cancel a previously scheduled task.
	 * @param[in,out] data Private state of the selected scheduler.
	 * @param[in,out] task Task descriptor to cancel.
	 * @return 0 on success, negative error code on failure.
	 *
	 * This operation is mandatory.
	 */
	int (*schedule_task_cancel)(void *data, struct task *task);

	/**
	 * @brief Free scheduler resources associated with a task.
	 * @param[in,out] data Private state of the selected scheduler.
	 * @param[in,out] task Task descriptor whose resources are to be freed.
	 * @return 0 on success, negative error code on failure.
	 *
	 * This operation is mandatory.
	 */
	int (*schedule_task_free)(void *data, struct task *task);

#if CONFIG_SOF_BOOT_TEST_STANDALONE || CONFIG_LIBRARY
	/**
	 * @brief Free the scheduler instance and release its underlying resources.
	 * @param[in,out] data Private state of the selected scheduler.
	 * @param[in] flags Scheduler-specific free flags (e.g. @ref SOF_SCHEDULER_FREE_IRQ_ONLY).
	 *
	 * This operation is optional (used only in standalone testing and library testbench).
	 */
	void (*scheduler_free)(void *data, uint32_t flags);
#endif

	/**
	 * @brief Initialize execution thread context for a scheduled task.
	 * @param[in,out] data Private state of the selected scheduler.
	 * @param[in,out] task Task requiring dedicated execution context.
	 * @return Pointer to allocated Zephyr thread object, or NULL on error or unsupported.
	 *
	 * This operation is optional.
	 */
	struct k_thread *(*scheduler_init_context)(void *data, struct task *task);
};

/**
 * @brief Descriptor container representing a registered scheduler instance.
 */
struct schedule_data {
	struct list_item list;			/**< List node linking this scheduler into the per-core schedulers list. */
	int type;				/**< Scheduler type identifier (SOF_SCHEDULE_*). */
	const struct scheduler_ops *ops;	/**< Pointer to scheduler operations table. */
	void *data;				/**< Pointer to scheduler-specific private context data. */
};

/**
 * @brief Container holding the linked list of all registered schedulers on a core.
 */
struct schedulers {
	struct list_item list;	/**< Head of the linked list of registered schedulers. */
};

/**
 * @brief Retrieve the array of registered kernel-space schedulers for the current core.
 *
 * Must only be called from privileged/kernel context (!k_is_user_context()).
 *
 * @return Pointer to the per-core schedulers list head pointer.
 */
struct schedulers **arch_schedulers_get(void);

/**
 * @brief Retrieve registered schedulers for user-space threads on the current core.
 *
 * Invokes cpu_get_id(), so this function must only be called from kernel context.
 * User-space callers should use @ref arch_user_schedulers_get_for_core instead.
 *
 * @return Pointer to the per-core user schedulers list head pointer, or NULL if user LL is disabled.
 */
struct schedulers **arch_user_schedulers_get(void);

/**
 * @brief Retrieve registered schedulers for user-space threads on a specific core.
 *
 * Safe to invoke from user-space contexts because the target core ID is explicitly provided.
 *
 * @param[in] core Target DSP core ID.
 * @return Pointer to the user schedulers list head pointer for the specified core, or NULL if disabled.
 */
struct schedulers **arch_user_schedulers_get_for_core(int core);

/**
 * @brief Lookup scheduler private context data by scheduler type from a schedulers container.
 * @param[in] schedulers Schedulers list container.
 * @param[in] type Scheduler type identifier (SOF_SCHEDULE_*).
 * @return Pointer to scheduler-specific private data, or NULL if not found.
 */
static inline void *scheduler_list_get_data(struct schedulers *schedulers, uint16_t type)
{
	struct schedule_data *sch;
	struct list_item *slist;

	if (!schedulers)
		return NULL;

	list_for_item(slist, &schedulers->list) {
		sch = container_of(slist, struct schedule_data, list);
		if (type == sch->type)
			return sch->data;
	}

	return NULL;
}

/**
 * @brief Retrieve private data of a registered kernel scheduler on the current core.
 * @param[in] type Scheduler type (SOF_SCHEDULE_*).
 * @return Pointer to scheduler private data, or NULL if not found.
 */
static inline void *scheduler_get_data(uint16_t type)
{
	return scheduler_list_get_data(*arch_schedulers_get(), type);
}

/**
 * @brief Retrieve private data of a registered user-space scheduler on the current core.
 * @param[in] type Scheduler type (SOF_SCHEDULE_*).
 * @return Pointer to scheduler private data, or NULL if not found.
 */
static inline void *scheduler_get_user_data(uint16_t type)
{
	return scheduler_list_get_data(*arch_user_schedulers_get(), type);
}

/**
 * @brief Notify the scheduler that a task is transitioning to the running state.
 * @param[in,out] task Task being set to running.
 * @return 0 on success, or negative error code from scheduler ops.
 */
static inline int schedule_task_running(struct task *task)
{
	struct schedule_data *sch;

	assert(task);
	sch = task->sch;

	if (!sch->ops->schedule_task_running)
		return 0;

	return sch->ops->schedule_task_running(sch->data, task);
}

/**
 * @brief Schedule a task for execution with start delay and period.
 * @param[in,out] task Task to schedule.
 * @param[in] start Start delay in microseconds from now.
 * @param[in] period Repetition period in microseconds (0 for one-shot execution).
 * @return 0 on success, -EINVAL if task is NULL, or negative error code from scheduler ops.
 */
static inline int schedule_task(struct task *task, uint64_t start,
				uint64_t period)
{
	struct schedule_data *sch;

	if (!task)
		return -EINVAL;

	sch = task->sch;

	return sch->ops->schedule_task(sch->data, task, start, period);
}

/**
 * @brief Schedule a task to execute before a specified reference task.
 *
 * @note Relative task ordering is optional and scheduler-specific. If the underlying
 *       scheduler does not implement schedule_task_before, this wrapper falls back to
 *       ordinary schedule_task() and ignores @p before.
 *
 * @param[in,out] task Task to schedule.
 * @param[in] start Start timing parameter (scheduler-specific).
 * @param[in] period Repetition period in microseconds (scheduler-specific).
 * @param[in] before Reference task before which this task must run.
 * @return 0 on success, -EINVAL if arguments are invalid, or negative error code from scheduler ops.
 */
static inline int schedule_task_before(struct task *task, uint64_t start,
				       uint64_t period, struct task *before)
{
	struct schedule_data *sch;

	if (!task || !before)
		return -EINVAL;

	sch = task->sch;

	if (sch->ops->schedule_task_before)
		return sch->ops->schedule_task_before(sch->data, task, start,
						      period, before);

	return sch->ops->schedule_task(sch->data, task, start, period);
}

/**
 * @brief Schedule a task to execute after a specified reference task.
 *
 * @note Relative task ordering is optional and scheduler-specific. If the underlying
 *       scheduler does not implement schedule_task_after, this wrapper falls back to
 *       ordinary schedule_task() and ignores @p after.
 *
 * @param[in,out] task Task to schedule.
 * @param[in] start Start timing parameter (scheduler-specific).
 * @param[in] period Repetition period in microseconds (scheduler-specific).
 * @param[in] after Reference task after which this task must run.
 * @return 0 on success, -EINVAL if arguments are invalid, or negative error code from scheduler ops.
 */
static inline int schedule_task_after(struct task *task, uint64_t start,
				      uint64_t period, struct task *after)
{
	struct schedule_data *sch;

	if (!task || !after)
		return -EINVAL;

	sch = task->sch;

	if (sch->ops->schedule_task_after)
		return sch->ops->schedule_task_after(sch->data, task, start,
						     period, after);

	return sch->ops->schedule_task(sch->data, task, start, period);
}

/**
 * @brief Reschedule an already scheduled task with an updated start time.
 * @param[in,out] task Task to reschedule.
 * @param[in] start New start delay in microseconds.
 * @return 0 on success, or negative error code from scheduler ops.
 */
static inline int reschedule_task(struct task *task, uint64_t start)
{
	struct schedule_data *sch;

	assert(task);
	sch = task->sch;

	/* optional operation */
	if (!sch->ops->reschedule_task)
		return 0;

	return sch->ops->reschedule_task(sch->data, task, start);
}

/**
 * @brief Cancel a scheduled or queued task.
 * @param[in,out] task Task to cancel.
 * @return 0 on success, -EINVAL if task or bound scheduler is NULL, or negative error code from scheduler ops.
 */
static inline int schedule_task_cancel(struct task *task)
{
	struct schedule_data *sch;

	if (!task || !task->sch)
		return -EINVAL;

	sch = task->sch;

	return sch->ops->schedule_task_cancel(sch->data, task);
}

/**
 * @brief Free scheduler resources associated with a task.
 * @param[in,out] task Task whose resources should be freed.
 * @return 0 on success, -EINVAL if task or bound scheduler is NULL, or negative error code from scheduler ops.
 */
static inline int schedule_task_free(struct task *task)
{
	struct schedule_data *sch;

	if (!task || !task->sch)
		return -EINVAL;

	sch = task->sch;

	return sch->ops->schedule_task_free(sch->data, task);
}

/* Only used in a stand-alone test and in a testbench test */
#if CONFIG_SOF_BOOT_TEST_STANDALONE || CONFIG_LIBRARY
/**
 * @brief Free all registered schedulers and their resources (testing/library only).
 * @param[in] flags Scheduler-specific free flags (e.g. @ref SOF_SCHEDULER_FREE_IRQ_ONLY).
 */
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
#endif

/**
 * @brief Initialize thread execution context for a task.
 * @param[in,out] task Task requiring execution context initialization.
 * @return Pointer to allocated Zephyr thread, or NULL if unsupported or failed.
 */
static inline struct k_thread *scheduler_init_context(struct task *task)
{
	struct schedule_data *sch;

	assert(task && task->sch);
	sch = task->sch;

	if (sch->ops->scheduler_init_context)
		return sch->ops->scheduler_init_context(sch->data, task);

	return NULL;
}

/**
 * @brief Initialize a scheduling task and bind it to the registered scheduler for its type and core.
 *
 * Validates scheduler type and searches registered schedulers. User schedulers look up the
 * target core via arch_user_schedulers_get_for_core(core), whereas kernel schedulers look up
 * via arch_schedulers_get() on the calling core. Therefore, kernel task initialization must
 * execute directly on the target core. Initializes task properties, priority, execution callback,
 * and sets task state to @ref SOF_TASK_STATE_INIT.
 *
 * @param[out] task Task structure to initialize.
 * @param[in] uid Statically assigned task UUID entry.
 * @param[in] type Scheduler type (SOF_SCHEDULE_*).
 * @param[in] priority Task scheduling priority (lower values indicate higher priority).
 * @param[in] run Execution callback function returning @ref task_state.
 * @param[in] data Private context pointer passed to run callback.
 * @param[in] core DSP core ID on which the task should execute.
 * @param[in] flags Task-specific scheduling flags.
 * @return 0 on success, -EINVAL if type >= SOF_SCHEDULE_COUNT, -ENODEV if scheduler cannot be found.
 */
int schedule_task_init(struct task *task,
		       const struct sof_uuid_entry *uid, uint16_t type,
		       int16_t priority, enum task_state (*run)(void *data),
		       void *data, uint16_t core, uint32_t flags);

/**
 * @brief Register a scheduler type instance with its operations table and private context data.
 *
 * Allocates a @ref schedule_data container on the system (or user) heap, links it into
 * the active core scheduler list, and associates the provided operations.
 *
 * @param[in] type Scheduler type identifier (SOF_SCHEDULE_*).
 * @param[in] ops Pointer to scheduler operations table (must provide schedule_task, schedule_task_cancel, schedule_task_free).
 * @param[in] data Pointer to scheduler private instance data.
 */
void scheduler_init(int type, const struct scheduler_ops *ops, void *data);

/**
 * @brief Extract scheduler task telemetry and properties for IPC4 base firmware.
 *
 * Populates @ref scheduler_props with the calling core ID and total task count,
 * appending @ref task_props structures for each active task in the list.
 *
 * @param[out] scheduler_props Properties structure to be filled.
 * @param[in,out] data_off_size Pointer to running byte size of scheduler_props buffer.
 * @param[in] tasks Linked list of tasks to inspect.
 */
void scheduler_get_task_info(struct scheduler_props *scheduler_props,
			     uint32_t *data_off_size, struct list_item *tasks);

/** @}*/

#endif /* __SOF_SCHEDULE_SCHEDULE_H__ */
