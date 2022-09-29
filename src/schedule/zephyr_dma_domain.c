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
#include <sof/schedule/task.h>
#include <ipc/topology.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_DECLARE(ll_schedule, CONFIG_SOF_LOG_LEVEL);

#ifdef CONFIG_IMX
#define interrupt_get_irq mux_interrupt_get_irq
#define interrupt_register mux_interrupt_register
#define interrupt_unregister mux_interrupt_unregister
#define interrupt_enable mux_interrupt_enable
#define interrupt_disable mux_interrupt_disable
#endif

#define SEM_LIMIT 1
#define ZEPHYR_PDOMAIN_STACK_SIZE 8192

K_KERNEL_STACK_ARRAY_DEFINE(zephyr_dma_domain_stack,
			    CONFIG_CORE_COUNT,
			    ZEPHYR_PDOMAIN_STACK_SIZE);

struct zephyr_dma_domain_data {
	int irq; /* IRQ associated with channel */
	struct dma_chan_data *channel; /* channel data */
	bool enabled_irq; /* true if DMA IRQ was already enabled */
	struct zephyr_dma_domain_thread *dt;
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

	struct zephyr_dma_domain_data data[PLATFORM_NUM_DMACS][PLATFORM_MAX_DMA_CHAN];
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
					  uint32_t num_tasks);

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
	struct zephyr_dma_domain_data *zephyr_dma_domain_data;
	struct zephyr_dma_domain_thread *dt;
	struct dma_chan_data *channel;
	struct k_sem *sem;
	int channel_index, irq;

	zephyr_dma_domain_data = data;
	channel = zephyr_dma_domain_data->channel;
	channel_index = channel->index;
	sem = &zephyr_dma_domain_data->dt->sem;
	irq = zephyr_dma_domain_data->irq;
	dt = zephyr_dma_domain_data->dt;

	/* clear IRQ */
	dma_interrupt_legacy(channel, DMA_IRQ_CLEAR);
	interrupt_clear_mask(irq, BIT(channel_index));

	/* give resources to thread semaphore */
	if (dt->handler)
		k_sem_give(sem);
}

static int enable_dma_irq(struct zephyr_dma_domain_data *data)
{
	int ret;

	dma_interrupt_legacy(data->channel, DMA_IRQ_CLEAR);

	ret = interrupt_register(data->irq, dma_irq_handler, data);
	if (ret < 0)
		return ret;

	interrupt_enable(data->irq, data);

	interrupt_clear_mask(data->irq, BIT(data->channel->index));

	dma_interrupt_legacy(data->channel, DMA_IRQ_UNMASK);

	return 0;
}

static int register_dma_irq(struct zephyr_dma_domain *domain,
			    struct zephyr_dma_domain_data **data,
			    struct zephyr_dma_domain_thread *dt,
			    struct pipeline_task *pipe_task,
			    int core)
{
	struct dma *crt_dma;
	struct dma_chan_data *crt_chan;
	struct zephyr_dma_domain_data *crt_data;
	int i, j, irq, ret;

	/* register the DMA IRQ only for PPL tasks marked as "registrable"
	 *
	 * this is done because, in case of mixer topologies there's
	 * multiple PPLs having the same scheduling component so there's
	 * no need to go through this function for all of those PPL
	 * tasks - only the PPL task containing the scheduling component
	 * will do the registering
	 *
	 */
	if (!pipe_task->registrable)
		return 0;

	/* iterate through all available channels in order to find a
	 * suitable channel for which the DMA IRQs will be enabled.
	 */
	for (i = 0; i < domain->num_dma; i++) {
		crt_dma = domain->dma_array + i;

		for (j = 0; j < crt_dma->plat_data.channels; j++) {
			crt_chan = crt_dma->chan + j;
			crt_data = &domain->data[i][j];

			/* skip if channel is not a scheduling source */
			if (!dma_is_scheduling_source(crt_chan))
				continue;

			/* skip if channel is not active */
			if (crt_chan->status != COMP_STATE_ACTIVE)
				continue;

			/* skip if channel not owned by current core */
			if (core != crt_chan->core)
				continue;

			/* skip if IRQ was already registered */
			if (crt_data->enabled_irq)
				continue;

			/* get IRQ number for current channel */
			irq = interrupt_get_irq(dma_chan_irq(crt_dma, j),
						dma_chan_irq_name(crt_dma, j));

			crt_data->irq = irq;
			crt_data->channel = crt_chan;
			crt_data->dt = dt;

			if (dt->started) {
				/* if the Zephyr thread was started, we
				 * can safely enable the DMA IRQs
				 */
				ret = enable_dma_irq(crt_data);
				if (ret < 0)
					return ret;

				crt_data->enabled_irq = true;
			}

			/* let caller know we have found a channel */
			*data = crt_data;

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
	struct zephyr_dma_domain_data *data;
	k_tid_t thread;
	int core, ret;
	char thread_name[] = "dma_domain_thread0";

	zephyr_dma_domain = ll_sch_get_pdata(domain);
	core = cpu_get_id();
	data = NULL;
	dt = zephyr_dma_domain->domain_thread + core;
	pipe_task = pipeline_task_get(task);

	tr_info(&ll_tr, "zephyr_dma_domain_register()");

	/* the DMA IRQ has to be registered before the Zephyr thread is
	 * started.
	 *
	 * this is done because we can have multiple DMA IRQs giving
	 * resources to the Zephyr thread semaphore on the same core
	 */
	ret = register_dma_irq(zephyr_dma_domain,
			       &data,
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
	k_sem_init(&dt->sem, 0, SEM_LIMIT);

	thread_name[sizeof(thread_name) - 2] = '0' + core;

	/* create Zephyr thread */
	thread = k_thread_create(&dt->ll_thread,
				 zephyr_dma_domain_stack[core],
				 ZEPHYR_PDOMAIN_STACK_SIZE,
				 zephyr_dma_domain_thread_fn,
				 dt,
				 NULL,
				 NULL,
				 CONFIG_NUM_PREEMPT_PRIORITIES - 1,
				 0,
				 K_FOREVER);

	k_thread_cpu_mask_clear(thread);
	k_thread_cpu_mask_enable(thread, core);
	k_thread_name_set(thread, thread_name);

	k_thread_start(thread);

	dt->started = true;

	/* TODO: maybe remove the second condition */
	if (data && !data->enabled_irq) {
		/* enable the DMA IRQ since register_dma_irq wasn't able
		 * to do so because of the fact that the Zephyr thread
		 * hadn't started at that point
		 */
		ret = enable_dma_irq(data);
		if (ret < 0) {
			tr_err(&ll_tr,
			       "failed to enable DMA IRQ for pipe task %p on core %d",
			       pipe_task,
			       core);
			return ret;
		}

		data->enabled_irq = true;

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

static void disable_dma_irq(struct zephyr_dma_domain_data *data)
{
	dma_interrupt_legacy(data->channel, DMA_IRQ_MASK);
	dma_interrupt_legacy(data->channel, DMA_IRQ_CLEAR);
	interrupt_clear_mask(data->irq, BIT(data->channel->index));
	interrupt_disable(data->irq, data);
	interrupt_unregister(data->irq, data);
}

static int unregister_dma_irq(struct zephyr_dma_domain *domain,
			      struct pipeline_task *pipe_task,
			      int core)
{
	struct zephyr_dma_domain_data *crt_data;
	struct dma *crt_dma;
	int i, j;
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

	for (i = 0; i < domain->num_dma; i++) {
		crt_dma = domain->dma_array + i;

		for (j = 0; j < crt_dma->plat_data.channels; j++) {
			crt_data = &domain->data[i][j];

			/* skip if DMA IRQ wasn't enabled */
			if (!crt_data->enabled_irq)
				continue;

			/* skip if channel not owned by current core */
			if (crt_data->channel->core != core)
				continue;

			/* skip if channel is still active */
			if (crt_data->channel->status == COMP_STATE_ACTIVE)
				continue;

			disable_dma_irq(crt_data);

			crt_data->enabled_irq = false;

			tr_info(&ll_tr, "unregister_dma_irq(): unregistered IRQ for task %p",
				pipe_task);

			return 0;
		}
	}

	/* if this point is reached then something went wrong */
	return -EINVAL;
}

static int zephyr_dma_domain_unregister(struct ll_schedule_domain *domain,
					struct task *task,
					uint32_t num_tasks)
{
	struct zephyr_dma_domain *zephyr_dma_domain;
	struct zephyr_dma_domain_thread *dt;
	struct pipeline_task *pipe_task;
	int ret, core;

	zephyr_dma_domain = ll_sch_get_pdata(domain);
	pipe_task = pipeline_task_get(task);
	core = cpu_get_id();
	dt = zephyr_dma_domain->domain_thread + core;

	tr_info(&ll_tr, "zephyr_dma_domain_unregister()");

	ret = unregister_dma_irq(zephyr_dma_domain, pipe_task, core);
	if (ret < 0) {
		tr_err(&ll_tr, "failed to unregister DMA IRQ for pipe task %p on core %d",
		       pipe_task,
		       core);
		return ret;
	}

	/* TODO: thread abortion logic goes here */

	return 0;
}

static void zephyr_dma_domain_task_cancel(struct ll_schedule_domain *domain,
					  uint32_t num_tasks)
{
	struct zephyr_dma_domain *zephyr_dma_domain;
	struct zephyr_dma_domain_thread *dt;
	int core;

	zephyr_dma_domain = ll_sch_get_pdata(domain);
	core = cpu_get_id();
	dt = zephyr_dma_domain->domain_thread + core;

	if (!num_tasks) {
		/* DMA IRQs got cut off, we need to let the Zephyr
		 * thread execute the handler one more time so as to be
		 * able to remove the task from the task queue
		 */
		k_sem_give(&dt->sem);
	}
}
