/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __SOF_SCHEDULE_LL_SCHEDULE_DOMAIN_H__
#define __SOF_SCHEDULE_LL_SCHEDULE_DOMAIN_H__

#include <sof/atomic.h>
#include <sof/debug/panic.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cpu.h>
#include <sof/lib/clk.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <stdbool.h>
#include <stdint.h>

struct dma;
struct ll_schedule_domain;
struct task;
struct timer;

struct ll_schedule_domain_ops {
	int (*domain_register)(struct ll_schedule_domain *domain,
			       uint64_t period, struct task *task,
			       void (*handler)(void *arg), void *arg);
	void (*domain_unregister)(struct ll_schedule_domain *domain,
				  uint32_t num_tasks);
	void (*domain_enable)(struct ll_schedule_domain *domain, int core);
	void (*domain_disable)(struct ll_schedule_domain *domain, int core);
	void (*domain_set)(struct ll_schedule_domain *domain, uint64_t start);
	void (*domain_clear)(struct ll_schedule_domain *domain);
	bool (*domain_is_pending)(struct ll_schedule_domain *domain,
				  struct task *task);
};

struct ll_schedule_domain {
	uint64_t last_tick;
	spinlock_t *lock;
	atomic_t total_num_tasks;
	atomic_t num_clients;
	bool registered[PLATFORM_CORE_COUNT];
	uint32_t ticks_per_ms;
	int type;
	int clk;
	const struct ll_schedule_domain_ops *ops;
	void *private;
};

#define ll_sch_domain_set_pdata(domain, data) ((domain)->private = (data))

#define ll_sch_domain_get_pdata(domain) ((domain)->private)

static inline struct ll_schedule_domain *domain_init(int type, int clk,
				const struct ll_schedule_domain_ops *ops)
{
	struct ll_schedule_domain *domain;

	domain = rzalloc(RZONE_SYS | RZONE_FLAG_UNCACHED, SOF_MEM_CAPS_RAM,
			 sizeof(*domain));
	domain->type = type;
	domain->clk = clk;
	domain->ticks_per_ms = clock_ms_to_ticks(clk, 1);
	domain->ops = ops;

	spinlock_init(&domain->lock);
	atomic_init(&domain->total_num_tasks, 0);
	atomic_init(&domain->num_clients, 0);

	return domain;
}

static inline int domain_register(struct ll_schedule_domain *domain,
				  uint64_t period, struct task *task,
				  void (*handler)(void *arg), void *arg)
{
	assert(domain->ops->domain_register);

	return domain->ops->domain_register(domain, period, task, handler, arg);
}

static inline void domain_unregister(struct ll_schedule_domain *domain,
				     uint32_t num_tasks)
{
	assert(domain->ops->domain_unregister);

	domain->ops->domain_unregister(domain, num_tasks);
}

static inline void domain_enable(struct ll_schedule_domain *domain, int core)
{
	if (domain->ops->domain_enable)
		domain->ops->domain_enable(domain, core);
}

static inline void domain_disable(struct ll_schedule_domain *domain, int core)
{
	if (domain->ops->domain_disable)
		domain->ops->domain_disable(domain, core);
}

static inline void domain_set(struct ll_schedule_domain *domain, uint64_t start)
{
	if (domain->ops->domain_set)
		domain->ops->domain_set(domain, start);
}

static inline void domain_clear(struct ll_schedule_domain *domain)
{
	if (domain->ops->domain_clear)
		domain->ops->domain_clear(domain);
}

static inline bool domain_is_pending(struct ll_schedule_domain *domain,
				     struct task *task)
{
	assert(domain->ops->domain_is_pending);

	return domain->ops->domain_is_pending(domain, task);
}

struct ll_schedule_domain *timer_domain_init(struct timer *timer, int clk,
					     uint64_t timeout);

struct ll_schedule_domain *dma_multi_chan_domain_init(struct dma *dma_array,
						      uint32_t num_dma, int clk,
						      bool aggregated_irq);

struct ll_schedule_domain *dma_single_chan_domain_init(struct dma *dma_array,
						       uint32_t num_dma,
						       int clk);

#endif /* __SOF_SCHEDULE_LL_SCHEDULE_DOMAIN_H__ */
