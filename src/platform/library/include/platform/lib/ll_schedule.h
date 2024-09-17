/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 */

#ifndef __LIBRARY_INCLUDE_LIB_SCHEDULE_H__
#define __LIBRARY_INCLUDE_LIB_SCHEDULE_H__

#include <rtos/task.h>
#include <stdint.h>

struct task;
struct sof_uuid_entry;
struct ll_schedule_domain;

void schedule_ll_run_tasks(void);

int scheduler_init_ll(struct ll_schedule_domain *domain);

int schedule_task_init_ll(struct task *task,
			  const struct sof_uuid_entry *uid, uint16_t type,
			  uint16_t priority, enum task_state (*run)(void *data),
			  void *data, uint16_t core, uint32_t flags);

#endif /* __LIBRARY_INCLUDE_LIB_SCHEDULE_H__ */
