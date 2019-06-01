/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <arch/task.h>

struct sof;
struct edf_task;

int do_task_master_core(struct sof *sof);

int do_task_slave_core(struct sof *sof);

static inline int allocate_tasks(void)
{
	return arch_allocate_tasks();
}

static inline int run_task(struct task *task)
{
	return arch_run_task(task);
}

#endif
