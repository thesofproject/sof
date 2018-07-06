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

#include <sof/schedule.h>
#include <sof/interrupt.h>
#include <platform/platform.h>
#include <sof/debug.h>
#include <sof/alloc.h>

/** \brief IRQ task data. */
struct irq_task {
	spinlock_t lock;	/**< lock */
	struct list_item list;	/**< list of tasks */
	uint32_t irq;		/**< IRQ level */
};

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
 * \brief Retrieves task IRQ level.
 * \param[in,out] task Task data.
 * \return IRQ level.
 */
static inline uint32_t task_get_irq(struct task *task)
{
	uint32_t irq;

	switch (task->priority) {
	case TASK_PRI_MED + 1 ... TASK_PRI_LOW:
		irq = PLATFORM_IRQ_TASK_LOW;
		break;
	case TASK_PRI_HIGH ... TASK_PRI_MED - 1:
		irq = PLATFORM_IRQ_TASK_HIGH;
		break;
	case TASK_PRI_MED:
	default:
		irq = PLATFORM_IRQ_TASK_MED;
		break;
	}

	return irq;
}

/**
 * \brief Adds task to the list per IRQ level.
 * \param[in,out] task Task data.
 */
static inline void task_set_data(struct task *task)
{
	struct list_item *dst = NULL;

	switch (task->priority) {
	case TASK_PRI_MED + 1 ... TASK_PRI_LOW:
		dst = &((*task_irq_low_get())->list);
		break;
	case TASK_PRI_HIGH ... TASK_PRI_MED - 1:
		dst = &((*task_irq_high_get())->list);
		break;
	case TASK_PRI_MED:
	default:
		dst = &((*task_irq_med_get())->list);
		break;
	}
	list_item_append(&task->irq_list, dst);
}

/**
 * \brief Interrupt handler for the IRQ task.
 * \param[in,out] arg IRQ task data.
 */
static void _irq_task(void *arg)
{
	struct irq_task *irq_task = *(struct irq_task **)arg;
	struct list_item *tlist;
	struct list_item *clist;
	struct task *task;
	uint32_t flags;

	/* intentionally don't lock list to have task added from schedule irq */
	list_for_item(tlist, &irq_task->list) {
		task = container_of(tlist, struct task, irq_list);

		if (task->func)
			task->func(task->data);

		schedule_task_complete(task);
	}

	spin_lock_irq(&irq_task->lock, flags);

	list_for_item_safe(clist, tlist, &irq_task->list) {
		task = container_of(clist, struct task, irq_list);
		list_item_del(&task->irq_list);
	}

	interrupt_clear(irq_task->irq);

	spin_unlock_irq(&irq_task->lock, flags);
}

/**
 * \brief Runs task.
 * \param[in,out] task Task data.
 */
static inline void arch_run_task(struct task *task)
{
	uint32_t irq;

	task_set_data(task);
	irq = task_get_irq(task);
	interrupt_set(irq);
}

/**
 * \brief Allocates IRQ tasks.
 */
static inline void arch_allocate_tasks(void)
{
	/* irq low */
	struct irq_task **low = task_irq_low_get();
	*low = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(**low));
	list_init(&((*low)->list));
	spinlock_init(&((*low)->lock));
	(*low)->irq = PLATFORM_IRQ_TASK_LOW;

	/* irq medium */
	struct irq_task **med = task_irq_med_get();
	*med = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(**med));
	list_init(&((*med)->list));
	spinlock_init(&((*med)->lock));
	(*med)->irq = PLATFORM_IRQ_TASK_MED;

	/* irq high */
	struct irq_task **high = task_irq_high_get();
	*high = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(**high));
	list_init(&((*high)->list));
	spinlock_init(&((*high)->lock));
	(*high)->irq = PLATFORM_IRQ_TASK_HIGH;
}

/**
 * \brief Frees IRQ tasks.
 */
static inline void arch_free_tasks(void)
{
	uint32_t flags;

	/* free IRQ low task */
	struct irq_task **low = task_irq_low_get();

	spin_lock_irq(&(*low)->lock, flags);
	interrupt_disable(PLATFORM_IRQ_TASK_LOW);
	interrupt_unregister(PLATFORM_IRQ_TASK_LOW);
	list_item_del(&(*low)->list);
	spin_unlock_irq(&(*low)->lock, flags);

	rfree(*low);

	/* free IRQ medium task */
	struct irq_task **med = task_irq_med_get();

	spin_lock_irq(&(*med)->lock, flags);
	interrupt_disable(PLATFORM_IRQ_TASK_MED);
	interrupt_unregister(PLATFORM_IRQ_TASK_MED);
	list_item_del(&(*med)->list);
	spin_unlock_irq(&(*med)->lock, flags);

	rfree(*med);

	/* free IRQ high task */
	struct irq_task **high = task_irq_high_get();

	spin_lock_irq(&(*high)->lock, flags);
	interrupt_disable(PLATFORM_IRQ_TASK_HIGH);
	interrupt_unregister(PLATFORM_IRQ_TASK_HIGH);
	list_item_del(&(*high)->list);
	spin_unlock_irq(&(*high)->lock, flags);

	rfree(*high);
}

/**
 * \brief Assigns IRQ tasks to interrupts.
 */
static inline int arch_assign_tasks(void)
{
	/* irq low */
	interrupt_register(PLATFORM_IRQ_TASK_LOW, _irq_task,
			   task_irq_low_get());
	interrupt_enable(PLATFORM_IRQ_TASK_LOW);

	/* irq medium */
	interrupt_register(PLATFORM_IRQ_TASK_MED, _irq_task,
			   task_irq_med_get());
	interrupt_enable(PLATFORM_IRQ_TASK_MED);

	/* irq high */
	interrupt_register(PLATFORM_IRQ_TASK_HIGH, _irq_task,
			   task_irq_high_get());
	interrupt_enable(PLATFORM_IRQ_TASK_HIGH);

	return 0;
}

#endif
