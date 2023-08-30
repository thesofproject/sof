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

struct processing_module;

/**
 *
 * DP scheduler is a scheduler that creates a separate preemptible Zephyr thread for each SOF task
 *
 * The task execution may be delayed and task may be re-scheduled periodically
 * NOTE: delayed start and rescheduling takes place in sync with LL scheduler, meaning the
 *   DP scheduler is triggered on each core after all LL task have been completed
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

/**
 * \brief Init the Data Processing scheduler
 */
int scheduler_dp_init(void);

/**
 * \brief initialize a DP task and add it to scheduling
 * It must be called on core the task is declared to run on
 *
 * \param[out] task pointer, pointer to allocated task structure will be return
 * \param[in] uid pointer to UUID of the task
 * \param[in] ops pointer to task functions
 * \param[in] mod pointer to the module to be run
 * \param[in] core CPU the thread should run on
 * \param[in] stack_size size of stack for a zephyr task
 * \param[in] task_priority priority of the zephyr task
 */
int scheduler_dp_task_init(struct task **task,
			   const struct sof_uuid_entry *uid,
			   const struct task_ops *ops,
			   struct processing_module *mod,
			   uint16_t core,
			   size_t stack_size,
			   uint32_t task_priority);

#endif /* __SOF_SCHEDULE_DP_SCHEDULE_H__ */
