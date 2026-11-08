/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2026 Intel Corporation.
 */

/*
 * Private data of the Zephyr LL scheduler and its timer domain, shared by
 * the parts in src/schedule, which also run in the LL scheduler threads, and
 * the kernel parts in src/kernel/schedule. Not for use outside the scheduler.
 */

#ifndef __SOF_SCHEDULE_ZEPHYR_LL_H__
#define __SOF_SCHEDULE_ZEPHYR_LL_H__

#include <sof/schedule/ll_schedule_domain.h>
#include <rtos/task.h>
#include <zephyr/kernel.h>
#include <stdint.h>

struct zephyr_domain_thread {
	struct k_thread *ll_thread;
	struct k_sem *sem;
#ifndef CONFIG_SOF_USERSPACE_LL
	struct k_thread ll_thread_obj;
	struct k_sem sem_obj;
#endif
	void (*handler)(void *arg);
	void *arg;
};

struct zephyr_domain {
	struct k_timer *timer;
#ifndef CONFIG_SOF_USERSPACE_LL
	struct k_timer timer_obj;
#endif
	struct zephyr_domain_thread domain_thread[CONFIG_CORE_COUNT];
	struct ll_schedule_domain *ll_domain;
#if CONFIG_CROSS_CORE_STREAM && !CONFIG_SOF_USERSPACE_LL
	atomic_t block;
	struct k_mutex block_mutex;
	struct k_condvar block_condvar;
#endif
};

/* LL scheduler thread function, p1 is the domain and p2 the core */
void zephyr_domain_thread_fn(void *p1, void *p2, void *p3);

#ifdef CONFIG_SOF_USERSPACE_LL
int zephyr_domain_register_user(struct ll_schedule_domain *domain,
				struct task *task,
				void (*handler)(void *arg), void *arg);
int zephyr_domain_unregister_user(struct ll_schedule_domain *domain,
				  struct task *task, uint32_t num_tasks);
#endif

#if CONFIG_CROSS_CORE_STREAM && !CONFIG_SOF_USERSPACE_LL
void zephyr_domain_block(struct ll_schedule_domain *domain);
void zephyr_domain_unblock(struct ll_schedule_domain *domain);
#endif

#endif /* __SOF_SCHEDULE_ZEPHYR_LL_H__ */
