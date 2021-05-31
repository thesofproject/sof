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

#define LL_TIMER_PERIOD_US 1000 /* period in microseconds */

K_KERNEL_STACK_ARRAY_DEFINE(ll_sched_stack, CONFIG_CORE_COUNT, ZEPHYR_LL_WORKQ_SIZE);

struct zephyr_domain_thread {
	struct k_thread ll_thread;
	void (*handler)(void *arg);
	void *arg;
	uint64_t period_us;
};

struct zephyr_domain {
	struct k_timer timer;
	struct timer *ll_timer;
	struct zephyr_domain_thread domain_thread[CONFIG_CORE_COUNT];
	struct ll_schedule_domain *ll_domain;
};

static void zephyr_domain_thread_fn(void *p1, void *p2, void *p3)
{
	struct zephyr_domain *zephyr_domain = p1;
	int core = cpu_get_id();
	struct zephyr_domain_thread *dt = zephyr_domain->domain_thread + core;

	for (;;) {
		/* immediately go to sleep, waiting to be woken up by the timer */
		k_thread_suspend(_current);

		dt->handler(dt->arg);
	}
}

static void zephyr_domain_timer_fn(struct k_timer *timer)
{
	struct zephyr_domain *zephyr_domain = timer->user_data;
	int core;

	if (!zephyr_domain)
		return;

	for (core = 0; core < CONFIG_CORE_COUNT; core++)
		if (zephyr_domain->domain_thread[core].handler)
			k_wakeup(&zephyr_domain->domain_thread[core].ll_thread);
}

static int zephyr_domain_register(struct ll_schedule_domain *domain,
				  uint64_t period, struct task *task,
				  void (*handler)(void *arg), void *arg)
{
	struct zephyr_domain *zephyr_domain = ll_sch_domain_get_pdata(domain);
	int core = cpu_get_id();
	struct zephyr_domain_thread *dt = zephyr_domain->domain_thread + core;
	char thread_name[] = "ll_thread0";
	k_tid_t thread;

	tr_dbg(&ll_tr, "zephyr_domain_register()");

	/* domain work only needs registered once on each core */
	if (dt->handler)
		return 0;

	if (!zephyr_domain->timer.user_data) {
		k_timer_init(&zephyr_domain->timer, zephyr_domain_timer_fn, NULL);
		zephyr_domain->timer.user_data = zephyr_domain;
	}

	dt->handler = handler;
	dt->arg = arg;
	dt->period_us = period;

	thread_name[sizeof(thread_name) - 2] = '0' + core;

	thread = k_thread_create(&dt->ll_thread,
				 ll_sched_stack[core],
				 ZEPHYR_LL_WORKQ_SIZE,
				 zephyr_domain_thread_fn, zephyr_domain, NULL, NULL,
				 -CONFIG_NUM_COOP_PRIORITIES, 0, K_FOREVER);

	k_thread_cpu_mask_clear(thread);
	k_thread_cpu_mask_enable(thread, core);
	k_thread_name_set(thread, thread_name);

	k_thread_start(thread);

	tr_info(&ll_tr, "zephyr_domain_register domain->type %d domain->clk %d domain->ticks_per_ms %d period %d",
		domain->type, domain->clk, domain->ticks_per_ms, (uint32_t)period);

	return 0;
}

static int zephyr_domain_unregister(struct ll_schedule_domain *domain,
				   struct task *task, uint32_t num_tasks)
{
	struct zephyr_domain *zephyr_domain = ll_sch_domain_get_pdata(domain);
	int core = cpu_get_id();

	tr_dbg(&ll_tr, "zephyr_domain_unregister()");

	/* tasks still registered on this core */
	if (num_tasks)
		return 0;

	k_timer_stop(&zephyr_domain->timer);
	zephyr_domain->domain_thread[core].handler = NULL;

	k_thread_abort(&zephyr_domain->domain_thread[core].ll_thread);

	tr_info(&ll_tr, "zephyr_domain_unregister domain->type %d domain->clk %d",
		domain->type, domain->clk);

	return 0;
}

/* Can be called on any core, sets timer IRQ for core 0 if CONFIG_SMP_BOOT_DELAY */
static void zephyr_domain_set(struct ll_schedule_domain *domain, uint64_t start)
{
	struct zephyr_domain *zephyr_domain = ll_sch_domain_get_pdata(domain);
	uint64_t current = platform_timer_get_atomic(zephyr_domain->ll_timer);
	uint64_t earliest_next = current + 1 + ZEPHYR_SCHED_COST;
	uint64_t ticks_tout = domain->ticks_per_ms * LL_TIMER_PERIOD_US / 1000;
	uint64_t ticks_req = MAX(start, earliest_next);
	int core = cpu_get_id();
	struct zephyr_domain_thread *dt = zephyr_domain->domain_thread + core;

	if (ticks_req >= domain->next_tick - ZEPHYR_SCHED_COST)
		return;

	if (domain->next_tick <= current ||
	    domain->next_tick > current + ticks_tout) {
		k_ticks_t remaining = k_timer_remaining_ticks(&zephyr_domain->timer);

		/* ll-scheduler expects .next_tick to be updated inside .set_domain() */
		if (remaining) {
			domain->next_tick = current + remaining;
		} else {
			k_timeout_t timeout, period;

			/* have we overshot the period length ?? */
			if (ticks_req > current + ticks_tout)
				ticks_req = current + ticks_tout;

			ticks_req -= ticks_req % CYC_PER_TICK;
			if (ticks_req < earliest_next) {
				/* The earliest schedule point has to be rounded up */
				ticks_req = earliest_next + CYC_PER_TICK - 1;
				ticks_req -= ticks_req % CYC_PER_TICK;
			}

			/* First call: start the timer */
			timeout = K_TIMEOUT_ABS_CYC(ticks_req - current);
			period = K_USEC(dt->period_us);

			k_timer_start(&zephyr_domain->timer, timeout, period);

			domain->next_tick = ticks_req;
		}
	}
}

static bool zephyr_domain_is_pending(struct ll_schedule_domain *domain,
				     struct task *task, struct comp_dev **comp)
{
	struct zephyr_domain *zephyr_domain = ll_sch_domain_get_pdata(domain);

	return task->start <= platform_timer_get_atomic(zephyr_domain->ll_timer);
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

	zephyr_domain->ll_timer = timer;
	zephyr_domain->ll_domain = domain;

	ll_sch_domain_set_pdata(domain, zephyr_domain);

	return domain;
}
