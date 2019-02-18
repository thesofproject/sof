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
 */

/**
 * \file arch/xtensa/task.c
 * \brief Arch task implementation file
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <sof/alloc.h>
#include <sof/debug.h>
#include <sof/interrupt.h>
#include <sof/schedule.h>
#include <platform/platform.h>
#include <arch/task.h>

static struct irq_task *task_get_irq_task(struct task *task)
{
	switch (task->priority) {
#ifdef CONFIG_TASK_HAVE_PRIORITY_MEDIUM
	case SOF_TASK_PRI_MED + 1 ... SOF_TASK_PRI_LOW:
		return *task_irq_low_get();
	case SOF_TASK_PRI_HIGH ... SOF_TASK_PRI_MED - 1:
		return *task_irq_high_get();
	case SOF_TASK_PRI_MED:
		return *task_irq_med_get();
#elif CONFIG_TASK_HAVE_PRIORITY_LOW
	case  SOF_TASK_PRI_MED ... SOF_TASK_PRI_LOW:
		return *task_irq_low_get();
	case SOF_TASK_PRI_HIGH ... SOF_TASK_PRI_MED - 1:
		return *task_irq_high_get();
#else
	case SOF_TASK_PRI_HIGH ... SOF_TASK_PRI_LOW:
		return *task_irq_high_get();
#endif
	default:
		trace_error(TRACE_CLASS_IRQ,
			    "task_get_irq_task() error: task priority %d",
			    task->priority);
	}

	return NULL;
}

/**
 * \brief Adds task to the list per IRQ level.
 * \param[in,out] task Task data.
 */
static int task_set_data(struct task *task)
{
	struct irq_task *irq_task = task_get_irq_task(task);
	struct list_item *dst;
	unsigned long flags;

	if (!irq_task)
		return -EINVAL;

	dst = &irq_task->list;
	spin_lock_irq(&irq_task->lock, flags);
	list_item_append(&task->irq_list, dst);
	spin_unlock_irq(&irq_task->lock, flags);

	return 0;
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
	int run_task = 0;

	spin_lock_irq(&irq_task->lock, flags);

	interrupt_clear(irq_task->irq);

	list_for_item_safe(clist, tlist, &irq_task->list) {
		task = container_of(clist, struct task, irq_list);
		list_item_del(clist);

		if (task->func &&
		    task->state == SOF_TASK_STATE_PENDING) {
			schedule_task_running(task);
			run_task = 1;
		} else {
			run_task = 0;
		}

		/* run task without holding task lock */
		spin_unlock_irq(&irq_task->lock, flags);

		if (run_task)
			task->func(task->data);

		spin_lock_irq(&irq_task->lock, flags);
		schedule_task_complete(task);
	}

	spin_unlock_irq(&irq_task->lock, flags);
}

int arch_run_task(struct task *task)
{
	struct irq_task *irq_task = task_get_irq_task(task);
	int ret;

	if (!irq_task)
		return -EINVAL;

	ret = task_set_data(task);

	if (ret < 0)
		return ret;

	interrupt_set(irq_task->irq);

	return 0;
}

int arch_allocate_tasks(void)
{
	int ret;

#ifdef CONFIG_TASK_HAVE_PRIORITY_LOW
	/* irq low */
	struct irq_task **low = task_irq_low_get();
	*low = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(**low));

	list_init(&((*low)->list));
	spinlock_init(&((*low)->lock));
	(*low)->irq = PLATFORM_IRQ_TASK_LOW;

	ret = interrupt_register((*low)->irq, IRQ_AUTO_UNMASK, _irq_task,
				 task_irq_low_get());
	if (ret < 0)
		return ret;
	interrupt_enable((*low)->irq);
#endif

#ifdef CONFIG_TASK_HAVE_PRIORITY_MEDIUM
	/* irq medium */
	struct irq_task **med = task_irq_med_get();
	*med = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(**med));

	list_init(&((*med)->list));
	spinlock_init(&((*med)->lock));
	(*med)->irq = PLATFORM_IRQ_TASK_MED;

	ret = interrupt_register((*med)->irq, IRQ_AUTO_UNMASK, _irq_task,
				 task_irq_med_get());
	if (ret < 0)
		return ret;
	interrupt_enable((*med)->irq);
#endif

	/* irq high */
	struct irq_task **high = task_irq_high_get();
	*high = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(**high));

	list_init(&((*high)->list));
	spinlock_init(&((*high)->lock));
	(*high)->irq = PLATFORM_IRQ_TASK_HIGH;

	ret = interrupt_register((*high)->irq, IRQ_AUTO_UNMASK, _irq_task,
				 task_irq_high_get());
	if (ret < 0)
		return ret;
	interrupt_enable((*high)->irq);

	return 0;
}

void arch_free_tasks(void)
{
	uint32_t flags;
/* TODO: do not want to free the tasks, just the entire heap */

#ifdef CONFIG_TASK_HAVE_PRIORITY_LOW
	/* free IRQ low task */
	struct irq_task **low = task_irq_low_get();

	spin_lock_irq(&(*low)->lock, flags);
	interrupt_disable((*low)->irq);
	interrupt_unregister((*low)->irq, task_irq_low_get());
	list_item_del(&(*low)->list);
	spin_unlock_irq(&(*low)->lock, flags);
#endif

#ifdef CONFIG_TASK_HAVE_PRIORITY_MEDIUM
	/* free IRQ medium task */
	struct irq_task **med = task_irq_med_get();

	spin_lock_irq(&(*med)->lock, flags);
	interrupt_disable((*med)->irq);
	interrupt_unregister((*med)->irq, task_irq_med_get());
	list_item_del(&(*med)->list);
	spin_unlock_irq(&(*med)->lock, flags);
#endif

	/* free IRQ high task */
	struct irq_task **high = task_irq_high_get();

	spin_lock_irq(&(*high)->lock, flags);
	interrupt_disable((*high)->irq);
	interrupt_unregister((*high)->irq, task_irq_high_get());
	list_item_del(&(*high)->list);
	spin_unlock_irq(&(*high)->lock, flags);
}
