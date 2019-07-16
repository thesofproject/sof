// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/schedule/task.h>
#include <stdint.h>
#include <sof/lib/wait.h>

/* TODO: Make host ll ops implementation */
struct scheduler_ops schedule_ll_ops = {
	.schedule_task = NULL,
	.schedule_task_init = NULL,
	.schedule_task_running = NULL,
	.schedule_task_complete = NULL,
	.reschedule_task = NULL,
	.schedule_task_cancel = NULL,
	.schedule_task_free = NULL,
	.scheduler_init = NULL,
	.scheduler_free = NULL,
	.scheduler_run = NULL
};
