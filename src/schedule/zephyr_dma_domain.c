// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2022 NXP
//
// Author: Paul Olaru <paul.olaru@nxp.com>
// Author: Laurentiu Mihalcea <laurentiu.mihalcea@nxp.com>

#include <sof/audio/component.h>
#include <rtos/bit.h>
#include <rtos/interrupt.h>
#include <rtos/timer.h>
#include <rtos/alloc.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dma.h>
#include <sof/platform.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/schedule.h>
#include <rtos/task.h>
#include <ipc/topology.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_DECLARE(ll_schedule, CONFIG_SOF_LOG_LEVEL);

#if (defined(CONFIG_IMX) || defined(CONFIG_AMD))
#define interrupt_get_irq mux_interrupt_get_irq
#define interrupt_register mux_interrupt_register
#define interrupt_unregister mux_interrupt_unregister
#define interrupt_enable mux_interrupt_enable
#define interrupt_disable mux_interrupt_disable
#endif

#ifdef CONFIG_ARM64
#define interrupt_clear_mask(irq, bit)
#endif /* CONFIG_ARM64 */

#define ZEPHYR_PDOMAIN_STACK_SIZE 8192

/* sanity check - make sure CONFIG_DMA_DOMAIN_SEM_LIMIT is not some
 * garbage value.
 */
BUILD_ASSERT(CONFIG_DMA_DOMAIN_SEM_LIMIT > 0, "Invalid DMA domain SEM_LIMIT");

K_KERNEL_STACK_ARRAY_DEFINE(zephyr_dma_domain_stack,
			    CONFIG_CORE_COUNT,
			    ZEPHYR_PDOMAIN_STACK_SIZE);

struct zephyr_dma_domain_channel {
	struct dma_chan_data *channel;
	/* used when unregistering a pipeline task - the channel
	 * which we're disabling is the one that has been tied to the
	 * passed pipeline task.
	 */
	struct pipeline_task *pipe_task;
	/* pointer to parent struct zephyr_dma_domain_irq.
	 *
	 * mostly used during unregister operation to avoid
	 * having to look for a channel's IRQ parent after it
	 * has been fetched.
	 */
	struct zephyr_dma_domain_irq *irq_data;
	/* used to keep track of channels using the same INTID */
	struct list_item list;
};

struct zephyr_dma_domain_irq {
	uint32_t intid; /* IRQ number */
	bool enabled; /* true if IRQ has been enabled */
	struct zephyr_dma_domain_thread *dt;
	struct list_item list; /* used to keep track of all IRQs */
	struct list_item channels; /* list of channels using this IRQ */
};

struct zephyr_dma_domain_thread {
	struct k_thread ll_thread; /* thread handle */
	struct k_sem sem; /* used to signal when work should be done */
	void (*handler)(void *arg); /* work to be done */
	void *arg; /* data used by work function */
	bool started; /* true if the thread was started */
};

struct zephyr_dma_domain {
	struct dma *dma_array; /* array of scheduling DMAs */
	uint32_t num_dma; /* number of scheduling DMAs */

	struct list_item irqs; /* list of all IRQs used by the DMACs */

	/* array of Zephyr threads - one for each core */
	struct zephyr_dma_domain_thread domain_thread[CONFIG_CORE_COUNT];
};

static int zephyr_dma_domain_register(struct ll_schedule_domain *domain,
				      struct task *task,
				      void (*handler)(void *arg),
				      void *arg);
static int zephyr_dma_domain_unregister(struct ll_schedule_domain *domain,
					struct task *task,
					uint32_t num_tasks);
static void zephyr_dma_domain_task_cancel(struct ll_schedule_domain *domain,
					  struct task *task);

static const struct ll_schedule_domain_ops zephyr_dma_domain_ops = {
	.domain_register	= zephyr_dma_domain_register,
	.domain_unregister	= zephyr_dma_domain_unregister,
	.domain_task_cancel	= zephyr_dma_domain_task_cancel
};

struct ll_schedule_domain *zephyr_dma_domain_init(struct dma *dma_array,
						  uint32_t num_dma,
						  int clk)
{
	struct ll_schedule_domain *domain;
	struct zephyr_dma_domain *zephyr_dma_domain;

	/* initialize domain */
	domain = domain_init(SOF_SCHEDULE_LL_DMA,
			     clk,
			     true,
			     &zephyr_dma_domain_ops);

	/* initialize domain pdata */
	zephyr_dma_domain = rzalloc(SOF_MEM_ZONE_SYS_SHARED,
				    0,
				    SOF_MEM_CAPS_RAM,
				    sizeof(*zephyr_dma_domain));

	zephyr_dma_domain->dma_array = dma_array;
	zephyr_dma_domain->num_dma = num_dma;

	list_init(&zephyr_dma_domain->irqs);

	/* set pdata */
	ll_sch_domain_set_pdata(domain, zephyr_dma_domain);

	return domain;
}

static void zephyr_dma_domain_thread_fn(void *p1, void *p2, void *p3)
{
	struct zephyr_dma_domain_thread *dt = p1;

	while (true) {
		/* wait for DMA IRQ */
		k_sem_take(&dt->sem, K_FOREVER);

		/* do work */
		dt->handler(dt->arg);
	}
}

static void dma_irq_handler(void *data)
{
	struct zephyr_dma_domain_irq *irq_data;
	struct zephyr_dma_domain_thread *dt;
	struct k_sem *sem;
	struct list_item *i;
	struct zephyr_dma_domain_channel *chan_data;

	irq_data = data;
	sem = &irq_data->dt->sem;
	dt = irq_data->dt;

	/* go through each channel using the INTID which corresponds to the IRQ
	 * that has been triggered. For each channel, we clear the IRQ bit, thus
	 * stopping them from asserting the IRQ.
	 */
	list_for_item(i, &irq_data->channels) {
		chan_data = container_of(i, struct zephyr_dma_domain_channel, list);

		if (dma_interrupt_legacy(chan_data->channel, DMA_IRQ_STATUS_GET))
			dma_interrupt_legacy(chan_data->channel, DMA_IRQ_CLEAR);
	}

	/* clear IRQ - the mask argument is unused ATM */
	interrupt_clear_mask(irq_data->intid, 0);

	/* give resources to thread semaphore */
	if (dt->handler)
		k_sem_give(sem);
}

static int enable_dma_irq(struct zephyr_dma_domain_irq *irq_data)
{
	struct zephyr_dma_domain_channel *chan_data;
	int ret;

	/* ATM, it's impossible to have two channels added to the IRQ list
	 * without calling enable_dma_irq. Therefore, the channel for which we
	 * need to UNMASK/CLEAR the IRQ is the last one added to the channel
	 * list.
	 */
	chan_data = container_of(irq_data->channels.prev,
					struct zephyr_dma_domain_channel, list);

	dma_interrupt_legacy(chan_data->channel, DMA_IRQ_CLEAR);

	/* only enable the interrupt once. Also register the ISR and data once.
	 *
	 * note: irq_data->enabled is never set to false but since the irq_data is
	 * free'd and allocated each time with rzalloc it will be set to false by
	 * default.
	 */
	if (!irq_data->enabled) {
		/* the mask argument is unused ATM */
		interrupt_clear_mask(irq_data->intid, 0);

		ret = interrupt_register(irq_data->intid, dma_irq_handler, irq_data);
		if (ret < 0)
			return ret;

		interrupt_enable(irq_data->intid, irq_data);

		irq_data->enabled = true;
	}

	dma_interrupt_legacy(chan_data->channel, DMA_IRQ_UNMASK);

	return 0;
}

static struct zephyr_dma_domain_irq *fetch_irq_data(struct zephyr_dma_domain *domain,
						uint32_t intid)
{
	struct zephyr_dma_domain_irq *crt;
	struct list_item *i;

	/* go through the list of IRQs used by the DMACs and find the one
	 * using the same INTID as the one passed as argument to this function.
	 */
	list_for_item(i, &domain->irqs) {
		crt = container_of(i, struct zephyr_dma_domain_irq, list);

		if (crt->intid == intid)
			return crt;
	}

	return NULL;
}

bool chan_is_registered(struct zephyr_dma_domain *domain, struct dma_chan_data *chan)
{
	struct zephyr_dma_domain_irq *irq_data;
	struct zephyr_dma_domain_channel *chan_data;
	struct list_item *i, *j;

	list_for_item(i, &domain->irqs) {
		irq_data = container_of(i, struct zephyr_dma_domain_irq, list);

		list_for_item(j, &irq_data->channels) {
			chan_data = container_of(j, struct zephyr_dma_domain_channel, list);

			if (chan_data->channel == chan)
				return true;
		}
	}

	return false;
}

static int register_dma_irq(struct zephyr_dma_domain *domain,
			    struct zephyr_dma_domain_irq **irq_data,
			    struct zephyr_dma_domain_thread *dt,
			    struct pipeline_task *pipe_task,
			    int core)
{
	struct dma *crt_dma;
	struct dma_chan_data *crt_chan;
	struct zephyr_dma_domain_irq *crt_irq_data;
	struct zephyr_dma_domain_channel *chan_data;
	int i, j, irq, ret;
	uint32_t flags;

	/* iterate through all available channels in order to find a
	 * suitable channel for which the DMA IRQs will be enabled.
	 */
	for (i = 0; i < domain->num_dma; i++) {
		crt_dma = domain->dma_array + i;

		for (j = 0; j < crt_dma->plat_data.channels; j++) {
			crt_chan = crt_dma->chan + j;

			/* skip if channel is not a scheduling source */
			if (!dma_is_scheduling_source(crt_chan))
				continue;

			/* skip if channel is not active */
			if (crt_chan->status != COMP_STATE_ACTIVE)
				continue;

			/* skip if channel not owned by current core */
			if (core != crt_chan->core)
				continue;

			/* skip is DMA chan is already registered with domain */
			if (chan_is_registered(domain, crt_chan))
				continue;

			/* get IRQ number for current channel */
			irq = interrupt_get_irq(dma_chan_irq(crt_dma, j),
						dma_chan_irq_name(crt_dma, j));

			crt_irq_data = fetch_irq_data(domain, irq);

			if (!crt_irq_data) {
				/* if there's no list item matching given IRQ then
				 * that IRQ hasn't been allocated yet so we need to do
				 * so here.
				 */
				crt_irq_data = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0,
						SOF_MEM_CAPS_RAM, sizeof(*crt_irq_data));
				if (!crt_irq_data)
					return -ENOMEM;

				list_init(&crt_irq_data->channels);

				crt_irq_data->intid = irq;
				crt_irq_data->dt = dt;

				list_item_append(&crt_irq_data->list, &domain->irqs);
			}

			chan_data = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0,
				SOF_MEM_CAPS_RAM, sizeof(*chan_data));
			if (!chan_data)
				return -ENOMEM;

			irq_local_disable(flags);

			chan_data->channel = crt_chan;
			/* bind registrable ptask to channel */
			chan_data->pipe_task = pipe_task;
			chan_data->irq_data = crt_irq_data;

			list_item_append(&chan_data->list, &crt_irq_data->channels);

			if (dt->started) {
				/* the IRQ should only be enabled after the DT has
				 * been started to avoid missing some interrupts.
				 */
				ret = enable_dma_irq(crt_irq_data);
				if (ret < 0) {
					irq_local_enable(flags);
					return ret;
				}
			}

			/* let caller know we have found a channel */
			*irq_data = crt_irq_data;

			irq_local_enable(flags);

			return 0;
		}
	}

	/* if this point is reached then that means we weren't able to
	 * find a suitable channel, let caller know
	 */
	return -EINVAL;
}

static int zephyr_dma_domain_register(struct ll_schedule_domain *domain,
				      struct task *task,
				      void (*handler)(void *arg),
				      void *arg)
{
	struct zephyr_dma_domain *zephyr_dma_domain;
	struct zephyr_dma_domain_thread *dt;
	struct pipeline_task *pipe_task;
	struct zephyr_dma_domain_irq *irq_data;
	k_tid_t thread;
	int core, ret;
	uint32_t flags;
	char thread_name[] = "dma_domain_thread0";

	zephyr_dma_domain = ll_sch_get_pdata(domain);
	core = cpu_get_id();
	irq_data = NULL;
	dt = zephyr_dma_domain->domain_thread + core;
	pipe_task = pipeline_task_get(task);

	tr_info(&ll_tr, "zephyr_dma_domain_register()");

	/* don't even bother trying to register DMA IRQ for
	 * non-registrable tasks.
	 *
	 * this is needed because zephyr_dma_domain_register() will
	 * return -EINVAL for non-registrable tasks because of
	 * register_dma_irq() which is not right.
	 */
	if (!pipe_task->registrable)
		return 0;

	/* the DMA IRQ has to be registered before the Zephyr thread is
	 * started.
	 *
	 * this is done because we can have multiple DMA IRQs giving
	 * resources to the Zephyr thread semaphore on the same core
	 */
	ret = register_dma_irq(zephyr_dma_domain,
			       &irq_data,
			       dt,
			       pipe_task,
			       core);

	if (ret < 0) {
		tr_err(&ll_tr,
		       "failed to register DMA IRQ for pipe task %p on core %d",
		       pipe_task, core);

		return ret;
	}

	/* skip if Zephyr thread was already started on this core */
	if (dt->handler)
		return 0;

	dt->handler = handler;
	dt->arg = arg;

	/* prepare work semaphore */
	k_sem_init(&dt->sem, 0, CONFIG_DMA_DOMAIN_SEM_LIMIT);

	thread_name[sizeof(thread_name) - 2] = '0' + core;

	/* create Zephyr thread */
	/* VERY IMPORTANT: DMA domain's priority needs to be
	 * in the cooperative range to avoid scenarios such
	 * as the following:
	 *
	 *	1) pipeline_copy() is in the middle of a pipeline
	 *	graph traversal marking buffer->walking as true.
	 *	2) IPC TRIGGER STOP comes and since edf thread
	 *	has a higher priority it will preempt the DMA domain
	 *	thread.
	 *	3) When TRIGGER STOP handler does a pipeline graph
	 *	traversal it will find some buffers with walking = true
	 *	and not go through all the components in the pipeline.
	 *	4) TRIGGER RESET comes and the components are not
	 *	stopped so the handler will try to stop them which
	 *	results in DMA IRQs being stopped and the pipeline tasks
	 *	being stuck in the scheduling queue.
	 */
	thread = k_thread_create(&dt->ll_thread,
				 zephyr_dma_domain_stack[core],
				 ZEPHYR_PDOMAIN_STACK_SIZE,
				 zephyr_dma_domain_thread_fn,
				 dt,
				 NULL,
				 NULL,
				 -CONFIG_NUM_COOP_PRIORITIES,
				 0,
				 K_FOREVER);

	k_thread_cpu_mask_clear(thread);
	k_thread_cpu_mask_enable(thread, core);
	k_thread_name_set(thread, thread_name);

	k_thread_start(thread);

	dt->started = true;

	if (irq_data && !irq_data->enabled) {
		/* enable the DMA IRQ since register_dma_irq wasn't able
		 * to do so because of the fact that the Zephyr thread
		 * hadn't started at that point
		 */
		irq_local_disable(flags);
		ret = enable_dma_irq(irq_data);
		irq_local_enable(flags);
		if (ret < 0) {
			tr_err(&ll_tr,
			       "failed to enable DMA IRQ for pipe task %p on core %d",
			       pipe_task,
			       core);
			return ret;
		}

		return 0;
	}

	/* if this point is reached then that means we weren't able to
	 * enable DMA IRQ either because data was NULL or the IRQ was
	 * already enabled even though the Zephyr thread wasn't started
	 */

	tr_err(&ll_tr, "failed to register pipeline task %p on core %d",
	       pipe_task,
	       core);

	return -EINVAL;
}

static struct zephyr_dma_domain_channel *fetch_channel_by_ptask(struct zephyr_dma_domain *domain,
								struct pipeline_task *pipe_task)
{
	struct zephyr_dma_domain_irq *irq_data;
	struct zephyr_dma_domain_channel *chan_data;
	struct list_item *i, *j;

	/* go through all of the channel and find the one tied to the
	 * same pipeline task as the one passed as argument to this function.
	 */
	list_for_item(i, &domain->irqs) {
		irq_data = container_of(i, struct zephyr_dma_domain_irq, list);

		list_for_item(j, &irq_data->channels) {
			chan_data = container_of(j, struct zephyr_dma_domain_channel, list);

			if (chan_data->pipe_task == pipe_task)
				return chan_data;
		}
	}

	return NULL;
}

static void disable_dma_irq(struct zephyr_dma_domain_channel *chan_data)
{
	dma_interrupt_legacy(chan_data->channel, DMA_IRQ_MASK);
	dma_interrupt_legacy(chan_data->channel, DMA_IRQ_CLEAR);

	/* the IRQ needs to be disable only when there's no more
	 * channels using it (a.k.a the list of channels is empty)
	 */
	if (list_is_empty(&chan_data->irq_data->channels)) {
		/* the mask argument is unusued ATM */
		interrupt_clear_mask(chan_data->irq_data->intid, 0);

		interrupt_disable(chan_data->irq_data->intid, chan_data->irq_data);
		interrupt_unregister(chan_data->irq_data->intid, chan_data->irq_data);

		list_item_del(&chan_data->irq_data->list);

		rfree(chan_data->irq_data);
	}
}

static int zephyr_dma_domain_unregister(struct ll_schedule_domain *domain,
					struct task *task,
					uint32_t num_tasks)
{
	struct zephyr_dma_domain *zephyr_dma_domain;
	struct zephyr_dma_domain_thread *dt;
	struct pipeline_task *pipe_task;
	struct zephyr_dma_domain_channel *chan_data;
	int core;
	uint32_t flags;

	zephyr_dma_domain = ll_sch_get_pdata(domain);
	pipe_task = pipeline_task_get(task);
	core = cpu_get_id();
	dt = zephyr_dma_domain->domain_thread + core;

	tr_info(&ll_tr, "zephyr_dma_domain_unregister()");

	/* unregister the DMA IRQ only for PPL tasks marked as "registrable"
	 *
	 * this is done because, in case of mixer topologies there's
	 * multiple PPLs having the same scheduling component so there's
	 * no need to go through this function for all of those PPL
	 * tasks - only the PPL task containing the scheduling component
	 * will do the unregistering
	 *
	 */
	if (!pipe_task->registrable)
		return 0;

	irq_local_disable(flags);

	chan_data = fetch_channel_by_ptask(zephyr_dma_domain, pipe_task);
	if (!chan_data) {
		irq_local_enable(flags);
		tr_err(&ll_tr, "pipeline task %p doesn't have an associated channel.", pipe_task);
		return -EINVAL;
	}

	if (chan_data->channel->status == COMP_STATE_ACTIVE) {
		tr_warn(&ll_tr, "trying to unregister ptask %p while channel still active.",
			pipe_task);
	}

	/* remove channel from parent IRQ's list */
	list_item_del(&chan_data->list);

	/* disable DMA IRQ if need be */
	disable_dma_irq(chan_data);

	rfree(chan_data);

	irq_local_enable(flags);

	/* TODO: thread abortion logic goes here */

	return 0;
}

static void zephyr_dma_domain_task_cancel(struct ll_schedule_domain *domain,
					  struct task *task)
{
	struct zephyr_dma_domain *zephyr_dma_domain;
	struct zephyr_dma_domain_thread *dt;
	struct pipeline_task *pipe_task;
	int core;

	zephyr_dma_domain = ll_sch_get_pdata(domain);
	core = cpu_get_id();
	dt = zephyr_dma_domain->domain_thread + core;
	pipe_task = pipeline_task_get(task);

	if (pipe_task->sched_comp->state != COMP_STATE_ACTIVE) {
		/* If the state of the scheduling component
		 * corresponding to a pipeline task is !=
		 * COMP_STATE_ACTIVE then that means the DMA IRQs are
		 * disabled. Because of this, when a task is cancelled
		 * we need to give resources to the semaphore to make
		 * sure that zephyr_ll_run() is still executed and the
		 * tasks can be safely cancelled.
		 *
		 * This works because the state of the scheduling
		 * component is updated before the trigger operation.
		 */
		k_sem_give(&dt->sem);
	}
}
