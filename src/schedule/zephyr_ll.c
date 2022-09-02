// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Guennadi Liakhovetski <guennadi.liakhovetski@linux.intel.com>

#include <sof/list.h>
#include <rtos/spinlock.h>
#include <sof/audio/component.h>
#include <rtos/interrupt.h>
#include <sof/lib/notifier.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>

#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(ll_schedule, CONFIG_SOF_LOG_LEVEL);

/* 1547fe68-de0c-11eb-8461-3158a1294853 */
DECLARE_SOF_UUID("zll-schedule", zll_sched_uuid, 0x1547fe68, 0xde0c, 0x11eb,
		 0x84, 0x61, 0x31, 0x58, 0xa1, 0x29, 0x48, 0x53);

DECLARE_TR_CTX(ll_tr, SOF_UUID(zll_sched_uuid), LOG_LEVEL_INFO);

/* per-scheduler data */
struct zephyr_ll {
	struct list_item tasks;			/* list of ll tasks */
	unsigned int n_tasks;			/* task counter */
	struct ll_schedule_domain *ll_domain;	/* scheduling domain */
	unsigned int core;			/* core ID of this instance */
};

/* per-task scheduler data */
struct zephyr_ll_pdata {
	bool run;
	bool freeing;
	struct k_sem sem;
};

static void zephyr_ll_lock(struct zephyr_ll *sch, uint32_t *flags)
{
	irq_local_disable(*flags);
}

static void zephyr_ll_unlock(struct zephyr_ll *sch, uint32_t *flags)
{
	irq_local_enable(*flags);
}

static void zephyr_ll_assert_core(const struct zephyr_ll *sch)
{
	assert(CONFIG_CORE_COUNT == 1 || sch->core == cpu_get_id());
}

/* Locking: caller should hold the domain lock */
static void zephyr_ll_task_done(struct zephyr_ll *sch,
				struct task *task)
{
	list_item_del(&task->list);

	if (!sch->n_tasks) {
		tr_info(&ll_tr, "task count underrun!");
		k_panic();
	}

	task->state = SOF_TASK_STATE_FREE;

	tr_info(&ll_tr, "task complete %p %pU", task, task->uid);
	tr_info(&ll_tr, "num_tasks %d total_num_tasks %ld",
		sch->n_tasks, atomic_read(&sch->ll_domain->total_num_tasks));

	/*
	 * If this is the last task, domain_unregister() won't return. It is
	 * important to decrement the task counter last before aborting the
	 * thread.
	 */
	domain_unregister(sch->ll_domain, task, --sch->n_tasks);
}

/* The caller must hold the lock and possibly disable interrupts */
static void zephyr_ll_task_insert_unlocked(struct zephyr_ll *sch, struct task *task)
{
	struct task *task_iter;
	struct list_item *list;

	task->state = SOF_TASK_STATE_QUEUED;

	/*
	 * Tasks are added into the list from highest to lowest priority. This
	 * way they can then be run in the same order. Tasks with the same
	 * priority are served on a first-come-first-serve basis
	 */
	list_for_item(list, &sch->tasks) {
		task_iter = container_of(list, struct task, list);
		if (task->priority < task_iter->priority) {
			list_item_append(&task->list, &task_iter->list);
			break;
		}
	}

	/*
	 * If the task has not been added, it means that it has the lowest
	 * priority and should be added at the end of the list
	 */
	if (list == &sch->tasks)
		list_item_append(&task->list, &sch->tasks);
}

static void zephyr_ll_task_insert_before_unlocked(struct task *task, struct task *before)
{
	list_item_append(&task->list, &before->list);
}

static void zephyr_ll_task_insert_after_unlocked(struct task *task, struct task *after)
{
	list_item_prepend(&task->list, &after->list);
}

/* perf measurement windows size 2^x */
#define CYCLES_WINDOW_SIZE	10

static inline enum task_state do_task_run(struct task *task)
{
	uint32_t cycles0, cycles1, diff;
	enum task_state state;

	cycles0 = k_cycle_get_32();

	state = task_run(task);

	cycles1 = k_cycle_get_32();
	if (cycles1 > cycles0)
		diff = cycles1 - cycles0;
	else
		diff = UINT32_MAX - cycles0 + cycles1;

	task->cycles_sum += diff;
	task->cycles_max = diff > task->cycles_max ? diff : task->cycles_max;

	if (++task->cycles_cnt == 1 << CYCLES_WINDOW_SIZE) {
		task->cycles_sum >>= CYCLES_WINDOW_SIZE;
		tr_info(&ll_tr, "ll task %p %pU avg %u, max %u",
			task, task->uid, task->cycles_sum, task->cycles_max);
		task->cycles_sum = 0;
		task->cycles_cnt = 0;
		task->cycles_max = 0;
	}

	return state;
}

/*
 * Task state machine:
 * INIT:	initialized
 * QUEUED:	inserted into the scheduler queue
 * RUNNING:	the scheduler is running, the task is moved to a temporary list
 *		and then executed
 * CANCEL:	the task has been cancelled but it is still active. Transition
 *		to CANCEL can happen anywhere, where the lock is not held, tasks
 *		can be cancelled asynchronously from an arbitrary context
 * FREE:	removed from all lists, ready to be freed
 * other:	never set, shouldn't be used, but RESCHEDULE and COMPLETED are
 *		returned by task's .run function, they are assigned to a
 *		temporary state, but not to the task .state field
 */

/*
 * struct task::start and struct ll_schedule_domain::next are parts of the
 * original LL scheduler design, they aren't needed in this Zephyr-only
 * implementation and will be removed after an initial upstreaming.
 */
static void zephyr_ll_run(void *data)
{
	struct zephyr_ll *sch = data;
	struct task *task;
	struct list_item *list;
	uint32_t flags;

	zephyr_ll_lock(sch, &flags);

	/*
	 * We have to traverse the list manually, because we drop the lock while
	 * executing tasks, at that time tasks can be removed from or added to
	 * the list.
	 */
	list = sch->tasks.next;

	while (list != &sch->tasks) {
		enum task_state state;
		struct zephyr_ll_pdata *pdata;

		task = container_of(list, struct task, list);
		pdata = task->priv_data;

		if (task->state == SOF_TASK_STATE_CANCEL) {
			list = list->next;
			zephyr_ll_task_done(sch, task);
			continue;
		}

		pdata->run = true;
		task->state = SOF_TASK_STATE_RUNNING;

		zephyr_ll_unlock(sch, &flags);

		/*
		 * task's .run() should only return either
		 * SOF_TASK_STATE_COMPLETED or SOF_TASK_STATE_RESCHEDULE
		 */
		state = do_task_run(task);
		if (state != SOF_TASK_STATE_COMPLETED &&
		    state != SOF_TASK_STATE_RESCHEDULE) {
			tr_err(&ll_tr,
			       "zephyr_ll_run: invalid return state %u",
			       state);
			state = SOF_TASK_STATE_RESCHEDULE;
		}

		zephyr_ll_lock(sch, &flags);

		/*
		 * The .next pointer could've been changed while the lock wasn't
		 * held
		 */
		list = list->next;

		if (pdata->freeing) {
			/*
			 * zephyr_ll_task_free() is trying to free this task.
			 * complete it and signal the semaphore to let the
			 * function proceed
			 */
			zephyr_ll_task_done(sch, task);
			k_sem_give(&pdata->sem);
		} else if (state == SOF_TASK_STATE_COMPLETED) {
			zephyr_ll_task_done(sch, task);
		} else {
			/*
			 * task->state could've been changed to
			 * SOF_TASK_STATE_CANCEL
			 */
			switch (task->state) {
			case SOF_TASK_STATE_CANCEL:
				zephyr_ll_task_done(sch, task);
				break;
			default:
				break;
			}
		}
	}

	zephyr_ll_unlock(sch, &flags);

	notifier_event(sch, NOTIFIER_ID_LL_POST_RUN,
		       NOTIFIER_TARGET_CORE_LOCAL, NULL, 0);
}

/*
 * Called once for periodic tasks or multiple times for one-shot tasks
 * TODO: start should be ignored in Zephyr LL scheduler implementation. Tasks
 * are scheduled to start on the following tick and run on each subsequent timer
 * event. In the future we shall add support for long-period tasks with periods
 * equal to a multiple of the scheduler tick time. Ignoring start will eliminate
 * the use of task::start and ll_schedule_domain::next in this scheduler.
 */
static int zephyr_ll_task_schedule_common(struct zephyr_ll *sch, struct task *task,
					  uint64_t start, uint64_t period,
					  struct task *reference, bool before)
{
	struct zephyr_ll_pdata *pdata;
	struct task *task_iter;
	struct list_item *list;
	uint32_t flags;
	int ret;

	zephyr_ll_assert_core(sch);

	tr_info(&ll_tr, "task add %p %pU priority %d flags 0x%x", task, task->uid,
		task->priority, task->flags);

	zephyr_ll_lock(sch, &flags);

	pdata = task->priv_data;

	if (!pdata || pdata->freeing) {
		/*
		 * The user has called schedule_task_free() and then
		 * schedule_task(). This is clearly a bug in the application
		 * code, but we have to protect against it
		 */
		zephyr_ll_unlock(sch, &flags);
		return -EDEADLK;
	}

	/* check if the task is already scheduled */
	list_for_item(list, &sch->tasks) {
		task_iter = container_of(list, struct task, list);

		if (task_iter == task) {
			/* if cancelled, reschedule the task */
			if (task->state == SOF_TASK_STATE_CANCEL)
				break;

			/*
			 * keep original start. TODO: this shouldn't be happening.
			 * Remove after verification
			 */
			zephyr_ll_unlock(sch, &flags);
			tr_warn(&ll_tr, "task %p (%pU) already scheduled",
				task, task->uid);
			return 0;
		}
	}

	if (task->state == SOF_TASK_STATE_CANCEL) {
		/* do not queue the same task again */
		task->state = SOF_TASK_STATE_QUEUED;
		zephyr_ll_unlock(sch, &flags);
		return 0;
	}

	if (!reference)
		zephyr_ll_task_insert_unlocked(sch, task);
	else if (before)
		zephyr_ll_task_insert_before_unlocked(task, reference);
	else
		zephyr_ll_task_insert_after_unlocked(task, reference);

	sch->n_tasks++;

	zephyr_ll_unlock(sch, &flags);

	ret = domain_register(sch->ll_domain, task, &zephyr_ll_run, sch);
	if (ret < 0)
		tr_err(&ll_tr, "zephyr_ll_task_schedule: cannot register domain %d",
		       ret);

	return 0;
}

static int zephyr_ll_task_schedule(void *data, struct task *task, uint64_t start,
				   uint64_t period)
{
	struct zephyr_ll *sch = data;

	return zephyr_ll_task_schedule_common(sch, task, start, period, NULL, false);
}

static int zephyr_ll_task_schedule_before(void *data, struct task *task, uint64_t start,
					  uint64_t period, struct task *before)
{
	struct zephyr_ll *sch = data;

	return zephyr_ll_task_schedule_common(sch, task, start, period, before, true);
}

static int zephyr_ll_task_schedule_after(void *data, struct task *task, uint64_t start,
					 uint64_t period, struct task *after)
{
	struct zephyr_ll *sch = data;

	return zephyr_ll_task_schedule_common(sch, task, start, period, after, false);
}

/*
 * This is synchronous - after this returns the object can be destroyed!
 * Assertion: under Zephyr this is always called from a thread context!
 */
static int zephyr_ll_task_free(void *data, struct task *task)
{
	struct zephyr_ll *sch = data;
	uint32_t flags;
	struct zephyr_ll_pdata *pdata = task->priv_data;
	bool must_wait, on_list = true;

	zephyr_ll_assert_core(sch);

	if (k_is_in_isr()) {
		tr_err(&ll_tr,
		       "zephyr_ll_task_free: cannot free tasks from interrupt context!");
		return -EDEADLK;
	}

	zephyr_ll_lock(sch, &flags);

	/*
	 * It is safe to free the task in state INIT or QUEUED. CANCEL is
	 * unknown, because it can be set either in a safe state or in an unsafe
	 * one. If this function took the spin-lock while tasks are pending on a
	 * temporary list in zephyr_ll_run(), no task can be freed, because
	 * doing so would corrupt the temporary list. We could use an array of
	 * task pointers instead, but that array would have to be dynamically
	 * allocated and we anyway have to wait for the task completion at least
	 * when it is in the RUNNING state. To identify whether CANCEL is a safe
	 * one an additional flag must be used.
	 */
	switch (task->state) {
	case SOF_TASK_STATE_INIT:
	case SOF_TASK_STATE_FREE:
		on_list = false;
		/* fall through */
	case SOF_TASK_STATE_QUEUED:
		must_wait = false;
		break;
	case SOF_TASK_STATE_CANCEL:
		must_wait = pdata->run;
		break;
	default:
		must_wait = true;
	}

	if (on_list && !must_wait)
		zephyr_ll_task_done(sch, task);

	pdata->freeing = true;

	zephyr_ll_unlock(sch, &flags);

	if (must_wait)
		/* Wait for up to 100 periods */
		k_sem_take(&pdata->sem, K_USEC(LL_TIMER_PERIOD_US * 100));

	/* Protect against racing with schedule_task() */
	zephyr_ll_lock(sch, &flags);
	task->priv_data = NULL;
	rfree(pdata);
	zephyr_ll_unlock(sch, &flags);

	return 0;
}

/* This is asynchronous */
static int zephyr_ll_task_cancel(void *data, struct task *task)
{
	struct zephyr_ll *sch = data;
	uint32_t flags;

	zephyr_ll_assert_core(sch);

	/*
	 * Read-modify-write of task state in zephyr_ll_task_schedule() must be
	 * kept atomic, so we have to lock here too.
	 */
	zephyr_ll_lock(sch, &flags);
	if (task->state != SOF_TASK_STATE_FREE)
		task->state = SOF_TASK_STATE_CANCEL;
	zephyr_ll_unlock(sch, &flags);

	return 0;
}

/*
 * Runs on secondary cores in their shutdown sequence. In theory tasks can still
 * be active, but other schedulers ignore them too... And we don't need to free
 * the scheduler data - it's allocated in the SYS zone.
 */
static void zephyr_ll_scheduler_free(void *data, uint32_t flags)
{
	struct zephyr_ll *sch = data;

	zephyr_ll_assert_core(sch);

	if (sch->n_tasks)
		tr_err(&ll_tr, "zephyr_ll_scheduler_free: %u tasks are still active!",
		       sch->n_tasks);
}

static const struct scheduler_ops zephyr_ll_ops = {
	.schedule_task		= zephyr_ll_task_schedule,
	.schedule_task_before	= zephyr_ll_task_schedule_before,
	.schedule_task_after	= zephyr_ll_task_schedule_after,
	.schedule_task_free	= zephyr_ll_task_free,
	.schedule_task_cancel	= zephyr_ll_task_cancel,
	.scheduler_free		= zephyr_ll_scheduler_free,
};

int zephyr_ll_task_init(struct task *task,
			const struct sof_uuid_entry *uid, uint16_t type,
			uint16_t priority, enum task_state (*run)(void *data),
			void *data, uint16_t core, uint32_t flags)
{
	struct zephyr_ll_pdata *pdata;
	int ret;

	if (task->priv_data)
		return -EEXIST;

	ret = schedule_task_init(task, uid, type, priority, run, data, core,
				 flags);
	if (ret < 0)
		return ret;

	pdata = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0, SOF_MEM_CAPS_RAM,
			sizeof(*pdata));
	if (!pdata) {
		tr_err(&ll_tr, "zephyr_ll_task_init(): alloc failed");
		return -ENOMEM;
	}

	k_sem_init(&pdata->sem, 0, 1);

	task->priv_data = pdata;

	return 0;
}

/* TODO: low-power mode clock support */
/* Runs on each core during initialisation with the same domain argument */
int zephyr_ll_scheduler_init(struct ll_schedule_domain *domain)
{
	struct zephyr_ll *sch;

	if (domain->type != SOF_SCHEDULE_LL_TIMER) {
		tr_warn(&ll_tr, "zephyr_ll_scheduler_init(): unsupported domain %u",
			domain->type);
		return -EINVAL;
	}

	/* initialize per-core scheduler private data */
	sch = rmalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM, sizeof(*sch));
	list_init(&sch->tasks);
	sch->ll_domain = domain;
	sch->core = cpu_get_id();
	sch->n_tasks = 0;

	scheduler_init(domain->type, &zephyr_ll_ops, sch);

	return 0;
}
