// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/bit.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/platform.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DMA_DOMAIN_OWNER_INVALID	0xFFFFFFFF

struct dma_domain_data {
	int irq;
	struct dma_chan_data *channel;
	void (*handler)(void *arg);
	void *arg;
};

struct dma_domain {
	struct dma *dma_array;	/* pointer to scheduling DMAs */
	uint32_t num_dma;	/* number of scheduling DMAs */
	uint32_t owner;		/* core owning the scheduling channel */
	bool channel_changed;	/* true if we needed to re-register */

	/* data per core */
	struct dma_domain_data data[PLATFORM_CORE_COUNT];
};

const struct ll_schedule_domain_ops dma_single_chan_domain_ops;

static void dma_single_chan_domain_enable(struct ll_schedule_domain *domain,
					  int core);
static void dma_domain_changed(void *arg, enum notify_id type, void *data);

/**
 * \brief Retrieves DMA channel with lowest period.
 * \param[in,out] dma_domain Pointer to DMA domain.
 * \return Pointer to DMA channel.
 */
static struct dma_chan_data *dma_chan_min_period(struct dma_domain *dma_domain)
{
	struct dma *dmas = dma_domain->dma_array;
	struct dma_chan_data *channel = NULL;
	int i;
	int j;

	/* get currently registered channel if exists */
	if (dma_domain->owner != DMA_DOMAIN_OWNER_INVALID &&
	    dma_domain->data[dma_domain->owner].channel)
		channel = dma_domain->data[dma_domain->owner].channel;

	for (i = 0; i < dma_domain->num_dma; ++i) {
		/* DMA not probed */
		if (!dmas[i].sref)
			continue;

		for (j = 0; j < dmas[i].plat_data.channels; ++j) {
			/* channel not set as scheduling source */
			if (!dma_is_scheduling_source(&dmas[i].chan[j]))
				continue;

			/* channel not running */
			if (dmas[i].chan[j].status != COMP_STATE_ACTIVE)
				continue;

			/* bigger period */
			if (channel &&
			    channel->period <= dmas[i].chan[j].period)
				continue;

			channel = &dmas[i].chan[j];
		}

		platform_shared_commit(dmas[i].chan, sizeof(*dmas[i].chan) *
				       dmas[i].plat_data.channels);
	}

	platform_shared_commit(dmas, sizeof(*dmas) * dma_domain->num_dma);

	return channel;
}

/**
 * \brief Sends notification event about scheduling DMA channel change.
 * \param[in,out] channel Pointer to new scheduling DMA channel.
 */
static void dma_domain_notify_change(struct dma_chan_data *channel)
{
	tr_info(&ll_tr, "dma_domain_notify_change()");

	notifier_event(channel, NOTIFIER_ID_DMA_DOMAIN_CHANGE,
		       NOTIFIER_TARGET_CORE_ALL_MASK & ~BIT(cpu_get_id()),
		       channel, sizeof(*channel));
}

/**
 * \brief Registers and enables selected DMA interrupt.
 * \param[in,out] channel Pointer to DMA channel.
 * \param[in,out] data Pointer to DMA domain data.
 * \param[in,out] handler Pointer to DMA interrupt handler.
 * \param[in,out] arg Pointer to DMA interrupt handler's argument.
 * \return Error code.
 */
static int dma_single_chan_domain_irq_register(struct dma_chan_data *channel,
					       struct dma_domain_data *data,
					       void (*handler)(void *arg),
					       void *arg)
{
	int irq = dma_chan_irq(channel->dma, channel->index);
	int ret;

	tr_info(&ll_tr, "dma_single_chan_domain_irq_register()");

	data->irq = interrupt_get_irq(irq, dma_irq_name(channel->dma));
	if (data->irq < 0) {
		ret = data->irq;
		goto out;
	}

	ret = interrupt_register(data->irq, handler, arg);
	if (ret < 0)
		goto out;

	interrupt_enable(data->irq, arg);

	interrupt_mask(data->irq, cpu_get_id());

	data->channel = channel;
	data->handler = handler;
	data->arg = arg;

out:
	platform_shared_commit(channel, sizeof(*channel));

	return ret;
}

/**
 * \brief Unregisters and disables selected DMA interrupt.
 * \param[in,out] data Pointer to DMA domain data.
 */
static void dma_single_chan_domain_irq_unregister(struct dma_domain_data *data)
{
	tr_info(&ll_tr, "dma_single_chan_domain_irq_unregister()");

	interrupt_disable(data->irq, data->arg);

	interrupt_unregister(data->irq, data->arg);
}

/**
 * \brief Registers task to DMA domain.
 * \param[in,out] domain Pointer to schedule domain.
 * \param[in] period Period of the scheduled task.
 * \param[in,out] task Task to be registered.
 * \param[in,out] handler Pointer to DMA interrupt handler.
 * \param[in,out] arg Pointer to DMA interrupt handler's argument.
 * \return Error code.
 *
 * Every core registers for the same DMA channel, but only the one on which
 * this channel is actually running is the owner. If there is already channel
 * running we still need to verify, if the recently started channel doesn't
 * have lower period. In such case change of channel is needed and all other
 * cores need to be notified.
 */
static int dma_single_chan_domain_register(struct ll_schedule_domain *domain,
					   uint64_t period, struct task *task,
					   void (*handler)(void *arg),
					   void *arg)
{
	struct dma_domain *dma_domain = ll_sch_domain_get_pdata(domain);
	struct pipeline_task *pipe_task = pipeline_task_get(task);
	int core = cpu_get_id();
	struct dma_domain_data *data = &dma_domain->data[core];
	struct dma_chan_data *channel;
	bool register_needed = true;
	int ret = 0;

	tr_info(&ll_tr, "dma_single_chan_domain_register()");

	/* check if task should be registered */
	if (!pipe_task->registrable)
		goto out;

	/* get running channel with min period */
	channel = dma_chan_min_period(dma_domain);
	if (!channel) {
		ret = -EINVAL;
		goto out;
	}

	if (data->channel) {
		/* channel with min period already registered */
		if (data->channel->period == channel->period)
			goto out;

		tr_info(&ll_tr, "dma_single_chan_domain_register(): lower period detected, registering again");

		/* unregister from current channel */
		dma_single_chan_domain_irq_unregister(data);
		dma_interrupt(data->channel, DMA_IRQ_MASK);
		dma_interrupt(data->channel, DMA_IRQ_CLEAR);

		dma_domain->channel_changed = true;

		/* already registered */
		register_needed = false;
	}

	tr_info(&ll_tr, "dma_single_chan_domain_register(): registering on channel with period %llu",
		channel->period);

	/* register for interrupt */
	ret = dma_single_chan_domain_irq_register(channel, data, handler, arg);
	if (ret < 0)
		goto out;

	/* enable channel interrupt */
	dma_interrupt(data->channel, DMA_IRQ_UNMASK);

	/* unmask if we are the owner */
	if (dma_domain->owner == core)
		interrupt_unmask(data->irq, core);

	/* notify scheduling channel change */
	if (dma_domain->owner != channel->core)
		dma_domain_notify_change(channel);

	/* register for source change notifications */
	if (register_needed)
		notifier_register(domain, NULL, NOTIFIER_ID_DMA_DOMAIN_CHANGE,
				  dma_domain_changed, 0);

	dma_domain->owner = channel->core;

out:
	platform_shared_commit(dma_domain, sizeof(*dma_domain));

	return ret;
}

/**
 * \brief Checks if any DMA channel is running on current core.
 * \param[in,out] dmas Array of possible DMAs.
 * \param[in] num Number of DMAs in the array.
 * \return True if any DMA channel is running, false otherwise.
 */
static bool dma_chan_is_any_running(struct dma *dmas, uint32_t num)
{
	int core = cpu_get_id();
	bool ret = false;
	int i;
	int j;

	for (i = 0; i < num; ++i) {
		/* DMA not probed */
		if (!dmas[i].sref)
			continue;

		for (j = 0; j < dmas[i].plat_data.channels; ++j) {
			/* channel not set as scheduling source */
			if (!dma_is_scheduling_source(&dmas[i].chan[j]))
				continue;

			/* channel owned by different core */
			if (core != dmas[i].chan[j].core)
				continue;

			if (dmas[i].chan[j].status == COMP_STATE_ACTIVE) {
				platform_shared_commit(dmas[i].chan,
						sizeof(*dmas[i].chan) *
						dmas[i].plat_data.channels);
				ret = true;
				goto out;
			}
		}

		platform_shared_commit(dmas[i].chan, sizeof(*dmas[i].chan) *
				       dmas[i].plat_data.channels);
	}

out:
	platform_shared_commit(dmas, sizeof(*dmas) * num);

	return ret;
}

/**
 * \brief Unregisters task from DMA domain if the current core is the owner.
 * \param[in,out] domain Pointer to schedule domain.
 * \param[in,out] data Pointer to DMA domain data.
 *
 * If the owner of scheduling DMA channel unregisters, it has to notify
 * other cores about the change.
 */
static void dma_domain_unregister_owner(struct ll_schedule_domain *domain,
					struct dma_domain_data *data)
{
	struct dma_domain *dma_domain = ll_sch_domain_get_pdata(domain);
	struct dma *dmas = dma_domain->dma_array;
	struct dma_chan_data *channel;
	int core = cpu_get_id();

	tr_info(&ll_tr, "dma_domain_unregister_owner()");

	/* transfers still scheduled on this channel */
	if (data->channel->status == COMP_STATE_ACTIVE)
		return;

	dma_single_chan_domain_irq_unregister(data);
	dma_interrupt(data->channel, DMA_IRQ_MASK);
	dma_interrupt(data->channel, DMA_IRQ_CLEAR);
	data->channel = NULL;

	channel = dma_chan_min_period(dma_domain);
	if (!channel) {
		dma_domain->owner = DMA_DOMAIN_OWNER_INVALID;
		notifier_unregister(domain, NULL,
				    NOTIFIER_ID_DMA_DOMAIN_CHANGE);

		return;
	}

	/* change owner */
	dma_domain->owner = channel->core;

	/* notify scheduling channel change */
	dma_domain_notify_change(channel);

	/* check if there is another channel running */
	if (dma_chan_is_any_running(dmas, dma_domain->num_dma)) {
		tr_info(&ll_tr, "dma_domain_unregister_owner(): some channel is still running, registering again");

		/* register again and enable */
		if (dma_single_chan_domain_irq_register(channel, data,
							data->handler,
							data->arg) < 0) {
			tr_err(&ll_tr, "dma_domain_unregister_owner(): couldn't register irq");
			return;
		}

		dma_interrupt(data->channel, DMA_IRQ_CLEAR);
		dma_single_chan_domain_enable(domain, core);
		dma_domain->channel_changed = true;
	}
}

/**
 * \brief Unregisters task from DMA domain.
 * \param[in,out] domain Pointer to schedule domain.
 * \param[in,out] task Task to be unregistered from the domain.
 * \param[in] num_tasks Number of currently scheduled tasks.
 */
static void dma_single_chan_domain_unregister(struct ll_schedule_domain *domain,
					      struct task *task,
					      uint32_t num_tasks)
{
	struct dma_domain *dma_domain = ll_sch_domain_get_pdata(domain);
	struct pipeline_task *pipe_task = pipeline_task_get(task);
	struct dma *dmas = dma_domain->dma_array;
	int core = cpu_get_id();
	struct dma_domain_data *data = &dma_domain->data[core];

	tr_info(&ll_tr, "dma_single_chan_domain_unregister()");

	/* check if task should be unregistered */
	if (!pipe_task->registrable)
		goto out;

	/* channel not registered */
	if (!data->channel)
		goto out;

	/* unregister domain owner */
	if (dma_domain->owner == core) {
		dma_domain_unregister_owner(domain, data);
		goto out;
	}

	/* some channel still running, so return */
	if (dma_chan_is_any_running(dmas, dma_domain->num_dma))
		goto out;

	/* no more transfers scheduled on this core */
	dma_single_chan_domain_irq_unregister(data);
	data->channel = NULL;

	notifier_unregister(domain, NULL, NOTIFIER_ID_DMA_DOMAIN_CHANGE);

out:
	platform_shared_commit(dma_domain, sizeof(*dma_domain));
}

/**
 * \brief Unmasks scheduling DMA channel's interrupt.
 * \param[in,out] domain Pointer to schedule domain.
 * \param[in] core Core on which interrupt should be unmasked.
 */
static void dma_single_chan_domain_enable(struct ll_schedule_domain *domain,
					  int core)
{
	struct dma_domain *dma_domain = ll_sch_domain_get_pdata(domain);
	struct dma_domain_data *data = &dma_domain->data[core];

	/* channel not registered */
	if (!data->channel)
		goto out;

	dma_interrupt(data->channel, DMA_IRQ_UNMASK);

	interrupt_unmask(data->irq, core);

out:
	platform_shared_commit(dma_domain, sizeof(*dma_domain));
}

/**
 * \brief Masks scheduling DMA channel's interrupt.
 * \param[in,out] domain Pointer to schedule domain.
 * \param[in] core Core on which interrupt should be masked.
 */
static void dma_single_chan_domain_disable(struct ll_schedule_domain *domain,
					   int core)
{
	struct dma_domain *dma_domain = ll_sch_domain_get_pdata(domain);
	struct dma_domain_data *data = &dma_domain->data[core];

	/* channel not registered */
	if (!data->channel)
		goto out;

	interrupt_mask(data->irq, core);

out:
	platform_shared_commit(dma_domain, sizeof(*dma_domain));
}

/**
 * \brief Calculates domain's last tick.
 * \param[in,out] domain Pointer to schedule domain.
 * \param[in] start Offset of last calculated tick.
 */
static void dma_single_chan_domain_set(struct ll_schedule_domain *domain,
				       uint64_t start)
{
	struct dma_domain *dma_domain = ll_sch_domain_get_pdata(domain);
	struct dma_domain_data *data = &dma_domain->data[cpu_get_id()];
	uint64_t ticks;

	/* channel not registered */
	if (!data->channel)
		goto out;

	if (dma_domain->channel_changed) {
		domain->last_tick = platform_timer_get(timer_get());

		dma_domain->channel_changed = false;
	} else {
		ticks = domain->ticks_per_ms * data->channel->period / 1000 +
			start;

		domain->last_tick = domain->last_tick ? ticks : start;
	}

out:
	platform_shared_commit(dma_domain, sizeof(*dma_domain));
}

/**
 * \brief Clears scheduling DMA channel's interrupt.
 * \param[in,out] domain Pointer to schedule domain.
 */
static void dma_single_chan_domain_clear(struct ll_schedule_domain *domain)
{
	struct dma_domain *dma_domain = ll_sch_domain_get_pdata(domain);
	struct dma_domain_data *data = &dma_domain->data[cpu_get_id()];

	/* channel not registered */
	if (!data->channel)
		goto out;

	dma_interrupt(data->channel, DMA_IRQ_CLEAR);

out:
	platform_shared_commit(dma_domain, sizeof(*dma_domain));
}

/**
 * \brief Checks if given task should be executed.
 * \param[in,out] domain Pointer to schedule domain.
 * \param[in,out] task Task to be checked.
 * \return True is task should be executed, false otherwise.
 */
static bool dma_single_chan_domain_is_pending(struct ll_schedule_domain *domain,
					      struct task *task)
{
	return task->start <= platform_timer_get(timer_get());
}

/**
 * \brief Scheduling DMA channel change notification handling.
 * \param[in,out] arg Pointer to self.
 * \param[in] type Id of the notification.
 * \param[in,out] data Pointer to notification event data.
 */
static void dma_domain_changed(void *arg, enum notify_id type, void *data)
{
	struct ll_schedule_domain *domain = arg;
	struct dma_domain *dma_domain = ll_sch_domain_get_pdata(domain);
	int core = cpu_get_id();
	struct dma_domain_data *domain_data = &dma_domain->data[core];

	tr_info(&ll_tr, "dma_domain_changed()");

	/* unregister from current DMA channel */
	dma_single_chan_domain_irq_unregister(domain_data);

	if (domain_data->channel->core == core) {
		dma_interrupt(domain_data->channel, DMA_IRQ_MASK);
		dma_interrupt(domain_data->channel, DMA_IRQ_CLEAR);
	}

	/* register to the new DMA channel */
	if (dma_single_chan_domain_irq_register(data, domain_data,
						domain_data->handler,
						domain_data->arg) < 0)
		goto out;

	dma_single_chan_domain_enable(domain, core);

out:
	platform_shared_commit(domain, sizeof(*domain));
	platform_shared_commit(dma_domain, sizeof(*dma_domain));
}

/**
 * \brief Initializes DMA single channel scheduling domain.
 * \param[in,out] dma_array Array of DMAs to be scheduled on.
 * \param[in] num_dma Number of DMAs passed as dma_array.
 * \param[in] clk Platform clock to base calculations on.
 * \return Pointer to initialized scheduling domain object.
 */
struct ll_schedule_domain *dma_single_chan_domain_init(struct dma *dma_array,
						       uint32_t num_dma,
						       int clk)
{
	struct ll_schedule_domain *domain;
	struct dma_domain *dma_domain;

	tr_info(&ll_tr, "dma_single_chan_domain_init(): num_dma %d, clk %d",
		num_dma, clk);

	domain = domain_init(SOF_SCHEDULE_LL_DMA, clk, false,
			     &dma_single_chan_domain_ops);

	dma_domain = rzalloc(SOF_MEM_ZONE_SYS, SOF_MEM_FLAG_SHARED,
			     SOF_MEM_CAPS_RAM, sizeof(*dma_domain));
	dma_domain->dma_array = dma_array;
	dma_domain->num_dma = num_dma;
	dma_domain->owner = DMA_DOMAIN_OWNER_INVALID;

	ll_sch_domain_set_pdata(domain, dma_domain);

	platform_shared_commit(domain, sizeof(*domain));
	platform_shared_commit(dma_domain, sizeof(*dma_domain));

	return domain;
}

const struct ll_schedule_domain_ops dma_single_chan_domain_ops = {
	.domain_register	= dma_single_chan_domain_register,
	.domain_unregister	= dma_single_chan_domain_unregister,
	.domain_enable		= dma_single_chan_domain_enable,
	.domain_disable		= dma_single_chan_domain_disable,
	.domain_set		= dma_single_chan_domain_set,
	.domain_clear		= dma_single_chan_domain_clear,
	.domain_is_pending	= dma_single_chan_domain_is_pending,
};
