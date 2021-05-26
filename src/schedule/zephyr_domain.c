// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019-2021 Intel Corporation. All rights reserved.
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

#define LL_TIMER_PERIOD 1000 /* period in microseconds */

struct zephyr_domain {
	struct k_work_q ll_workq[CONFIG_CORE_COUNT];
	int ll_workq_registered[CONFIG_CORE_COUNT];
	struct timer *timer;
};

struct zephyr_domain_work {
	struct k_work_delayable work;
	void (*handler)(void *arg);
	void *arg;
};

struct zephyr_domain_work zdata[CONFIG_CORE_COUNT];

static void zephyr_domain_report_delay(int id, uint64_t delay)
{
	uint32_t ll_delay_us = (delay * 1000) /
				clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1);

	/* Zephyr schedules on nearest kernel tick not HW cycle */
	if (delay <= CYC_PER_TICK)
		return;

	if (delay <= UINT_MAX)
		tr_err(&ll_tr, "zephyr_domain_report_delay(): timer %d delayed by %d uS %d ticks",
		       id, ll_delay_us, (unsigned int)delay);
	else
		tr_err(&ll_tr, "zephyr_domain_report_delay(): timer %d delayed by %d uS, ticks > %u",
		       id, ll_delay_us, UINT_MAX);

	/* Fix compile error when traces are disabled */
	(void)ll_delay_us;
}

static void zephyr_domain_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct zephyr_domain_work *zd = CONTAINER_OF(dwork, struct zephyr_domain_work, work);

	zd->handler(zd->arg);
}

static int zephyr_domain_register(struct ll_schedule_domain *domain,
				 uint64_t period, struct task *task,
				 void (*handler)(void *arg), void *arg)
{
	struct zephyr_domain *zephyr_domain = ll_sch_domain_get_pdata(domain);
	int core = cpu_get_id();
	int ret = 0;
	void *stack;
	char *qname;
	struct k_thread *thread;

	tr_dbg(&ll_tr, "zephyr_domain_register()");

	/* domain work only needs registered once */
	if (zephyr_domain->ll_workq_registered[core])
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
	k_work_queue_start(&zephyr_domain->ll_workq[core], stack,
			   ZEPHYR_LL_WORKQ_SIZE, -CONFIG_NUM_COOP_PRIORITIES, NULL);

	thread = &zephyr_domain->ll_workq[core].thread;

	k_thread_suspend(thread);

	k_thread_cpu_mask_clear(thread);
	k_thread_cpu_mask_enable(thread, core);
	k_thread_name_set(thread, qname);

	k_thread_resume(thread);

	zephyr_domain->ll_workq_registered[core] = 1;

	k_work_init_delayable(&zdata[core].work, zephyr_domain_handler);

	tr_info(&ll_tr, "zephyr_domain_register domain->type %d domain->clk %d domain->ticks_per_ms %d period %d",
		domain->type, domain->clk, domain->ticks_per_ms, (uint32_t)period);
out:

	return ret;
}

static int zephyr_domain_unregister(struct ll_schedule_domain *domain,
				   struct task *task, uint32_t num_tasks)
{
	int core = cpu_get_id();

	tr_dbg(&ll_tr, "zephyr_domain_unregister()");

	/* tasks still registered on this core */
	if (num_tasks)
		return 0;

	k_work_cancel_delayable(&zdata[core].work);

	tr_info(&ll_tr, "zephyr_domain_unregister domain->type %d domain->clk %d",
		domain->type, domain->clk);

	return 0;
}

static void zephyr_domain_set(struct ll_schedule_domain *domain, uint64_t start)
{
	struct zephyr_domain *zephyr_domain = ll_sch_domain_get_pdata(domain);
	uint64_t ticks_tout = domain->ticks_per_ms * LL_TIMER_PERIOD / 1000;
	uint64_t ticks_set;
	uint64_t current = platform_timer_get_atomic(zephyr_domain->timer);
	uint64_t earliest_next = current + 1 + ZEPHYR_SCHED_COST;
	uint64_t ticks_req = domain->next_tick ? start + ticks_tout :
		MAX(start, earliest_next);
	int ret, core = cpu_get_id();
	uint64_t ticks_delta;

	if (domain->next_tick <= current ||
	    domain->next_tick > current + ticks_tout ||
	    !domain->next_tick) {
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
	} else {
		/* The other core has already calculated the next timer event */
		ticks_req = domain->next_tick;
	}

	/* work out next start time relative to start */
	ticks_delta = ticks_req - current;

	/* This actually sets the timer and the next call only reads it back */
	/* using K_CYC(ticks_delta - 885) brings "requested - set" to about 180-700
	 * cycles, audio sounds very slow and distorted.
	 */
	ret = k_work_reschedule_for_queue(&zephyr_domain->ll_workq[core],
					  &zdata[core].work,
					  K_CYC(ticks_delta - ZEPHYR_SCHED_COST));
	if (ret < 0) {
		tr_err(&ll_tr, "queue submission error %d", ret);
		return;
	}

	ticks_set = k_work_delayable_remaining_get(&zdata[core].work) * CYC_PER_TICK +
		current - current % CYC_PER_TICK;

	tr_dbg(&ll_tr, "zephyr_domain_set(): ticks_set %u ticks_req %u current %u",
	       (unsigned int)ticks_set, (unsigned int)ticks_req,
	       (unsigned int)platform_timer_get_atomic(zephyr_domain->timer));

	/* Was timer set to the value we requested? If no it means some
	 * delay occurred and we should report that in error log.
	 */
	if (ticks_req < ticks_set)
		zephyr_domain_report_delay(zephyr_domain->timer->id,
					   ticks_set - ticks_req);

	domain->next_tick = ticks_set;

}

static bool zephyr_domain_is_pending(struct ll_schedule_domain *domain,
				    struct task *task, struct comp_dev **comp)
{
	return task->start <= platform_timer_get_atomic(timer_get());
}

static const struct ll_schedule_domain_ops zephyr_domain_ops = {
	.domain_register	= zephyr_domain_register,
	.domain_unregister	= zephyr_domain_unregister,
	.domain_set		= zephyr_domain_set,
	.domain_is_pending	= zephyr_domain_is_pending
};

struct ll_schedule_domain *timer_domain_init(struct timer *timer, int clk)
{
	struct ll_schedule_domain *domain;
	struct zephyr_domain *zephyr_domain;

	domain = domain_init(SOF_SCHEDULE_LL_TIMER, clk, false,
			     &zephyr_domain_ops);

	zephyr_domain = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0, SOF_MEM_CAPS_RAM,
				sizeof(*zephyr_domain));
	zephyr_domain->timer = timer;

	ll_sch_domain_set_pdata(domain, zephyr_domain);

	return domain;
}
