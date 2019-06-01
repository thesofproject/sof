// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/**
 * \file arch/xtensa/up/task.c
 * \brief Xtensa UP task implementation file
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <arch/task.h>

/** \brief IRQ low task data pointer. */
static struct irq_task *irq_low_task;

/** \brief IRQ medium task data pointer. */
static struct irq_task *irq_med_task;

/** \brief IRQ high task data pointer. */
static struct irq_task *irq_high_task;

struct irq_task **task_irq_low_get(void)
{
	return &irq_low_task;
}

struct irq_task **task_irq_med_get(void)
{
	return &irq_med_task;
}

struct irq_task **task_irq_high_get(void)
{
	return &irq_high_task;
}
