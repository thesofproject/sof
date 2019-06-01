// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/**
 * \file arch/xtensa/smp/task.c
 * \brief Xtensa SMP task implementation file
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <xtos-structs.h>
#include <arch/cpu.h>
#include <arch/task.h>

struct irq_task **task_irq_low_get(void)
{
	struct core_context *ctx = (struct core_context *)cpu_read_threadptr();

	return &ctx->irq_low_task;
}

struct irq_task **task_irq_med_get(void)
{
	struct core_context *ctx = (struct core_context *)cpu_read_threadptr();

	return &ctx->irq_med_task;
}

struct irq_task **task_irq_high_get(void)
{
	struct core_context *ctx = (struct core_context *)cpu_read_threadptr();

	return &ctx->irq_high_task;
}
