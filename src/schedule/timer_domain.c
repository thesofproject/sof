// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <rtos/timer.h>
#include <rtos/alloc.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <ipc/topology.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LL_TIMER_SET_OVERHEAD_TICKS  1000 /* overhead/delay to set the tick, in ticks */

struct timer_domain {
	struct timer *timer;
	void *arg[CONFIG_CORE_COUNT];
};

static void timer_report_delay(int id, uint64_t delay)
{
	uint32_t ll_delay_us = (delay * 1000) /
				clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1);

	if (delay <= UINT_MAX)
		tr_err(&ll_tr, "timer_report_delay(): timer %d delayed by %d uS %d ticks",
		       id, ll_delay_us, (unsigned int)delay);
	else
		tr_err(&ll_tr, "timer_report_delay(): timer %d delayed by %d uS, ticks > %u",
		       id, ll_delay_us, UINT_MAX);

	/* Fix compile error when traces are disabled */
	(void)ll_delay_us;
}

static int timer_domain_register(struct ll_schedule_domain *domain,
				 struct task *task,
				 void (*handler)(void *arg), void *arg)
{
	struct timer_domain *timer_domain = ll_sch_domain_get_pdata(domain);
	int core = cpu_get_id();

	tr_dbg(&ll_tr, "timer_domain_register()");

	/* tasks already registered on this core */
	if (timer_domain->arg[core])
		return 0;

	timer_domain->arg[core] = arg;

	tr_info(&ll_tr, "timer_domain_register domain->type %d domain->clk %d domain->ticks_per_ms %d",
		domain->type, domain->clk, domain->ticks_per_ms);

	return timer_register(timer_domain->timer, handler, arg);
}

static int timer_domain_unregister(struct ll_schedule_domain *domain,
				   struct task *task, uint32_t num_tasks)
{
	struct timer_domain *timer_domain = ll_sch_domain_get_pdata(domain);
	int core = cpu_get_id();

	if (task)
		return 0;

	tr_dbg(&ll_tr, "timer_domain_unregister()");

	/* tasks still registered on this core */
	if (!timer_domain->arg[core] || num_tasks)
		return 0;

	tr_info(&ll_tr, "timer_domain_unregister domain->type %d domain->clk %d",
		domain->type, domain->clk);

	timer_unregister(timer_domain->timer, timer_domain->arg[core]);

	timer_domain->arg[core] = NULL;

	return 0;
}

static void timer_domain_enable(struct ll_schedule_domain *domain, int core)
{
	struct timer_domain *timer_domain = ll_sch_domain_get_pdata(domain);

	timer_enable(timer_domain->timer, timer_domain->arg[core], core);
}

static void timer_domain_disable(struct ll_schedule_domain *domain, int core)
{
	struct timer_domain *timer_domain = ll_sch_domain_get_pdata(domain);

	timer_disable(timer_domain->timer, timer_domain->arg[core], core);
}

static void timer_domain_set(struct ll_schedule_domain *domain, uint64_t start)
{
	struct timer_domain *timer_domain = ll_sch_domain_get_pdata(domain);
	uint64_t ticks_set;
	/* make sure to require for ticks later than tout from now */
	const uint64_t time = platform_timer_get_atomic(timer_domain->timer);
	const uint64_t ticks_req = MAX(start, time + LL_TIMER_SET_OVERHEAD_TICKS);

	ticks_set = platform_timer_set(timer_domain->timer, ticks_req);

	tr_dbg(&ll_tr, "timer_domain_set(): ticks_set %u ticks_req %u current %u",
	       (unsigned int)ticks_set, (unsigned int)ticks_req,
	       (unsigned int)platform_timer_get_atomic(timer_get()));

	/* Was timer set to the value we requested? If no it means some
	 * delay occurred and we should report that in error log.
	 */
	if (ticks_req < ticks_set)
		timer_report_delay(timer_domain->timer->id,
				   ticks_set - ticks_req);

	domain->next_tick = ticks_set;

}

static void timer_domain_clear(struct ll_schedule_domain *domain)
{
	struct timer_domain *timer_domain = ll_sch_domain_get_pdata(domain);

	platform_timer_clear(timer_domain->timer);
}

static bool timer_domain_is_pending(struct ll_schedule_domain *domain,
				    struct task *task, struct comp_dev **comp)
{
	return task->start <= platform_timer_get_atomic(timer_get());
}

static const struct ll_schedule_domain_ops timer_domain_ops = {
	.domain_register	= timer_domain_register,
	.domain_unregister	= timer_domain_unregister,
	.domain_enable		= timer_domain_enable,
	.domain_disable		= timer_domain_disable,
	.domain_set		= timer_domain_set,
	.domain_clear		= timer_domain_clear,
	.domain_is_pending	= timer_domain_is_pending
};

struct ll_schedule_domain *timer_domain_init(struct timer *timer, int clk)
{
	struct ll_schedule_domain *domain;
	struct timer_domain *timer_domain;

	domain = domain_init(SOF_SCHEDULE_LL_TIMER, clk, false,
			     &timer_domain_ops);

	timer_domain = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*timer_domain));
	timer_domain->timer = timer;

	ll_sch_domain_set_pdata(domain, timer_domain);

	return domain;
}
