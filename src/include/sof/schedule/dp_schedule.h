/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Szkudlinski
 */

#ifndef __SOF_SCHEDULE_DP_SCHEDULE_H__
#define __SOF_SCHEDULE_DP_SCHEDULE_H__

#include <rtos/task.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <stdint.h>

/**
 *
 * DP scheduler is a scheduler that creates a separate preemptible Zephyr thread for each SOF task
 *   There's only one instance of DP in the system, however, threads can be assigned and pinned
 *   to any core in the system for its execution, there's no SMP processing.
 *
 * The task execution may be delayed and task may be re-scheduled periodically
 * NOTE: delayed start and rescheduling takes place in sync with LL scheduler, meaning the
 *   DP scheduler is triggered as the last task of LL running on a primary core.
 *   That implies a limitation: LL scheduler MUST be running on primary core in order to have
 *   this feature working.
 *   It is fine, because rescheduling is a feature used for data processing when a pipeline is
 *   running.
 *
 * Other possible usage of DP scheduler is to schedule task with DP_SCHEDULER_RUN_TASK_IMMEDIATELY
 *   as start parameter. It will force the task to work without any delays and async to LL.
 *   This kind of scheduling may be used for staring regular zephyr tasks using SOF api
 *
 * Task run() may return:
 *  SOF_TASK_STATE_RESCHEDULE - the task will be rescheduled as specified in scheduler period
 *				note that task won't ever be rescheduled if LL is not running
 *  SOF_TASK_STATE_COMPLETED  - the task will be removed from scheduling,
 *				calling schedule_task will add the task to processing again
 *				task_complete() will be called
 *  SOF_TASK_STATE_CANCEL     - the task will be removed from scheduling,
 *				calling schedule_task will add the task to processing again
 *				task_complete() won't be called
 *  other statuses - assert will go off
 *
 * NOTE: task - means a SOF task
 *	 thread - means a Zephyr preemptible thread
 *
 * TODO  - EDF:
 * Threads run on the same priority, lower than thread running LL tasks. Zephyr EDF mechanism
 * is used for decision which thread/task is to be scheduled next. The DP scheduler calculates
 * the task deadline and set it in Zephyr thread properties, the final scheduling decision is made
 * by Zephyr.
 *
 * Each time tick the scheduler iterates through the list of all active tasks and calculates
 * a deadline based on
 *  - knowledge how the modules are bound
 *  - declared time required by a task to complete processing
 *  - the deadline of the last module
 *
 */

/** \brief tell the scheduler to run the task immediately, even if LL tick is not yet running */
#define SCHEDULER_DP_RUN_TASK_IMMEDIATELY ((uint64_t)-1)

/**
 * \brief Init the Data Processing scheduler
 */
int scheduler_dp_init(void);

/**
 * \brief Set the Data Processing scheduler to be accessible at secondary cores
 */
int scheduler_dp_init_secondary_core(void);

/**
 * \brief initialize a DP task and add it to scheduling
 *
 * \param[out] task pointer, pointer to allocated task structure will be return
 * \param[in] uid pointer to UUID of the task
 * \param[in] ops pointer to task functions
 * \param[in] data pointer to the thread private data
 * \param[in] core CPU the thread should run on
 * \param[in] stack_size size of stack for a zephyr task
 * \param[in] task_priority priority of the zephyr task
 */
int scheduler_dp_task_init(struct task **task,
			   const struct sof_uuid_entry *uid,
			   const struct task_ops *ops,
			   void *data,
			   uint16_t core,
			   size_t stack_size,
			   uint32_t task_priority);

#endif /* __SOF_SCHEDULE_DP_SCHEDULE_H__ */
