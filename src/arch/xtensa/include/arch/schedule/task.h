/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/**
 * \file arch/xtensa/include/arch/schedule/task.h
 * \brief Arch task header file
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_SCHEDULE_TASK_H__

#ifndef __ARCH_SCHEDULE_TASK_H__
#define __ARCH_SCHEDULE_TASK_H__

#include <sof/list.h>
#include <sof/spinlock.h>
#include <stdint.h>

/** \brief IRQ task data. */
struct irq_task {
	spinlock_t lock;	/**< lock */
	struct list_item list;	/**< list of tasks */
	uint32_t irq;		/**< IRQ level */
};

struct task;

/**
 * \brief Returns IRQ low task data.
 * \return Pointer to pointer of IRQ low task data.
 */
struct irq_task **task_irq_low_get(void);

/**
 * \brief Returns IRQ medium task data.
 * \return Pointer to pointer of IRQ medium task data.
 */
struct irq_task **task_irq_med_get(void);

/**
 * \brief Returns IRQ high task data.
 * \return Pointer to pointer of IRQ high task data.
 */
struct irq_task **task_irq_high_get(void);

/**
 * \brief Runs task.
 * \param[in,out] task Task data.
 */
int arch_run_task(struct task *task);

/**
 * \brief Allocates IRQ tasks.
 */
int arch_allocate_tasks(void);

/**
 * \brief Frees IRQ tasks.
 */
void arch_free_tasks(void);

#endif /* __ARCH_SCHEDULE_TASK_H__ */

#else

#error "This file shouldn't be included from outside of sof/schedule/task.h"

#endif /* __SOF_SCHEDULE_TASK_H__ */
