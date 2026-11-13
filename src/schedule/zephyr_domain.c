// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019-2021 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <rtos/alloc.h>
#include <rtos/symbol.h>
#include <sof/lib/cpu.h>
#include <sof/lib/watchdog.h>
#include <sof/math/numbers.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/zephyr_ll.h>
#include <sof/schedule/schedule.h>
#include <rtos/task.h>
#include <ipc/topology.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/clock.h>

LOG_MODULE_DECLARE(ll_schedule, CONFIG_SOF_LOG_LEVEL);

/* The thread and timer setup, which runs in kernel mode, is in
 * src/kernel/schedule/zephyr_domain-kernel.c
 */

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

void zephyr_domain_thread_fn(void *p1, void *p2, void *p3)
{
	struct zephyr_domain *zephyr_domain = p1;
	int core = POINTER_TO_INT(p2);
	struct zephyr_domain_thread *dt = zephyr_domain->domain_thread + core;
#ifdef CONFIG_SCHEDULE_LL_STATS_LOG
	unsigned int runs = 0, overruns = 0, cycles_sum = 0, cycles_max = 0;
	unsigned int cycles0, cycles1, diff, timer_fired;
#endif

	tr_dbg(&ll_tr, "ll core %u thread starting", core);

	for (;;) {
		/* immediately go to sleep, waiting to be woken up by the timer */
		k_sem_take(dt->sem, K_FOREVER);

#ifdef CONFIG_SCHEDULE_LL_STATS_LOG
		cycles0 = k_cycle_get_32();
#endif

#if CONFIG_CROSS_CORE_STREAM && !CONFIG_SOF_USERSPACE_LL
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
#endif /* CONFIG_CROSS_CORE_STREAM && !CONFIG_SOF_USERSPACE_LL */

		if (dt->handler)
			dt->handler(dt->arg);

#ifdef CONFIG_SCHEDULE_LL_STATS_LOG
		cycles1 = k_cycle_get_32();

		/* This handles wrapping correctly too */
		diff = cycles1 - cycles0;

		timer_fired = k_timer_status_get(zephyr_domain->timer);
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

#ifdef CONFIG_SOF_USERSPACE_LL
/*
 * User-space register: bookkeeping only. The privileged thread setup has
 * already been done by domain_thread_init() called from kernel context.
 */
int zephyr_domain_register_user(struct ll_schedule_domain *domain,
				struct task *task,
				void (*handler)(void *arg), void *arg)
{
	struct zephyr_domain *zephyr_domain = ll_sch_domain_get_pdata(domain);
	struct zephyr_domain_thread *dt;
	int core;

	tr_dbg(&ll_tr, "register_user entry");

	if (task->core >= CONFIG_CORE_COUNT)
		return -EINVAL;

	core = task->core;
	dt = zephyr_domain->domain_thread + core;

	if (!dt->ll_thread) {
		tr_err(&ll_tr, "domain_thread_init() not called for core %d", core);
		return -EINVAL;
	}

	__ASSERT_NO_MSG(!dt->handler || dt->handler == handler);
	if (dt->handler)
		return 0;

	dt->handler = handler;
	dt->arg = arg;

	tr_info(&ll_tr, "task registered on core %d", core);

	return 0;
}

int zephyr_domain_unregister_user(struct ll_schedule_domain *domain,
				  struct task *task, uint32_t num_tasks)
{
	struct zephyr_domain *zephyr_domain = ll_sch_domain_get_pdata(domain);
	int core = task->core;

	tr_dbg(&ll_tr, "entry");

	/* tasks still registered on this core */
	if (num_tasks)
		return 0;

	k_mutex_lock(domain->lock, K_FOREVER);

	zephyr_domain->domain_thread[core].handler = NULL;

	k_mutex_unlock(domain->lock);

	/*
	 * In this user thread implementation, the timer is left
	 * running until privileged domain_thread_free() is called
	 * to clean up resources.
	 */

	tr_dbg(&ll_tr, "exit");

	return 0;
}
#endif /* CONFIG_SOF_USERSPACE_LL */

#if CONFIG_CROSS_CORE_STREAM && !CONFIG_SOF_USERSPACE_LL
void zephyr_domain_block(struct ll_schedule_domain *domain)
{
	struct zephyr_domain *zephyr_domain = ll_sch_domain_get_pdata(domain);

	tr_dbg(&ll_tr, "Blocking LL scheduler");

	k_mutex_lock(&zephyr_domain->block_mutex, K_FOREVER);
	atomic_set(&zephyr_domain->block, 1);
	k_mutex_unlock(&zephyr_domain->block_mutex);
}

void zephyr_domain_unblock(struct ll_schedule_domain *domain)
{
	struct zephyr_domain *zephyr_domain = ll_sch_domain_get_pdata(domain);

	tr_dbg(&ll_tr, "Unblocking LL scheduler");

	k_mutex_lock(&zephyr_domain->block_mutex, K_FOREVER);
	atomic_set(&zephyr_domain->block, 0);
	k_condvar_broadcast(&zephyr_domain->block_condvar);
	k_mutex_unlock(&zephyr_domain->block_mutex);
}
#endif /* CONFIG_CROSS_CORE_STREAM && !CONFIG_SOF_USERSPACE_LL */

/* Check if currently running in the LL scheduler thread context */
bool ll_sch_is_current(void)
{
	struct ll_schedule_domain *ll_dom = zephyr_ll_domain();
	struct zephyr_domain *zephyr_domain = ll_dom ? ll_sch_domain_get_pdata(ll_dom) : NULL;

	if (!zephyr_domain)
		return false;

	struct zephyr_domain_thread *dt = zephyr_domain->domain_thread + cpu_get_id();

	return k_current_get() == dt->ll_thread;
}
EXPORT_SYMBOL(ll_sch_is_current);
