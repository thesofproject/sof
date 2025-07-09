// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <sof/audio/component.h>
#include <rtos/bit.h>
#include <rtos/interrupt.h>
#include <rtos/timer.h>
#include <rtos/alloc.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
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

/* For i.MX, when building SOF with Zephyr, we use wrapper.c,
 * interrupt.c and interrupt-irqsteer.c which causes name
 * collisions.
 * In order to avoid this and make any second level interrupt
 * handling go through interrupt-irqsteer.c define macros to
 * rename the duplicated functions.
 */
#if defined(__ZEPHYR__) && (defined(CONFIG_IMX) || defined(CONFIG_AMD))
#define interrupt_get_irq mux_interrupt_get_irq
#define interrupt_register mux_interrupt_register
#define interrupt_unregister mux_interrupt_unregister
#define interrupt_enable mux_interrupt_enable
#define interrupt_disable mux_interrupt_disable
#endif

struct dma_domain_data {
	int irq;
	struct pipeline_task *task;
	void (*handler)(void *arg);
	void *arg;
};

struct dma_domain {
	struct dma *dma_array;	/* pointer to scheduling DMAs */
	uint32_t num_dma;	/* number of scheduling DMAs */
	bool aggregated_irq;	/* true if aggregated interrupts */

	/* mask of currently running channels */
	uint32_t channel_mask[PLATFORM_NUM_DMACS][CONFIG_CORE_COUNT];
	/* array of arguments for aggregated mode */
	struct dma_domain_data *arg[PLATFORM_NUM_DMACS][CONFIG_CORE_COUNT];
	/* array of registered channels data */
	struct dma_domain_data data[PLATFORM_NUM_DMACS][PLATFORM_MAX_DMA_CHAN];
};

const struct ll_schedule_domain_ops dma_multi_chan_domain_ops;

/**
 * \brief Generic DMA interrupt handler.
 * \param[in,out] data Pointer to DMA domain data.
 */
static void dma_multi_chan_domain_irq_handler(void *data)
{
	struct dma_domain_data *domain_data = data;

	/* just call registered handler */
	domain_data->handler(domain_data->arg);

}

/**
 * \brief Registers and enables selected DMA interrupt.
 * \param[in,out] data Pointer to DMA domain data.
 * \param[in,out] handler Pointer to DMA interrupt handler.
 * \return Error code.
 */
static int dma_multi_chan_domain_irq_register(struct dma_domain_data *data,
					      void (*handler)(void *arg))
{
	int ret;

	tr_info(&ll_tr, "dma_multi_chan_domain_irq_register()");

	/* always go through dma_multi_chan_domain_irq_handler,
	 * so we have different arg registered for every channel
	 */
	ret = interrupt_register(data->irq, dma_multi_chan_domain_irq_handler,
				 data);
	if (ret < 0)
		return ret;

	interrupt_enable(data->irq, data);

	return 0;
}

/**
 * \brief Registers task to DMA domain.
 *
 * \param[in,out] domain Pointer to schedule domain.
 * \param[in,out] task Task to be registered.
 * \param[in,out] handler Pointer to DMA interrupt handler.
 * \param[in,out] arg Pointer to DMA interrupt handler's argument.
 * \return Error code.
 *
 * Keeps track of potential double registrations and handles non aggregated
 * DMA interrupts (different irq number per DMA channel).
 */
static int dma_multi_chan_domain_register(struct ll_schedule_domain *domain,
					  struct task *task,
					  void (*handler)(void *arg), void *arg)
{
	struct dma_domain *dma_domain = ll_sch_domain_get_pdata(domain);
	struct pipeline_task *pipe_task = pipeline_task_get(task);
	struct dma *dmas = dma_domain->dma_array;
	int core = cpu_get_id();
	int ret = 0;
	int i;
	int j;

	tr_info(&ll_tr, "dma_multi_chan_domain_register()");

	/* check if task should be registered */
	if (!pipe_task->registrable)
		goto out;

	for (i = 0; i < dma_domain->num_dma ; ++i) {
		if (dmas[i].chan == NULL)
			continue;
		for (j = 0; j < dmas[i].plat_data.channels ; ++j) {
			/* channel not set as scheduling source */
			if (!dma_is_scheduling_source(&dmas[i].chan[j]))
				continue;

			/* channel not running */
			if (dmas[i].chan[j].status != COMP_STATE_ACTIVE)
				continue;

			/* channel owned by different core */
			if (core != dmas[i].chan[j].core)
				continue;

			/* channel has been already running */
			if (dma_domain->channel_mask[i][core] & BIT(j))
				continue;

			dma_interrupt_legacy(&dmas[i].chan[j], DMA_IRQ_CLEAR);

			/* register only if not aggregated or not registered */
			if (!dma_domain->aggregated_irq ||
			    !dma_domain->channel_mask[i][core]) {
				ret = dma_multi_chan_domain_irq_register(
						&dma_domain->data[i][j],
						handler);
				if (ret < 0)
					goto out;

				dma_domain->data[i][j].handler = handler;
				dma_domain->data[i][j].arg = arg;

				/* needed to unregister aggregated interrupts */
				dma_domain->arg[i][core] =
					&dma_domain->data[i][j];
			}

			interrupt_clear_mask(dma_domain->data[i][j].irq,
					     BIT(j));

			dma_interrupt_legacy(&dmas[i].chan[j], DMA_IRQ_UNMASK);

			dma_domain->data[i][j].task = pipe_task;
			dma_domain->channel_mask[i][core] |= BIT(j);

			goto out;
		}
	}

out:
	return ret;
}

/**
 * \brief Unregisters and disables selected DMA interrupt.
 * \param[in,out] data data Pointer to DMA domain data.
 */
static void dma_multi_chan_domain_irq_unregister(struct dma_domain_data *data)
{
	tr_info(&ll_tr, "dma_multi_chan_domain_irq_unregister()");

	interrupt_disable(data->irq, data);

	interrupt_unregister(data->irq, data);
}

/**
 * \brief Unregisters task from DMA domain.
 * \param[in,out] domain Pointer to schedule domain.
 * \param[in,out] task Task to be unregistered from the domain..
 * \param[in] num_tasks Number of currently scheduled tasks.
 * \return Error code.
 */
static int dma_multi_chan_domain_unregister(struct ll_schedule_domain *domain,
					    struct task *task,
					    uint32_t num_tasks)
{
	struct dma_domain *dma_domain = ll_sch_domain_get_pdata(domain);
	struct pipeline_task *pipe_task = pipeline_task_get(task);
	struct dma *dmas = dma_domain->dma_array;
	int core = cpu_get_id();
	int i;
	int j;

	tr_info(&ll_tr, "dma_multi_chan_domain_unregister()");

	/* check if task should be unregistered */
	if (!task || !pipe_task->registrable)
		return 0;

	for (i = 0; i < dma_domain->num_dma; ++i) {
		if (dmas[i].chan == NULL)
			continue;
		for (j = 0; j < dmas[i].plat_data.channels ; ++j) {
			/* channel not set as scheduling source */
			if (!dma_is_scheduling_source(&dmas[i].chan[j]))
				continue;

			/* channel still running */
			if (dmas[i].chan[j].status == COMP_STATE_ACTIVE)
				continue;

			/* channel owned by different core */
			if (core != dmas[i].chan[j].core)
				continue;

			/* channel hasn't been running */
			if (!(dma_domain->channel_mask[i][core] & BIT(j)))
				continue;

			dma_interrupt_legacy(&dmas[i].chan[j], DMA_IRQ_MASK);
			dma_interrupt_legacy(&dmas[i].chan[j], DMA_IRQ_CLEAR);
			interrupt_clear_mask(dma_domain->data[i][j].irq,
					     BIT(j));

			dma_domain->data[i][j].task = NULL;
			dma_domain->channel_mask[i][core] &= ~BIT(j);

			/* unregister interrupt */
			if (!dma_domain->aggregated_irq)
				dma_multi_chan_domain_irq_unregister(
						&dma_domain->data[i][j]);
			else if (!dma_domain->channel_mask[i][core])
				dma_multi_chan_domain_irq_unregister(
						dma_domain->arg[i][core]);
			return 0;
		}
	}

	/* task in running or unregistered at all, can't unregister it */
	return -EINVAL;
}

/**
 * \brief Checks if given task should be executed.
 * \param[in,out] domain Pointer to schedule domain.
 * \param[in,out] task Task to be checked.
 * \return True is task should be executed, false otherwise.
 */
static bool dma_multi_chan_domain_is_pending(struct ll_schedule_domain *domain,
					     struct task *task, struct comp_dev **comp)
{
	struct dma_domain *dma_domain = ll_sch_domain_get_pdata(domain);
	struct pipeline_task *pipe_task = pipeline_task_get(task);
	struct dma *dmas = dma_domain->dma_array;
	struct ll_task_pdata *pdata;
	uint32_t status;
	int i;
	int j;

	for (i = 0; i < dma_domain->num_dma; ++i) {
		if (dmas[i].chan == NULL)
			continue;
		for (j = 0; j < dmas[i].plat_data.channels; ++j) {
			if (!*comp) {
				status = dma_interrupt_legacy(&dmas[i].chan[j],
							      DMA_IRQ_STATUS_GET);
				if (!status)
					continue;

				*comp = dma_domain->data[i][j].task->sched_comp;
			} else if (!dma_domain->data[i][j].task ||
				   dma_domain->data[i][j].task->sched_comp != *comp) {
				continue;
			}

			/* not the same scheduling component */
			if (dma_domain->data[i][j].task->sched_comp !=
			    pipe_task->sched_comp)
				continue;

			/* Schedule task based on the frequency they
			 * were configured with, not time (task.start)
			 *
			 * There are cases when a DMA transfer from a DAI
			 * is finished earlier than task.start and,
			 * without full_sync mode, this task will not
			 * be scheduled
			 */
			if (domain->full_sync) {
				pdata = ll_sch_get_pdata(&pipe_task->task);
				pdata->skip_cnt++;
				if (pdata->skip_cnt == pdata->ratio)
					pdata->skip_cnt = 0;

				if (pdata->skip_cnt != 0)
					continue;
			} else {
				/* it's too soon for this task */
				if (!pipe_task->registrable &&
				    pipe_task->task.start >
				    sof_cycle_get_64_atomic())
					continue;
			}

			notifier_event(&dmas[i].chan[j], NOTIFIER_ID_DMA_IRQ,
				       NOTIFIER_TARGET_CORE_LOCAL,
				       &dmas[i].chan[j],
				       sizeof(struct dma_chan_data));

			/* clear interrupt */
			if (pipe_task->registrable) {
				dma_interrupt_legacy(&dmas[i].chan[j], DMA_IRQ_CLEAR);
				interrupt_clear_mask(dma_domain->data[i][j].irq,
						     BIT(j));
			}

			return true;
		}
	}

	return false;
}

/**
 * \brief Initializes DMA multichannel scheduling domain.
 * \param[in,out] dma_array Array of DMAs to be scheduled on.
 * \param[in] num_dma Number of DMAs passed as dma_array.
 * \param[in] clk Platform clock to base calculations on.
 * \param[in] aggregated_irq True if all DMAs share the same interrupt line.
 * \return Pointer to initialized scheduling domain object.
 */
struct ll_schedule_domain *dma_multi_chan_domain_init(struct dma *dma_array,
						      uint32_t num_dma, int clk,
						      bool aggregated_irq)
{
	struct ll_schedule_domain *domain;
	struct dma_domain *dma_domain;
	struct dma *dma;
	int i;
	int j;

	tr_info(&ll_tr, "dma_multi_chan_domain_init(): num_dma %d, clk %d, aggregated_irq %d",
		num_dma, clk, aggregated_irq);

	domain = domain_init(SOF_SCHEDULE_LL_DMA, clk, true,
			     &dma_multi_chan_domain_ops);
	if (!domain) {
		tr_err(&ll_tr, "dma_multi_chan_domain_init(): domain init failed");
		return NULL;
	}

	dma_domain = rzalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT, sizeof(*dma_domain));
	if (!dma_domain) {
		tr_err(&ll_tr, "dma_multi_chan_domain_init(): allocation failed");
		rfree(domain);
		return NULL;
	}
	dma_domain->dma_array = dma_array;
	dma_domain->num_dma = num_dma;
	dma_domain->aggregated_irq = aggregated_irq;

	/* retrieve IRQ numbers for each DMA channel */
	for (i = 0; i < num_dma; ++i) {
		dma = &dma_array[i];
		for (j = 0; j < dma->plat_data.channels; ++j)
			dma_domain->data[i][j].irq = interrupt_get_irq(
				dma_chan_irq(dma, j),
				dma_chan_irq_name(dma, j));
	}

	ll_sch_domain_set_pdata(domain, dma_domain);

	return domain;
}

const struct ll_schedule_domain_ops dma_multi_chan_domain_ops = {
	.domain_register	= dma_multi_chan_domain_register,
	.domain_unregister	= dma_multi_chan_domain_unregister,
	.domain_is_pending	= dma_multi_chan_domain_is_pending,
	.domain_set		= NULL,
	.domain_enable		= NULL,
	.domain_disable		= NULL,
	.domain_clear		= NULL,
};
