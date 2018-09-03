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

#include <sof/schedule.h>
#include <sof/interrupt.h>
#include <platform/platform.h>
#include <sof/debug.h>
#include <arch/task.h>
#include <sof/alloc.h>
#include <stdint.h>
#include <errno.h>

struct irq_task {
	spinlock_t lock;
	struct list_item list;	/* list of tasks per irq */
	uint32_t irq;
};

static struct irq_task *irq_low_task;
static struct irq_task *irq_med_task;
static struct irq_task *irq_high_task;

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

static inline void task_set_data(struct task *task)
{
	struct list_item *dst = NULL;
	struct irq_task *irq_task;
	uint32_t flags;

	switch (task->priority) {
	case TASK_PRI_MED + 1 ... TASK_PRI_LOW:
		irq_task = irq_low_task;
		dst = &irq_task->list;
		break;
	case TASK_PRI_HIGH ... TASK_PRI_MED - 1:
		irq_task = irq_high_task;
		dst = &irq_task->list;
		break;
	case TASK_PRI_MED:
	default:
		irq_task = irq_med_task;
		dst = &irq_task->list;
		break;
	}

	spin_lock_irq(&irq_task->lock, flags);
	list_item_append(&task->irq_list, dst);
	spin_unlock_irq(&irq_task->lock, flags);
}

static void _irq_task(void *arg)
{
	struct irq_task *irq_task = *(struct irq_task **)arg;
	struct list_item *tlist;
	struct list_item *clist;
	struct task *task;
	uint32_t flags;

	spin_lock_irq(&irq_task->lock, flags);
	list_for_item_safe(clist, tlist, &irq_task->list) {

		task = container_of(clist, struct task, irq_list);
		list_item_del(clist);

		spin_unlock_irq(&irq_task->lock, flags);

		if (task->func && task->state == TASK_STATE_RUNNING)
			task->func(task->data);

		schedule_task_complete(task);
		spin_lock_irq(&irq_task->lock, flags);
	}

	interrupt_clear(irq_task->irq);

	spin_unlock_irq(&irq_task->lock, flags);
}

/* architecture specific method of running task */
void arch_run_task(struct task *task)
{
	uint32_t irq;

	task_set_data(task);
	irq = task_get_irq(task);
	interrupt_set(irq);
}

void arch_allocate_tasks(void)
{
	/* irq low */
	irq_low_task = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM,
			sizeof(*irq_low_task));
	list_init(&irq_low_task->list);
	spinlock_init(&irq_low_task->lock);
	irq_low_task->irq = PLATFORM_IRQ_TASK_LOW;

	/* irq medium */
	irq_med_task = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM,
			sizeof(*irq_med_task));
	list_init(&irq_med_task->list);
	spinlock_init(&irq_med_task->lock);
	irq_med_task->irq = PLATFORM_IRQ_TASK_MED;

	/* irq high */
	irq_high_task = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM,
			sizeof(*irq_high_task));
	list_init(&irq_high_task->list);
	spinlock_init(&irq_high_task->lock);
	irq_high_task->irq = PLATFORM_IRQ_TASK_HIGH;
}

int arch_assign_tasks(void)
{
	/* irq low */
	interrupt_register(PLATFORM_IRQ_TASK_LOW, _irq_task, &irq_low_task);
	interrupt_enable(PLATFORM_IRQ_TASK_LOW);

	/* irq medium */
	interrupt_register(PLATFORM_IRQ_TASK_MED, _irq_task, &irq_med_task);
	interrupt_enable(PLATFORM_IRQ_TASK_MED);

	/* irq high */
	interrupt_register(PLATFORM_IRQ_TASK_HIGH, _irq_task, &irq_high_task);
	interrupt_enable(PLATFORM_IRQ_TASK_HIGH);

	return 0;
}
