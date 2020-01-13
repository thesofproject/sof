// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/platform.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <ipc/topology.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct timer_domain {
	struct timer *timer;
	uint64_t timeout;
	void *arg[PLATFORM_CORE_COUNT];
};

const struct ll_schedule_domain_ops timer_domain_ops;

static inline void timer_report_delay(int id, uint64_t delay)
{
	uint32_t ll_delay_us = (delay * 1000) /
				clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1);

	trace_ll_error("timer_report_delay(): timer %d delayed by %d uS %d "
		       "ticks", id, ll_delay_us, delay);

	/* Fix compile error when traces are disabled */
	(void)ll_delay_us;
}

static int timer_domain_register(struct ll_schedule_domain *domain,
				 uint64_t period, struct task *task,
				 void (*handler)(void *arg), void *arg)
{
	struct timer_domain *timer_domain = ll_sch_domain_get_pdata(domain);
	int core = cpu_get_id();
	int ret = 0;

	tracev_ll("timer_domain_register()");

	/* tasks already registered on this core */
	if (timer_domain->arg[core])
		goto out;

	trace_ll("timer_domain_register domain->type %d domain->clk %d domain->ticks_per_ms %d period %d",
		 domain->type, domain->clk, domain->ticks_per_ms, period);

	timer_domain->arg[core] = arg;

	ret = timer_register(timer_domain->timer, handler, arg);

out:
	platform_shared_commit(timer_domain, sizeof(*timer_domain));

	return ret;
}

static void timer_domain_unregister(struct ll_schedule_domain *domain,
				    struct task *task, uint32_t num_tasks)
{
	struct timer_domain *timer_domain = ll_sch_domain_get_pdata(domain);
	int core = cpu_get_id();

	tracev_ll("timer_domain_unregister()");

	/* tasks still registered on this core */
	if (!timer_domain->arg[core] || num_tasks)
		goto out;

	trace_ll("timer_domain_unregister domain->type %d domain->clk %d",
		 domain->type, domain->clk);

	timer_unregister(timer_domain->timer, timer_domain->arg[core]);

	timer_domain->arg[core] = NULL;

out:
	platform_shared_commit(timer_domain, sizeof(*timer_domain));
}

static void timer_domain_enable(struct ll_schedule_domain *domain, int core)
{
	struct timer_domain *timer_domain = ll_sch_domain_get_pdata(domain);

	timer_enable(timer_domain->timer, timer_domain->arg[core], core);

	platform_shared_commit(timer_domain, sizeof(*timer_domain));
}

static void timer_domain_disable(struct ll_schedule_domain *domain, int core)
{
	struct timer_domain *timer_domain = ll_sch_domain_get_pdata(domain);

	timer_disable(timer_domain->timer, timer_domain->arg[core], core);

	platform_shared_commit(timer_domain, sizeof(*timer_domain));
}

static void timer_domain_set(struct ll_schedule_domain *domain, uint64_t start)
{
	struct timer_domain *timer_domain = ll_sch_domain_get_pdata(domain);
	uint64_t ticks_req = domain->ticks_per_ms * timer_domain->timeout /
			     1000 + start;
	uint64_t ticks_set;

	ticks_set = platform_timer_set(timer_domain->timer, ticks_req);

	/* Was timer set to the value we requested? If no it means some
	 * delay occurred and we should report that in error log.
	 */
	if (ticks_req < ticks_set)
		timer_report_delay(timer_domain->timer->id,
				   ticks_set - ticks_req);

	domain->last_tick = ticks_set;

	platform_shared_commit(timer_domain, sizeof(*timer_domain));
}

static void timer_domain_clear(struct ll_schedule_domain *domain)
{
	struct timer_domain *timer_domain = ll_sch_domain_get_pdata(domain);

	platform_timer_clear(timer_domain->timer);

	platform_shared_commit(timer_domain, sizeof(*timer_domain));
}

static bool timer_domain_is_pending(struct ll_schedule_domain *domain,
				    struct task *task)
{
	return task->start <= platform_timer_get(timer_get());
}

struct ll_schedule_domain *timer_domain_init(struct timer *timer, int clk,
					     uint64_t timeout)
{
	struct ll_schedule_domain *domain;
	struct timer_domain *timer_domain;

	trace_ll("timer_domain_init clk %d timeout %u", clk, timeout);

	domain = domain_init(SOF_SCHEDULE_LL_TIMER, clk, false,
			     &timer_domain_ops);

	timer_domain = rzalloc(SOF_MEM_ZONE_SYS, SOF_MEM_FLAG_SHARED,
			       SOF_MEM_CAPS_RAM, sizeof(*timer_domain));
	timer_domain->timer = timer;
	timer_domain->timeout = timeout;

	ll_sch_domain_set_pdata(domain, timer_domain);

	platform_shared_commit(domain, sizeof(*domain));
	platform_shared_commit(timer_domain, sizeof(*timer_domain));

	return domain;
}

const struct ll_schedule_domain_ops timer_domain_ops = {
	.domain_register	= timer_domain_register,
	.domain_unregister	= timer_domain_unregister,
	.domain_enable		= timer_domain_enable,
	.domain_disable		= timer_domain_disable,
	.domain_set		= timer_domain_set,
	.domain_clear		= timer_domain_clear,
	.domain_is_pending	= timer_domain_is_pending
};
