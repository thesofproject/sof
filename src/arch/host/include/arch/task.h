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

#ifdef __SOF_TASK_H__

#ifndef __ARCH_TASK_H__
#define __ARCH_TASK_H__

struct task;

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

#endif /* __ARCH_TASK_H__ */

#else

#error "This file shouldn't be included from outside of sof/task.h"

#endif /* __SOF_TASK_H__ */
