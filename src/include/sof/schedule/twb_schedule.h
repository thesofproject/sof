/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Bonislawski <adrian.bonislawski@intel.com>
 */

#ifndef __SOF_SCHEDULE_TWB_SCHEDULE_H__
#define __SOF_SCHEDULE_TWB_SCHEDULE_H__

#include <rtos/task.h>
#include <stdint.h>

/**
 * @brief Task With Budget (TWB) Scheduler
 *
 * TWB scheduler is a scheduler that creates a separate preemptible Zephyr thread
 * for each SOF task that has a pre-allocated MCPS budget renewed with every system tick.
 * The TWB scheduler assigns either MEDIUM_PRIORITY or LOW_PRIORITY to the task thread
 * based on the budget left in the current system tick.
 * It allows for opportunistic execution if there is no other ready task
 * with a higher priority while the budget is already spent.
 *
 * Examples of tasks with budget include IPC Task and IDC Task.
 *
 * The TWB scheduler has two key parameters assigned:
 * - cycles granted: the budget per system tick
 * - cycles consumed: the number of cycles consumed in a given system tick for task execution
 *
 * The number of cycles consumed is reset to 0 at the beginning of each system tick,
 * renewing the TWB budget.
 * When the number of cycles consumed exceeds the cycles granted,
 * the task is switchedfrom MEDIUM to LOW priority.
 * When the task with budget thread is created, the MPP Scheduling is responsible
 * for setting the thread time slice equal to the task budget, along with
 * setting a callback on time slice timeout.
 * Thread time slicing guarantees that the Zephyr scheduler will interrupt execution
 * when the budget is spent,
 * so the MPP Scheduling timeout callback can re-evaluate the task priority.
 *
 * If there is a budget left in some system tick
 * (i.e., the task spent less time or started executing closeto the system tick
 * that preempts execution), it is reset and not carried over to the next tick.
 *
 * More info:
 * https://thesofproject.github.io/latest/architectures/firmware/sof-zephyr/mpp_layer/mpp_scheduling.html
 */

/**
 * \brief default static stack size for each TWB thread
 */
#define ZEPHYR_TWB_STACK_SIZE 8192

/**
 * \brief max budget limit
 */
#define ZEPHYR_TWB_BUDGET_MAX (CONFIG_SYS_CLOCK_TICKS_PER_SEC / LL_TIMER_PERIOD_US)

/**
 * \brief Init the Tasks with Budget scheduler
 */
int scheduler_twb_init(void);

/**
 * \brief initialize a TWB task and add it to scheduling
 * It must be called on core the task is declared to run on
 *
 * \param[out] task pointer, pointer to allocated task structure will be return
 * \param[in] uid pointer to UUID of the task
 * \param[in] ops pointer to task functions
 * \param[in] data pointer to the task data
 * \param[in] core CPU the thread should run on
 * \param[in] name zephyr thread name
 * \param[in] stack_size size of stack for a zephyr thread
 * \param[in] thread_priority priority of the zephyr thread
 * \param[in] cycles_granted cycles budget for the zephyr thread
 */
int scheduler_twb_task_init(struct task **task,
			    const struct sof_uuid_entry *uid,
			    const struct task_ops *ops,
			    void *data,
			    int32_t core,
			    const char *name,
			    size_t stack_size,
			    int32_t thread_priority,
			    uint32_t cycles_granted);

#endif /* __SOF_SCHEDULE_TWB_SCHEDULE_H__ */
