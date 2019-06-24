/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/**
 * \file arch/xtensa/include/arch/task.h
 * \brief Arch task header file
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __ARCH_TASK_H_
#define __ARCH_TASK_H_

#include <sof/schedule/schedule.h>

/**
 * \brief Allocates IRQ tasks.
 */
static inline int arch_allocate_tasks(void)
{
	return 0;
}

/**
 * \brief Runs task.
 * \param[in,out] task Task data.
 */
static inline int arch_run_task(struct task *task)
{
	return 0;
}

#endif
