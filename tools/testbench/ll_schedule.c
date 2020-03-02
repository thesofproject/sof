// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>

int schedule_task_init_ll(struct task *task, uint32_t uid, uint16_t type,
			  uint16_t priority,
			  enum task_state (*run)(void *data), void *data,
			  uint16_t core, uint32_t flags)
{
	int ret;

	ret = schedule_task_init(task, uid, type, priority, run, data, core,
				 flags);

	return ret;
}
