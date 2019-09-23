/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file arch/xtensa/include/arch/schedule/task.h
 * \brief Arch task header file
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *          Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifdef __SOF_SCHEDULE_TASK_H__

#ifndef __ARCH_SCHEDULE_TASK_H__
#define __ARCH_SCHEDULE_TASK_H__

/**
 * \brief Returns main task data.
 * \return Pointer to pointer of main task data.
 */
struct task **task_main_get(void);

/**
 * \brief Returns current system context.
 */
volatile void *task_context_get(void);

/**
 * \brief Switches system context.
 * \param[in,out] task_ctx Task context to be set.
 */
void task_context_set(void *task_ctx);

/**
 * \brief Allocates task context.
 * \param[in,out] task_ctx Assigned to allocated structure on return.
 */
int task_context_alloc(void **task_ctx);

/**
 * \brief Initializes task context.
 * \param[in,out] task_ctx Task context to be initialized.
 * \param[in] entry Entry point for task execution.
 * \param[in] arg0 First argument to be passed to entry function.
 * \param[in] arg1 Second argument to be passed to entry function.
 * \param[in] task_core Id of the core that task will be executed on.
 * \param[in] stack Address of the stack, if NULL then allocated internally.
 * \param[in] stack_size Size of the stack, ignored if stack is NULL.
 */
int task_context_init(void *task_ctx, void *entry, void *arg0, void *arg1,
		      int task_core, void *stack, int stack_size);

/**
 * \brief Frees task context.
 * \param[in,out] task_ctx Task with context to be freed.
 */
void task_context_free(void *task_ctx);

/**
 * \brief Performs cache operation on task's context.
 * \param[in,out] task_ctx Context to be wtb/inv.
 * \param[in] cmd Cache operation to be performed.
 */
void task_context_cache(void *task_ctx, int cmd);

#endif /* __ARCH_SCHEDULE_TASK_H__ */

#else

#error "This file shouldn't be included from outside of sof/schedule/task.h"

#endif /* __SOF_SCHEDULE_TASK_H__ */
