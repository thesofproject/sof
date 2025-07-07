// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019-2021 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <rtos/timer.h>
#include <rtos/alloc.h>
#include <rtos/symbol.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/watchdog.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/schedule.h>
#include <rtos/task.h>
#include <ipc/topology.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/sys_clock.h>

LOG_MODULE_DECLARE(ll_schedule, CONFIG_SOF_LOG_LEVEL);

/*
 * Currently the Zephyr clock rate is part it's Kconfig known at build time.
 * SOF on Intel CAVS platforms currently only aligns with Zephyr when both
 * use the CAVS 19.2 MHz SSP clock. TODO - needs runtime alignment.
 */
#if CONFIG_XTENSA && CONFIG_CAVS && !CONFIG_INTEL_ADSP_TIMER
#error "Zephyr uses 19.2MHz clock derived from SSP which must be enabled."
#endif

#define ZEPHYR_LL_STACK_SIZE	8192

K_KERNEL_STACK_ARRAY_DEFINE(ll_sched_stack, CONFIG_CORE_COUNT, ZEPHYR_LL_STACK_SIZE);

struct zephyr_domain_thread {
	struct k_thread ll_thread;
	struct k_sem sem;
	void (*handler)(void *arg);
	void *arg;
};

struct zephyr_domain {
	struct k_timer timer;
	struct zephyr_domain_thread domain_thread[CONFIG_CORE_COUNT];
	struct ll_schedule_domain *ll_domain;
#if CONFIG_CROSS_CORE_STREAM
	atomic_t block;
	struct k_mutex block_mutex;
	struct k_condvar block_condvar;
#endif
};

/* perf measurement windows size 2^x */
#define CYCLES_WINDOW_SIZE	CONFIG_SCHEDULE_LL_STATS_LOG_WINDOW_SIZE

#ifdef CONFIG_SCHEDULE_LL_STATS_LOG
static inline void stats_report(unsigned int runs, int core, unsigned int cycles_sum,
				unsigned int cycles_max, unsigned int overruns)
{
#ifdef CONFIG_SCHEDULE_LL_STATS_LOG_EVERY_OTHER_WINDOW
	if (runs & BIT(CYCLES_WINDOW_SIZE))
		return;
#endif
	tr_info(&ll_tr, "ll core %u timer avg %u, max %u, overruns %u",
		core, cycles_sum, cycles_max, overruns);
}
#endif /* CONFIG_SCHEDULE_LL_STATS_LOG */

static void zephyr_domain_thread_fn(void *p1, void *p2, void *p3)
{
	struct zephyr_domain *zephyr_domain = p1;
	int core = cpu_get_id();
	struct zephyr_domain_thread *dt = zephyr_domain->domain_thread + core;
#ifdef CONFIG_SCHEDULE_LL_STATS_LOG
	unsigned int runs = 0, overruns = 0, cycles_sum = 0, cycles_max = 0;
	unsigned int cycles0, cycles1, diff, timer_fired;
#endif

	for (;;) {
		/* immediately go to sleep, waiting to be woken up by the timer */
		k_sem_take(&dt->sem, K_FOREVER);

#ifdef CONFIG_SCHEDULE_LL_STATS_LOG
		cycles0 = k_cycle_get_32();
#endif

#if CONFIG_CROSS_CORE_STREAM
		/*
		 * If zephyr_domain->block is set -- block LL scheduler from starting its
		 * next cycle.
		 * Mutex locking might be somewhat expensive, hence first check for
		 * zephyr_domain->block value is made without locking the mutex. If
		 * zephyr_domain->block is not set -- no need to do anything. Otherwise,
		 * usual condvar procedure is performed: mutex is locked to properly check
		 * zephyr_domain->block value again to avoid race with unblocking procedure
		 * (clearing zephyr_domain->block and broadcasting the condvar).
		 */
		if (atomic_get(&zephyr_domain->block)) {
			k_mutex_lock(&zephyr_domain->block_mutex, K_FOREVER);
			if (atomic_get(&zephyr_domain->block))
				k_condvar_wait(&zephyr_domain->block_condvar,
					       &zephyr_domain->block_mutex, K_FOREVER);
			k_mutex_unlock(&zephyr_domain->block_mutex);
		}
#endif

		dt->handler(dt->arg);

#ifdef CONFIG_SCHEDULE_LL_STATS_LOG
		cycles1 = k_cycle_get_32();

		/* This handles wrapping correctly too */
		diff = cycles1 - cycles0;

		timer_fired = k_timer_status_get(&zephyr_domain->timer);
		if (timer_fired > 1)
			overruns++;

		cycles_sum += diff;
		cycles_max = diff > cycles_max ? diff : cycles_max;

		if (!(++runs & MASK(CYCLES_WINDOW_SIZE - 1, 0))) {
			cycles_sum >>= CYCLES_WINDOW_SIZE;
			stats_report(runs, core, cycles_sum, cycles_max, overruns);
			cycles_sum = 0;
			cycles_max = 0;
		}
#endif /* CONFIG_SCHEDULE_LL_STATS_LOG */

		/* Feed the watchdog */
		watchdog_feed(core);
	}
}

/* Timer callback: runs in timer IRQ context */
static void zephyr_domain_timer_fn(struct k_timer *timer)
{
	struct zephyr_domain *zephyr_domain = k_timer_user_data_get(timer);
	int core;

	/*
	 * A race is possible when the Zephyr LL scheduling domain is being
	 * unregistered while a timer IRQ is processed on a different core. Then
	 * the timer is removed by the former but then re-added by the latter,
	 * but this time with no user data and no handler set. This leads to the
	 * timer continuing to trigger and then leading to a lock up when it is
	 * registered again next time.
	 */
	if (!zephyr_domain) {
		k_timer_stop(timer);
		return;
	}

	for (core = 0; core < CONFIG_CORE_COUNT; core++) {
		struct zephyr_domain_thread *dt = zephyr_domain->domain_thread + core;

		if (dt->handler)
			k_sem_give(&dt->sem);
	}
}

static int zephyr_domain_register(struct ll_schedule_domain *domain,
				  struct task *task,
				  void (*handler)(void *arg), void *arg)
{
	struct zephyr_domain *zephyr_domain = ll_sch_domain_get_pdata(domain);
	int core = cpu_get_id();
	struct zephyr_domain_thread *dt = zephyr_domain->domain_thread + core;
	char thread_name[] = "ll_thread0";
	k_tid_t thread;
	k_spinlock_key_t key;

	tr_dbg(&ll_tr, "zephyr_domain_register()");

	/* domain work only needs registered once on each core */
	if (dt->handler)
		return 0;

	dt->handler = handler;
	dt->arg = arg;

	/* 10 is rather random, we better not accumulate 10 missed timer interrupts */
	k_sem_init(&dt->sem, 0, 10);

	thread_name[sizeof(thread_name) - 2] = '0' + core;

	thread = k_thread_create(&dt->ll_thread,
				 ll_sched_stack[core],
				 ZEPHYR_LL_STACK_SIZE,
				 zephyr_domain_thread_fn, zephyr_domain, NULL, NULL,
				 CONFIG_LL_THREAD_PRIORITY, 0, K_FOREVER);

	k_thread_cpu_mask_clear(thread);
	k_thread_cpu_mask_enable(thread, core);
	k_thread_name_set(thread, thread_name);

	k_thread_start(thread);

	key = k_spin_lock(&domain->lock);

	if (!k_timer_user_data_get(&zephyr_domain->timer)) {
		k_timeout_t start = {0};

		k_timer_init(&zephyr_domain->timer, zephyr_domain_timer_fn, NULL);
		k_timer_user_data_set(&zephyr_domain->timer, zephyr_domain);

		k_timer_start(&zephyr_domain->timer, start, K_USEC(LL_TIMER_PERIOD_US));

		/* Enable the watchdog */
		watchdog_enable(core);
	}

	k_spin_unlock(&domain->lock, key);

	tr_info(&ll_tr, "zephyr_domain_register domain->type %d domain->clk %d domain->ticks_per_ms %d period %d",
		domain->type, domain->clk, domain->ticks_per_ms, (uint32_t)LL_TIMER_PERIOD_US);

	return 0;
}

static int zephyr_domain_unregister(struct ll_schedule_domain *domain,
				   struct task *task, uint32_t num_tasks)
{
	struct zephyr_domain *zephyr_domain = ll_sch_domain_get_pdata(domain);
	int core = cpu_get_id();
	k_spinlock_key_t key;

	tr_dbg(&ll_tr, "zephyr_domain_unregister()");

	/* tasks still registered on this core */
	if (num_tasks)
		return 0;

	key = k_spin_lock(&domain->lock);

	if (!atomic_read(&domain->total_num_tasks)) {
		/* Disable the watchdog */
		watchdog_disable(core);

		k_timer_stop(&zephyr_domain->timer);
		k_timer_user_data_set(&zephyr_domain->timer, NULL);
	}

	zephyr_domain->domain_thread[core].handler = NULL;

	k_spin_unlock(&domain->lock, key);

	tr_info(&ll_tr, "zephyr_domain_unregister domain->type %d domain->clk %d",
		domain->type, domain->clk);

	/*
	 * If running in the context of the domain thread, k_thread_abort() will
	 * not return
	 */
	k_thread_abort(&zephyr_domain->domain_thread[core].ll_thread);

	return 0;
}

#if CONFIG_CROSS_CORE_STREAM
static void zephyr_domain_block(struct ll_schedule_domain *domain)
{
	struct zephyr_domain *zephyr_domain = ll_sch_domain_get_pdata(domain);

	tr_dbg(&ll_tr, "Blocking LL scheduler");

	k_mutex_lock(&zephyr_domain->block_mutex, K_FOREVER);
	atomic_set(&zephyr_domain->block, 1);
	k_mutex_unlock(&zephyr_domain->block_mutex);
}

static void zephyr_domain_unblock(struct ll_schedule_domain *domain)
{
	struct zephyr_domain *zephyr_domain = ll_sch_domain_get_pdata(domain);

	tr_dbg(&ll_tr, "Unblocking LL scheduler");

	k_mutex_lock(&zephyr_domain->block_mutex, K_FOREVER);
	atomic_set(&zephyr_domain->block, 0);
	k_condvar_broadcast(&zephyr_domain->block_condvar);
	k_mutex_unlock(&zephyr_domain->block_mutex);
}
#endif

static const struct ll_schedule_domain_ops zephyr_domain_ops = {
	.domain_register	= zephyr_domain_register,
	.domain_unregister	= zephyr_domain_unregister,
#if CONFIG_CROSS_CORE_STREAM
	.domain_block		= zephyr_domain_block,
	.domain_unblock		= zephyr_domain_unblock,
#endif
};

struct ll_schedule_domain *zephyr_domain_init(int clk)
{
	struct ll_schedule_domain *domain;
	struct zephyr_domain *zephyr_domain;

	domain = domain_init(SOF_SCHEDULE_LL_TIMER, clk, false,
			     &zephyr_domain_ops);
	if (!domain) {
		tr_err(&ll_tr, "zephyr_domain_init: domain init failed");
		return NULL;
	}

	zephyr_domain = rzalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT,
				sizeof(*zephyr_domain));
	if (!zephyr_domain) {
		tr_err(&ll_tr, "zephyr_domain_init: domain allocation failed");
		rfree(domain);
		return NULL;
	}

	zephyr_domain->ll_domain = domain;

#if CONFIG_CROSS_CORE_STREAM
	atomic_set(&zephyr_domain->block, 0);
	k_mutex_init(&zephyr_domain->block_mutex);
	k_condvar_init(&zephyr_domain->block_condvar);
#endif

	ll_sch_domain_set_pdata(domain, zephyr_domain);

	return domain;
}

/* Check if currently running in the LL scheduler thread context */
bool ll_sch_is_current(void)
{
	struct zephyr_domain *zephyr_domain = ll_sch_domain_get_pdata(zephyr_ll_domain());

	if (!zephyr_domain)
		return false;

	struct zephyr_domain_thread *dt = zephyr_domain->domain_thread + cpu_get_id();

	return k_current_get() == &dt->ll_thread;
}
EXPORT_SYMBOL(ll_sch_is_current);
