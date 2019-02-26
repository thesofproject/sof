// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/**
 * \file arch/xtensa/task.c
 * \brief Arch task implementation file
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <sof/drivers/interrupt.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cpu.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <config.h>
#include <xtos-structs.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

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

#if CONFIG_TASK_HAVE_PRIORITY_LOW
	/* irq low */
	struct irq_task **low = task_irq_low_get();
	*low = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(**low));

	list_init(&((*low)->list));
	spinlock_init(&((*low)->lock));
	(*low)->irq = interrupt_get_irq(PLATFORM_IRQ_TASK_LOW,
					PLATFORM_IRQ_TASK_LOW_NAME);
	if ((*low)->irq < 0)
		return (*low)->irq;

	ret = interrupt_register((*low)->irq, IRQ_AUTO_UNMASK, _irq_task, low);
	if (ret < 0)
		return ret;
	interrupt_enable((*low)->irq, low);
#endif

#if CONFIG_TASK_HAVE_PRIORITY_MEDIUM
	/* irq medium */
	struct irq_task **med = task_irq_med_get();
	*med = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(**med));

	list_init(&((*med)->list));
	spinlock_init(&((*med)->lock));
	(*med)->irq = interrupt_get_irq(PLATFORM_IRQ_TASK_MED,
					PLATFORM_IRQ_TASK_MED_NAME);
	if ((*med)->irq < 0)
		return (*med)->irq;

	ret = interrupt_register((*med)->irq, IRQ_AUTO_UNMASK, _irq_task, med);
	if (ret < 0)
		return ret;
	interrupt_enable((*med)->irq, med);
#endif

	/* irq high */
	struct irq_task **high = task_irq_high_get();
	*high = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(**high));

	list_init(&((*high)->list));
	spinlock_init(&((*high)->lock));
	(*high)->irq = interrupt_get_irq(PLATFORM_IRQ_TASK_HIGH,
					 PLATFORM_IRQ_TASK_HIGH_NAME);
	if ((*high)->irq < 0)
		return (*high)->irq;

	ret = interrupt_register((*high)->irq, IRQ_AUTO_UNMASK, _irq_task,
				 high);
	if (ret < 0)
		return ret;
	interrupt_enable((*high)->irq, high);

	return 0;
}

void arch_free_tasks(void)
{
	uint32_t flags;
/* TODO: do not want to free the tasks, just the entire heap */

#if CONFIG_TASK_HAVE_PRIORITY_LOW
	/* free IRQ low task */
	struct irq_task **low = task_irq_low_get();

	spin_lock_irq(&(*low)->lock, flags);
	interrupt_disable((*low)->irq, low);
	interrupt_unregister((*low)->irq, low);
	list_item_del(&(*low)->list);
	spin_unlock_irq(&(*low)->lock, flags);
#endif

#if CONFIG_TASK_HAVE_PRIORITY_MEDIUM
	/* free IRQ medium task */
	struct irq_task **med = task_irq_med_get();

	spin_lock_irq(&(*med)->lock, flags);
	interrupt_disable((*med)->irq, med);
	interrupt_unregister((*med)->irq, med);
	list_item_del(&(*med)->list);
	spin_unlock_irq(&(*med)->lock, flags);
#endif

	/* free IRQ high task */
	struct irq_task **high = task_irq_high_get();

	spin_lock_irq(&(*high)->lock, flags);
	interrupt_disable((*high)->irq, high);
	interrupt_unregister((*high)->irq, high);
	list_item_del(&(*high)->list);
	spin_unlock_irq(&(*high)->lock, flags);
}
