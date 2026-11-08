// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Guennadi Liakhovetski <guennadi.liakhovetski@linux.intel.com>

#include <sof/list.h>
#include <rtos/spinlock.h>
#include <rtos/symbol.h>
#include <sof/audio/component.h>
#include <rtos/interrupt.h>
#include <sof/lib/memory.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/zephyr_ll.h>
#include <sof/schedule/schedule.h>
#include <rtos/task.h>
#include <sof/lib/perf_cnt.h>
#include <zephyr/kernel.h>
#include <ipc4/base_fw.h>
#include <sof/debug/telemetry/telemetry.h>

LOG_MODULE_DECLARE(ll_schedule, CONFIG_SOF_LOG_LEVEL);

/* Kernel part of the Zephyr LL scheduler: the scheduler initialization, the task
 * semaphore syscalls and the access grants to the scheduler objects. The rest is in
 * src/schedule/zephyr_ll.c
 */

#if CONFIG_SOF_USERSPACE_LL
/*
 * Kernel-only copy of zephyr_ll_locks[]. That array is writable by user
 * threads, so kernel code granting access to the locks must use this copy,
 * otherwise a user thread could make it grant access to any kernel object.
 */
static struct k_mutex *zephyr_ll_klocks[CONFIG_CORE_COUNT];
#endif

#if CONFIG_DYNAMIC_OBJECTS
static struct list_item zephyr_ll_task_sem_list = LIST_INIT(zephyr_ll_task_sem_list);

struct zephyr_ll_task_sem {
	struct task *task;
	struct k_sem *sem;
	struct list_item list;
};

/*
 * The syscall validators pass in pdata that they read once from user memory
 * and validated, so these helpers must not read task->priv_data again.
 */
static int zephyr_ll_task_sem_alloc_pdata(struct task *task, struct zephyr_ll_pdata *pdata)
{
	struct zephyr_ll_task_sem *ts = rmalloc(SOF_MEM_FLAG_COHERENT, sizeof(*ts));

	if (!ts)
		return -ENOMEM;

	ts->sem = k_object_alloc(K_OBJ_SEM);
	if (!ts->sem) {
		rfree(ts);
		return -ENOMEM;
	}

	k_sem_init(ts->sem, 0, 1);

#if CONFIG_SOF_USERSPACE_LL
	/*
	 * The per-task semaphore is signalled from zephyr_ll_task_done(),
	 * which runs in the (unprivileged) LL scheduler thread when a task is
	 * freed while it is still running. k_object_alloc() only grants access
	 * to the calling thread (the IPC handler that creates the task), so the
	 * LL thread must be granted access explicitly, otherwise its
	 * k_sem_give() traps with a userspace permission fault.
	 *
	 * Resolve the LL thread from kernel-only per-core state keyed by the
	 * task's target core; never traverse the user-accessible scheduler or
	 * domain objects from privileged context.
	 */
	struct k_thread *ll_tid = zephyr_domain_thread_tid_for_core(task->core);

	if (ll_tid)
		k_thread_access_grant(ll_tid, ts->sem);
#endif /* CONFIG_SOF_USERSPACE_LL */

	ts->task = task;
	pdata->sem_p = ts->sem;
	/* List is protected by IPC serialization */
	list_item_append(&ts->list, &zephyr_ll_task_sem_list);

	return 0;
}

int z_impl_zephyr_ll_task_sem_alloc(struct task *task)
{
	return zephyr_ll_task_sem_alloc_pdata(task, task->priv_data);
}

static int zephyr_ll_task_sem_free_pdata(struct task *task, struct zephyr_ll_pdata *pdata)
{
	struct list_item *list;
	struct zephyr_ll_task_sem *ts;
	bool found = false;

	/* List is protected by IPC serialization */
	list_for_item(list, &zephyr_ll_task_sem_list) {
		ts = container_of(list, struct zephyr_ll_task_sem, list);
		if (ts->task == task) {
			found = true;
			break;
		}
	}

	if (!found)
		return -ENOENT;

	if (pdata->sem_p != ts->sem)
		return -EINVAL;

	list_item_del(list);
	k_object_free(ts->sem);
	rfree(ts);

	return 0;
}

int z_impl_zephyr_ll_task_sem_free(struct task *task)
{
	return zephyr_ll_task_sem_free_pdata(task, task->priv_data);
}

#ifdef CONFIG_USERSPACE
#include <zephyr/internal/syscall_handler.h>
#include <sof/userspace/syscall_caller.h>
/*
 * Read task->priv_data exactly once. The task lives in user memory and a
 * thread on another core can change the pointer after it has been checked.
 *
 * @return validated pdata pointer, or NULL if the task has none
 */
static struct zephyr_ll_pdata *zephyr_ll_task_pdata_vrfy(struct task *task)
{
	struct zephyr_ll_pdata *pdata;

	K_OOPS(K_SYSCALL_MEMORY_WRITE(task, sizeof(*task)));
	K_OOPS(k_usermode_from_copy(&pdata, &task->priv_data, sizeof(pdata)));
	if (pdata)
		K_OOPS(K_SYSCALL_MEMORY_WRITE(pdata, sizeof(*pdata)));

	return pdata;
}

static inline int z_vrfy_zephyr_ll_task_sem_alloc(struct task *task)
{
	struct zephyr_ll_pdata *pdata;

	SOF_SYSCALL_CALLER_CHECK(SOF_SYSUSER_INFRA);

	if (!task)
		return -EINVAL;
	pdata = zephyr_ll_task_pdata_vrfy(task);
	if (!pdata)
		return -EINVAL;

	return zephyr_ll_task_sem_alloc_pdata(task, pdata);
}
#include <zephyr/syscalls/zephyr_ll_task_sem_alloc_mrsh.c>

static inline int z_vrfy_zephyr_ll_task_sem_free(struct task *task)
{
	struct zephyr_ll_pdata *pdata;

	SOF_SYSCALL_CALLER_CHECK(SOF_SYSUSER_INFRA);

	if (!task)
		return -EINVAL;
	pdata = zephyr_ll_task_pdata_vrfy(task);
	if (!pdata)
		return -EINVAL;

	return zephyr_ll_task_sem_free_pdata(task, pdata);
}
#include <zephyr/syscalls/zephyr_ll_task_sem_free_mrsh.c>
#endif /* CONFIG_USERSPACE */
#endif /* CONFIG_DYNAMIC_OBJECTS */

#if CONFIG_SOF_USERSPACE_LL
struct k_thread *zephyr_ll_init_context(void *data, struct task *task)
{
	struct zephyr_ll *sch = data;
	struct zephyr_ll_pdata *pdata = task->priv_data;
	int ret;

	/*
	 * Use domain_thread_init() for privileged setup (thread creation,
	 * timer, access grants). domain_register() is now bookkeeping only
	 * and will be called later from user context when scheduling tasks.
	 */
	ret = domain_thread_init(sch->ll_domain, task);
	if (ret < 0) {
		tr_err(&ll_tr, "cannot init_context %d", ret);
		return NULL;
	}

	assert(!k_is_user_context());
	k_thread_access_grant(zephyr_domain_thread_tid(sch->ll_domain), sch->lock, pdata->sem_p);

	tr_dbg(&ll_tr, "granting access to lock %p for thread %p", sch->lock,
	       zephyr_domain_thread_tid(sch->ll_domain));
	tr_dbg(&ll_tr, "granting access to domain lock %p for thread %p", &sch->ll_domain->lock,
	       zephyr_domain_thread_tid(sch->ll_domain));

	return zephyr_domain_thread_tid(sch->ll_domain);
}

void user_ll_grant_access(struct k_thread *thread, int core)
{
	assert(core < CONFIG_CORE_COUNT && zephyr_ll_klocks[core] != NULL);

	k_thread_access_grant(thread, zephyr_ll_klocks[core]);
}
#endif /* CONFIG_SOF_USERSPACE_LL */

#if CONFIG_SOF_BOOT_TEST_STANDALONE || CONFIG_LIBRARY
static void zephyr_ll_scheduler_free(void *data, uint32_t flags)
{
	struct zephyr_ll *sch = data;

	zephyr_ll_assert_core(sch);

#if CONFIG_SOF_USERSPACE_LL
	zephyr_ll_locks[sch->core] = NULL;
	zephyr_ll_klocks[sch->core] = NULL;
	k_object_free(sch->lock);
#endif

	if (sch->n_tasks)
		tr_err(&ll_tr, "%u tasks are still active!",
		       sch->n_tasks);

#if CONFIG_SOF_USERSPACE_LL
	domain_thread_free(sch->ll_domain, sch->n_tasks);
#endif
	sof_heap_free(sch->heap, sch);
}
#endif /* CONFIG_SOF_BOOT_TEST_STANDALONE || CONFIG_LIBRARY */

static const struct scheduler_ops zephyr_ll_ops = {
	.schedule_task		= zephyr_ll_task_schedule,
	.schedule_task_before	= zephyr_ll_task_schedule_before,
	.schedule_task_after	= zephyr_ll_task_schedule_after,
	.schedule_task_free	= zephyr_ll_task_free,
	.schedule_task_cancel	= zephyr_ll_task_cancel,
#if CONFIG_SOF_BOOT_TEST_STANDALONE || CONFIG_LIBRARY
	.scheduler_free		= zephyr_ll_scheduler_free,
#endif
#if CONFIG_SOF_USERSPACE_LL
	.scheduler_init_context	= zephyr_ll_init_context,
#endif
};

/* TODO: low-power mode clock support */
/* Runs on each core during initialisation with the same domain argument */
__cold int zephyr_ll_scheduler_init(struct ll_schedule_domain *domain)
{
	struct zephyr_ll *sch;
	int core = cpu_get_id();
	struct k_heap *heap = sof_sys_heap_get();
	int flags = SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT;

	assert_can_be_cold();

#if CONFIG_SOF_USERSPACE_LL
	heap = zephyr_ll_user_heap();
	flags = SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT;
#endif
	tr_dbg(&ll_tr, "init on core %d", core);

	/* initialize per-core scheduler private data */
	sch = sof_heap_alloc(heap, flags, sizeof(*sch), 0);
	if (!sch) {
		tr_err(&ll_tr, "allocation failed");
		return -ENOMEM;
	}

	memset(sch, 0, sizeof(*sch));

	list_init(&sch->tasks);
	sch->ll_domain = domain;
	sch->core = core;
	sch->n_tasks = 0;
	sch->heap = heap;

#if CONFIG_SOF_USERSPACE_LL
	/* Allocate mutex dynamically for userspace access */
	sch->lock = k_object_alloc(K_OBJ_MUTEX);
	if (!sch->lock) {
		tr_err(&ll_tr, "mutex allocation failed");
		sof_heap_free(sch->heap, sch);
		return -ENOMEM;
	}
	assert(core < CONFIG_CORE_COUNT && zephyr_ll_locks[core] == NULL);
	zephyr_ll_locks[core] = sch->lock;
	zephyr_ll_klocks[core] = sch->lock;
	k_mutex_init(sch->lock);

	tr_dbg(&ll_tr, "ll-scheduler init done, sch %p sch->lock %p", sch, sch->lock);
#endif

	scheduler_init(domain->type, &zephyr_ll_ops, sch);

	return 0;
}
