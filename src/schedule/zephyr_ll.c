// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Guennadi Liakhovetski <guennadi.liakhovetski@linux.intel.com>

#include <sof/list.h>
#include <sof/spinlock.h>
#include <sof/audio/component.h>
#include <sof/drivers/interrupt.h>
#include <sof/lib/notifier.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>

#include <kernel.h>

/* 1547fe68-de0c-11eb-8461-3158a1294853 */
DECLARE_SOF_UUID("zll-schedule", zll_sched_uuid, 0x1547fe68, 0xde0c, 0x11eb,
		 0x84, 0x61, 0x31, 0x58, 0xa1, 0x29, 0x48, 0x53);

DECLARE_TR_CTX(ll_tr, SOF_UUID(zll_sched_uuid), LOG_LEVEL_INFO);

/* per-scheduler data */
struct zephyr_ll {
	struct list_item tasks;			/* list of ll tasks */
	unsigned int n_tasks;			/* task counter */
	spinlock_t lock;			/* protects the list of tasks and the counter */
	struct ll_schedule_domain *ll_domain;	/* scheduling domain */
};

/* per-task scheduler data */
struct zephyr_ll_pdata {
	bool run;
	bool freeing;
	struct k_sem sem;
};

/* FIXME: would be good to use the timer, configured in the underlying domain */
#define domain_time(sch) platform_timer_get_atomic(timer_get())

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
	tr_info(&ll_tr, "num_tasks %d total_num_tasks %d",
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
	struct list_item *list, *tmp, head;
	uint32_t flags;

	list_init(&head);

	spin_lock_irq(&sch->lock, flags);

	/* Move all pending tasks to a temporary local list */
	list_for_item_safe(list, tmp, &sch->tasks) {
		struct comp_dev *sched_comp;
		struct zephyr_ll_pdata *pdata;

		task = container_of(list, struct task, list);
		pdata = task->priv_data;

		if (task->state == SOF_TASK_STATE_CANCEL) {
			zephyr_ll_task_done(sch, task);
			continue;
		}

		/* To be removed together with .start and .next_tick */
		if (!domain_is_pending(sch->ll_domain, task, &sched_comp))
			continue;

		/*
		 * Do not adjust .n_tasks, it's only decremented if the task is
		 * removed via zephyr_ll_task_done()
		 */
		list_item_del(&task->list);
		list_item_append(&task->list, &head);
		pdata->run = true;

		task->state = SOF_TASK_STATE_RUNNING;
	}

	spin_unlock_irq(&sch->lock, flags);

	/*
	 * Execute tasks on the temporary list. We are racing against
	 * zephyr_ll_task_free(), see comments there for details.
	 */
	list_for_item_safe(list, tmp, &head) {
		enum task_state state;
		struct zephyr_ll_pdata *pdata;

		task = container_of(list, struct task, list);

		/*
		 * While the lock is not held, the task can be cancelled, we
		 * deal with it after the task completion.
		 */
		if (task->state == SOF_TASK_STATE_RUNNING) {
			/*
			 * task's .run() should only return either
			 * SOF_TASK_STATE_COMPLETED or SOF_TASK_STATE_RESCHEDULE
			 */
			state = task_run(task);
			if (state != SOF_TASK_STATE_COMPLETED &&
			    state != SOF_TASK_STATE_RESCHEDULE) {
				tr_err(&ll_tr,
				       "zephyr_ll_run: invalid return state %u",
				       state);
				state = SOF_TASK_STATE_RESCHEDULE;
			}
		} else {
			state = SOF_TASK_STATE_COMPLETED;
		}

		spin_lock_irq(&sch->lock, flags);

		pdata = task->priv_data;

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
				/* reschedule */
				list_item_del(&task->list);

				zephyr_ll_task_insert_unlocked(sch, task);

				task->start = sch->ll_domain->next_tick;
			}
		}

		spin_unlock_irq(&sch->lock, flags);
	}

	notifier_event(sch, NOTIFIER_ID_LL_POST_RUN,
		       NOTIFIER_TARGET_CORE_LOCAL, NULL, 0);
}

static void zephyr_ll_init_scheduler_for_first_task(struct zephyr_ll *sch)
{
	/*
	 * If we're adding the first task to the scheduler, it is possible that
	 * the scheduler has already run before on this core and its thread has
	 * been aborted while holding the spinlock, so we have to re-initialize
	 * it.
	 */
	spinlock_init(&sch->lock);
}

/*
 * Called once for periodic tasks or multiple times for one-shot tasks
 * TODO: start should be ignored in Zephyr LL scheduler implementation. Tasks
 * are scheduled to start on the following tick and run on each subsequent timer
 * event. In the future we shall add support for long-period tasks with periods
 * equal to a multiple of the scheduler tick time. Ignoring start will eliminate
 * the use of task::start and ll_schedule_domain::next in this scheduler.
 */
static int zephyr_ll_task_schedule(void *data, struct task *task, uint64_t start,
				   uint64_t period)
{
	struct zephyr_ll *sch = data;
	struct zephyr_ll_pdata *pdata;
	struct task *task_iter;
	struct list_item *list;
	uint64_t delay = period ? period : start;
	uint32_t flags;
	int ret;

	tr_info(&ll_tr, "task add %p %pU priority %d flags 0x%x", task, task->uid,
		task->priority, task->flags);

	irq_local_disable(flags);
	/*
	 * The task counter is decremented as the last operation in
	 * zephyr_ll_task_done() before aborting the thread, so just disabling
	 * local IRQs provides sufficient protection here.
	 */
	if (!sch->n_tasks)
		zephyr_ll_init_scheduler_for_first_task(sch);
	irq_local_enable(flags);

	spin_lock_irq(&sch->lock, flags);

	pdata = task->priv_data;

	if (!pdata || pdata->freeing) {
		/*
		 * The user has called schedule_task_free() and then
		 * schedule_task(). This is clearly a bug in the application
		 * code, but we have to protect against it
		 */
		spin_unlock_irq(&sch->lock, flags);
		return -EDEADLK;
	}

	/* check if the task is already scheduled */
	list_for_item(list, &sch->tasks) {
		task_iter = container_of(list, struct task, list);

		/*
		 * keep original start. TODO: this shouldn't be happening.
		 * Remove after verification
		 */
		if (task_iter == task) {
			spin_unlock_irq(&sch->lock, flags);
			tr_warn(&ll_tr, "task %p (%pU) already scheduled",
				task, task->uid);
			return 0;
		}
	}

	task->start = domain_time(sch) + delay;
	if (sch->ll_domain->next_tick != UINT64_MAX &&
	    sch->ll_domain->next_tick > task->start)
		task->start = sch->ll_domain->next_tick;

	zephyr_ll_task_insert_unlocked(sch, task);

	sch->n_tasks++;

	spin_unlock_irq(&sch->lock, flags);

	ret = domain_register(sch->ll_domain, task, &zephyr_ll_run, sch);
	if (ret < 0)
		tr_err(&ll_tr, "zephyr_ll_task_schedule: cannot register domain %d",
		       ret);

	return 0;
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

	if (k_is_in_isr()) {
		tr_err(&ll_tr,
		       "zephyr_ll_task_free: cannot free tasks from interrupt context!");
		return -EDEADLK;
	}

	spin_lock_irq(&sch->lock, flags);

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
	case SOF_TASK_STATE_FREE:
		on_list = false;
		/* fall through */
	case SOF_TASK_STATE_INIT:
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

	spin_unlock_irq(&sch->lock, flags);

	if (must_wait)
		/* Wait for up to 100 periods */
		k_sem_take(&pdata->sem, K_USEC(LL_TIMER_PERIOD_US * 100));

	/* Protect against racing with schedule_task() */
	spin_lock_irq(&sch->lock, flags);
	task->priv_data = NULL;
	rfree(pdata);
	spin_unlock_irq(&sch->lock, flags);

	return 0;
}

/* This seems to be asynchronous? */
static int zephyr_ll_task_cancel(void *data, struct task *task)
{
	struct zephyr_ll *sch = data;
	uint32_t flags;

	/*
	 * Read-modify-write of task state in zephyr_ll_task_schedule() must be
	 * kept atomic, so we have to lock here too.
	 */
	spin_lock_irq(&sch->lock, flags);
	task->state = SOF_TASK_STATE_CANCEL;
	spin_unlock_irq(&sch->lock, flags);

	return 0;
}

/*
 * Runs on secondary cores in their shutdown sequence. In theory tasks can still
 * be active, but other schedulers ignore them too... And we don't need to free
 * the scheduler data - it's allocated in the SYS zone.
 */
static void zephyr_ll_scheduler_free(void *data)
{
	struct zephyr_ll *sch = data;

	if (sch->n_tasks)
		tr_err(&ll_tr, "zephyr_ll_scheduler_free: %u tasks are still active!",
		       sch->n_tasks);
}

static const struct scheduler_ops zephyr_ll_ops = {
	.schedule_task		= zephyr_ll_task_schedule,
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

	pdata = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0, SOF_MEM_CAPS_RAM,
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
		tr_err(&ll_tr, "zephyr_ll_scheduler_init(): unsupported domain %u",
		       domain->type);
		return -EINVAL;
	}

	/* initialize per-core scheduler private data */
	sch = rmalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM, sizeof(*sch));
	list_init(&sch->tasks);
	sch->ll_domain = domain;

	scheduler_init(domain->type, &zephyr_ll_ops, sch);

	return 0;
}
