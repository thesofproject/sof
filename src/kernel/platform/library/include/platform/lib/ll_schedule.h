/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 */

/**
 * @file platform/lib/ll_schedule.h
 * @brief Platform library / testbench Low-Latency scheduler mock declarations.
 * @ingroup scheduling_api
 */

#ifndef __LIBRARY_INCLUDE_LIB_SCHEDULE_H__
#define __LIBRARY_INCLUDE_LIB_SCHEDULE_H__

#include <rtos/task.h>
#include <stdint.h>

struct task;
struct sof_uuid_entry;
struct ll_schedule_domain;

/**
 * @brief Run one iteration of all active Low-Latency tasks (library testbench harness).
 */
void schedule_ll_run_tasks(void);

/**
 * @brief Initialize Low-Latency scheduler on library/testbench platform.
 * @param[in,out] domain Low-Latency scheduling domain.
 * @return 0 on success, negative error code on failure.
 */
int scheduler_init_ll(struct ll_schedule_domain *domain);

/**
 * @brief Initialize a Low-Latency task descriptor for library/testbench execution.
 * @param[out] task Task structure to initialize.
 * @param[in] uid UUID identifying the task or associated component.
 * @param[in] type Scheduler type identifier (SOF_SCHEDULE_LL_TIMER or SOF_SCHEDULE_LL_DMA).
 * @param[in] priority Scheduling priority level (lower values run first).
 * @param[in] run Main execution callback function.
 * @param[in] data Context pointer passed to run callback.
 * @param[in] core Target DSP core ID.
 * @param[in] flags Task-specific scheduling flags.
 * @return 0 on success, negative error code on failure.
 */
int schedule_task_init_ll(struct task *task,
			  const struct sof_uuid_entry *uid, uint16_t type,
			  int16_t priority, enum task_state (*run)(void *data),
			  void *data, uint16_t core, uint32_t flags);

#endif /* __LIBRARY_INCLUDE_LIB_SCHEDULE_H__ */
