/*
 * Copyright (c) 2017, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 */

/**
 * \file arch/xtensa/include/arch/task.h
 * \brief Arch task header file
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __ARCH_TASK_H_
#define __ARCH_TASK_H_

#include <sof/list.h>

#include <arch/spinlock.h>

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

#endif
