// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
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

#ifdef __ZEPHYR__

#include <kernel.h>
#include <sys_clock.h>

/*
 * Currently the Zephyr clock rate is part it's Kconfig known at build time.
 * SOF on Intel CAVS platforms currently only aligns with Zephyr when both
 * use the CAVS 19.2 MHz SSP clock. TODO - needs runtime alignment.
 */
#if CONFIG_XTENSA && CONFIG_CAVS && !CONFIG_CAVS_TIMER
#error "Zephyr uses 19.2MHz clock derived from SSP which must be enabled."
#endif

#define ZEPHYR_LL_WORKQ_SIZE	8192

#define CYC_PER_TICK	(CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC	\
			/ CONFIG_SYS_CLOCK_TICKS_PER_SEC)

/* zephyr alignment to nearest kernel tick */
#define ZEPHYR_SCHED_COST	CYC_PER_TICK

K_THREAD_STACK_DEFINE(ll_workq_stack0, ZEPHYR_LL_WORKQ_SIZE);
#if CONFIG_CORE_COUNT > 1
K_THREAD_STACK_DEFINE(ll_workq_stack1, ZEPHYR_LL_WORKQ_SIZE);
#endif
#if CONFIG_CORE_COUNT > 2
K_THREAD_STACK_DEFINE(ll_workq_stack2, ZEPHYR_LL_WORKQ_SIZE);
#endif
#if CONFIG_CORE_COUNT > 3
K_THREAD_STACK_DEFINE(ll_workq_stack3, ZEPHYR_LL_WORKQ_SIZE);
#endif
#endif

#define LL_TIMER_SET_OVERHEAD_TICKS  1000 /* overhead/delay to set the tick, in ticks */

struct timer_domain {
#ifdef __ZEPHYR__
	struct k_work_q ll_workq[CONFIG_CORE_COUNT];
	int ll_workq_registered[CONFIG_CORE_COUNT];
#endif
	struct timer *timer;
	void *arg[CONFIG_CORE_COUNT];
	uint64_t timeout; /* in ticks */
};

#ifdef __ZEPHYR__
struct timer_zdata {
	struct k_delayed_work work;
	void (*handler)(void *arg);
	void *arg;
};

struct timer_zdata zdata[CONFIG_CORE_COUNT];
#endif

const struct ll_schedule_domain_ops timer_domain_ops;

static inline void timer_report_delay(int id, uint64_t delay)
{
	uint32_t ll_delay_us = (delay * 1000) /
				clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1);

#ifdef __ZEPHYR__
	/* Zephyr schedules on nearest kernel tick not HW cycle */
	if (delay <= CYC_PER_TICK)
		return;
#endif

	if (delay <= UINT_MAX)
		tr_err(&ll_tr, "timer_report_delay(): timer %d delayed by %d uS %d ticks",
		       id, ll_delay_us, (unsigned int)delay);
	else
		tr_err(&ll_tr, "timer_report_delay(): timer %d delayed by %d uS, ticks > %u",
		       id, ll_delay_us, UINT_MAX);

	/* Fix compile error when traces are disabled */
	(void)ll_delay_us;
}

#ifdef __ZEPHYR__
static void timer_z_handler(struct k_work *work)
{
	struct timer_zdata *zd = CONTAINER_OF(work, struct timer_zdata, work);

	zd->handler(zd->arg);
}
#endif

static int timer_domain_register(struct ll_schedule_domain *domain,
				 uint64_t period, struct task *task,
				 void (*handler)(void *arg), void *arg)
{
	struct timer_domain *timer_domain = ll_sch_domain_get_pdata(domain);
	int core = cpu_get_id();
	int ret = 0;
#ifdef __ZEPHYR__
	void *stack;
	char *qname;
	struct k_thread *thread;
#endif

	tr_dbg(&ll_tr, "timer_domain_register()");

#ifdef __ZEPHYR__

	/* domain work only needs registered once */
	if (timer_domain->ll_workq_registered[core])
		goto out;

	switch (core) {
	case 0:
		stack = ll_workq_stack0;
		qname = "ll_workq0";
		break;
#if CONFIG_CORE_COUNT > 1
	case 1:
		stack = ll_workq_stack1;
		qname = "ll_workq1";
		break;
#endif
#if CONFIG_CORE_COUNT > 2
	case 2:
		stack = ll_workq_stack2;
		qname = "ll_workq2";
		break;
#endif
#if CONFIG_CORE_COUNT > 3
	case 3:
		stack = ll_workq_stack3;
		qname = "ll_workq3";
		break;
#endif
	default:
		tr_err(&ll_tr, "invalid timer domain core %d\n", core);
		return -EINVAL;
	}

	zdata[core].handler = handler;
	zdata[core].arg = arg;
	k_work_q_start(&timer_domain->ll_workq[core], stack,
		       ZEPHYR_LL_WORKQ_SIZE, -CONFIG_NUM_COOP_PRIORITIES);

	thread = &timer_domain->ll_workq[core].thread;

	k_thread_suspend(thread);

	k_thread_cpu_mask_clear(thread);
	k_thread_cpu_mask_enable(thread, core);
	k_thread_name_set(thread, qname);

	k_thread_resume(thread);

	timer_domain->ll_workq_registered[core] = 1;

	k_delayed_work_init(&zdata[core].work, timer_z_handler);

#else
	/* tasks already registered on this core */
	if (timer_domain->arg[core])
		goto out;

	timer_domain->arg[core] = arg;
	ret = timer_register(timer_domain->timer, handler, arg);
#endif

	tr_info(&ll_tr, "timer_domain_register domain->type %d domain->clk %d domain->ticks_per_ms %d period %d",
		domain->type, domain->clk, domain->ticks_per_ms, (uint32_t)period);
out:

	return ret;
}

static void timer_domain_unregister(struct ll_schedule_domain *domain,
				    struct task *task, uint32_t num_tasks)
{
	struct timer_domain *timer_domain = ll_sch_domain_get_pdata(domain);
	int core = cpu_get_id();

	tr_dbg(&ll_tr, "timer_domain_unregister()");

	/* tasks still registered on this core */
	if (!timer_domain->arg[core] || num_tasks)
		return;

	tr_info(&ll_tr, "timer_domain_unregister domain->type %d domain->clk %d",
		domain->type, domain->clk);
#ifndef __ZEPHYR__
	timer_unregister(timer_domain->timer, timer_domain->arg[core]);
#endif
	timer_domain->arg[core] = NULL;
}

static void timer_domain_enable(struct ll_schedule_domain *domain, int core)
{
#ifndef __ZEPHYR__
	struct timer_domain *timer_domain = ll_sch_domain_get_pdata(domain);

	timer_enable(timer_domain->timer, timer_domain->arg[core], core);

#endif
}

static void timer_domain_disable(struct ll_schedule_domain *domain, int core)
{
#ifndef __ZEPHYR__
	struct timer_domain *timer_domain = ll_sch_domain_get_pdata(domain);

	timer_disable(timer_domain->timer, timer_domain->arg[core], core);

#endif
}

static void timer_domain_set(struct ll_schedule_domain *domain, uint64_t start)
{
	struct timer_domain *timer_domain = ll_sch_domain_get_pdata(domain);
	uint64_t ticks_tout = timer_domain->timeout;
	uint64_t ticks_set;
#ifndef __ZEPHYR__
	/* make sure to require for ticks later than tout from now */
	const uint64_t time = platform_timer_get_atomic(timer_domain->timer);
	const uint64_t ticks_req = MAX(start, time + ticks_tout);

	ticks_set = platform_timer_set(timer_domain->timer, ticks_req);
#else
	uint64_t current = platform_timer_get_atomic(timer_domain->timer);
	uint64_t earliest_next = current + 1 + ZEPHYR_SCHED_COST;
	uint64_t ticks_req = domain->next_tick ? start + ticks_tout :
		MAX(start, earliest_next);
	int ret, core = cpu_get_id();
	uint64_t ticks_delta;

	if (ticks_tout < CYC_PER_TICK)
		ticks_tout = CYC_PER_TICK;

	/* have we overshot the period length ?? */
	if (ticks_req > current + ticks_tout)
		ticks_req = current + ticks_tout;

	ticks_req -= ticks_req % CYC_PER_TICK;
	if (ticks_req < earliest_next) {
		/* The earliest schedule point has to be rounded up */
		ticks_req = earliest_next + CYC_PER_TICK - 1;
		ticks_req -= ticks_req % CYC_PER_TICK;
	}

	/* work out next start time relative to start */
	ticks_delta = ticks_req - current;

	/* This actually sets the timer and the next call only reads it back */
	/* using K_CYC(ticks_delta - 885) brings "requested - set" to about 180-700
	 * cycles, audio sounds very slow and distorted.
	 */
	ret = k_delayed_work_submit_to_queue(&timer_domain[core].ll_workq[core],
					     &zdata[core].work,
					     K_CYC(ticks_delta - ZEPHYR_SCHED_COST));
	if (ret < 0) {
		tr_err(&ll_tr, "queue submission error %d", ret);
		return;
	}

	ticks_set = k_delayed_work_remaining_ticks(&zdata[core].work) * CYC_PER_TICK +
		current - current % CYC_PER_TICK;
#endif

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
#ifndef __ZEPHYR__
	struct timer_domain *timer_domain = ll_sch_domain_get_pdata(domain);

	platform_timer_clear(timer_domain->timer);

#endif
}

static bool timer_domain_is_pending(struct ll_schedule_domain *domain,
				    struct task *task, struct comp_dev **comp)
{
	return task->start <= platform_timer_get_atomic(timer_get());
}

struct ll_schedule_domain *timer_domain_init(struct timer *timer, int clk)
{
	struct ll_schedule_domain *domain;
	struct timer_domain *timer_domain;

	domain = domain_init(SOF_SCHEDULE_LL_TIMER, clk, false,
			     &timer_domain_ops);

	timer_domain = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*timer_domain));
	timer_domain->timer = timer;
	timer_domain->timeout = LL_TIMER_SET_OVERHEAD_TICKS;

	ll_sch_domain_set_pdata(domain, timer_domain);

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
