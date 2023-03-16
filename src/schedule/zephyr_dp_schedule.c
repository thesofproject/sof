// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Szkudlinski
 */

#include <sof/audio/component.h>
#include <rtos/task.h>
#include <stdint.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/trace/trace.h>
#include <rtos/wait.h>
#include <rtos/interrupt.h>
#include <zephyr/kernel.h>
#include <zephyr/sys_clock.h>
#include <sof/lib/notifier.h>

#include <zephyr/kernel/thread.h>

LOG_MODULE_REGISTER(dp_schedule, CONFIG_SOF_LOG_LEVEL);
/* 87858bc2-baa9-40b6-8e4c-2c95ba8b1545 */
DECLARE_SOF_UUID("dp-schedule", dp_sched_uuid, 0x87858bc2, 0xbaa9, 0x40b6,
		 0x8e, 0x4c, 0x2c, 0x95, 0xba, 0x8b, 0x15, 0x45);

DECLARE_TR_CTX(dp_tr, SOF_UUID(dp_sched_uuid), LOG_LEVEL_INFO);

struct scheduler_dp_data {
	struct list_item tasks;		/* list of active dp tasks */
	struct k_spinlock lock;		/* synchronization between cores */
};

struct task_dp_pdata {
	k_tid_t thread_id;		/* zephyr thread ID */
	k_thread_stack_t __sparse_cache *p_stack;	/* pointer to thread stack */
	uint32_t ticks_period;		/* period the task should be scheduled in LL ticks */
	uint32_t ticks_to_trigger;	/* number of ticks the task should be triggered after */
	struct k_sem sem;		/* semaphore for task scheduling */
};

/*
 * there's only one instance of DP scheduler for all cores
 * Keep pointer to it here
 */
static struct scheduler_dp_data *dp_sch;

static inline k_spinlock_key_t scheduler_dp_lock(void)
{
	return k_spin_lock(&dp_sch->lock);
}

static inline void scheduler_dp_unlock(k_spinlock_key_t key)
{
	k_spin_unlock(&dp_sch->lock, key);
}

/*
 * function called after every LL tick
 *
 * TODO:
 * the scheduler should here calculate deadlines of all task and tell Zephyr about them
 * Currently there's an assumption that the task is always ready to run
 */
void scheduler_dp_ll_tick(void *receiver_data, enum notify_id event_type, void *caller_data)
{
	(void)receiver_data;
	(void)event_type;
	(void)caller_data;
	struct list_item *tlist;
	struct task *curr_task;
	struct task_dp_pdata *pdata;
	k_spinlock_key_t lock_key;

	if (cpu_get_id() != PLATFORM_PRIMARY_CORE_ID)
		return;

	if (!dp_sch)
		return;

	lock_key = scheduler_dp_lock();
	list_for_item(tlist, &dp_sch->tasks) {
		curr_task = container_of(tlist, struct task, list);
		pdata = curr_task->priv_data;

		if (pdata->ticks_to_trigger == 0) {
			if (curr_task->state == SOF_TASK_STATE_QUEUED) {
				/* set new trigger time, start the thread */
				pdata->ticks_to_trigger = pdata->ticks_period;
				curr_task->state = SOF_TASK_STATE_RUNNING;
				k_sem_give(&pdata->sem);
			}
		} else {
			if (curr_task->state == SOF_TASK_STATE_QUEUED ||
			    curr_task->state == SOF_TASK_STATE_RUNNING)
				/* decrease num of ticks to re-schedule */
				pdata->ticks_to_trigger--;
		}
	}
	scheduler_dp_unlock(lock_key);
}

static int scheduler_dp_task_cancel(void *data, struct task *task)
{
	(void)(data);
	k_spinlock_key_t lock_key;

	/* this is asyn cancel - mark the task as canceled and remove it from scheduling */
	lock_key = scheduler_dp_lock();

	task->state = SOF_TASK_STATE_CANCEL;
	list_item_del(&task->list);

	scheduler_dp_unlock(lock_key);

	return 0;
}

static int scheduler_dp_task_free(void *data, struct task *task)
{
	k_spinlock_key_t lock_key;
	struct task_dp_pdata *pdata = task->priv_data;

	/* abort the execution of the thread */
	k_thread_abort(pdata->thread_id);

	lock_key = scheduler_dp_lock();
	list_item_del(&task->list);
	task->priv_data = NULL;
	task->state = SOF_TASK_STATE_FREE;
	scheduler_dp_unlock(lock_key);

	/* free task stack */
	rfree((__sparse_force void *)pdata->p_stack);

	/* all other memory has been allocated as a single malloc, will be freed later by caller */
	return 0;
}

/* Thread function called in component context, on target core */
static void dp_thread_fn(void *p1, void *p2, void *p3)
{
	struct task *task = p1;
	(void)p2;
	(void)p3;
	struct task_dp_pdata *task_pdata = task->priv_data;
	k_spinlock_key_t lock_key;
	enum task_state state;

	while (1) {
		/*
		 * the thread is started immediately after creation, it will stop on semaphore
		 * Semaphore will be released once the task is ready to process
		 */
		k_sem_take(&task_pdata->sem, K_FOREVER);

		if (task->state == SOF_TASK_STATE_RUNNING)
			state = task_run(task);
		else
			state = task->state;	/* to avoid undefined variable warning */

		lock_key = scheduler_dp_lock();
		/*
		 * check if task is still running, may have been canceled by external call
		 * if not, set the state returned by run procedure
		 */
		if (task->state == SOF_TASK_STATE_RUNNING) {
			task->state = state;
			switch (state) {
			case SOF_TASK_STATE_RESCHEDULE:
				/* mark to reschedule, schedule time is already calculated */
				task->state = SOF_TASK_STATE_QUEUED;
				break;

			case SOF_TASK_STATE_CANCEL:
			case SOF_TASK_STATE_COMPLETED:
				/* remove from scheduling */
				list_item_del(&task->list);
				break;

			default:
				/* illegal state, serious defect, won't happen */
				k_panic();
			}
		}

		/* call task_complete  */
		if (task->state == SOF_TASK_STATE_COMPLETED) {
			/* call task_complete out of lock, it may eventually call schedule again */
			scheduler_dp_unlock(lock_key);
			task_complete(task);
		} else {
			scheduler_dp_unlock(lock_key);
		}

	};

	/* never be here */
}

static int scheduler_dp_task_shedule(void *data, struct task *task, uint64_t start,
				     uint64_t period)
{
	struct scheduler_dp_data *sch = data;
	struct task_dp_pdata *pdata = task->priv_data;
	k_spinlock_key_t lock_key;

	lock_key = scheduler_dp_lock();

	if (task->state != SOF_TASK_STATE_INIT &&
	    task->state != SOF_TASK_STATE_CANCEL &&
	    task->state != SOF_TASK_STATE_COMPLETED) {
		scheduler_dp_unlock(lock_key);
		return -EINVAL;
	}

	/* calculate period and start time in LL ticks */
	pdata->ticks_period = period / LL_TIMER_PERIOD_US;

	/* add a task to DP scheduler list */
	list_item_prepend(&task->list, &sch->tasks);

	if (start == SCHEDULER_DP_RUN_TASK_IMMEDIATELY) {
		/* trigger the task immediately, don't wait for LL tick */
		pdata->ticks_to_trigger = 0;
		task->state = SOF_TASK_STATE_RUNNING;
		k_sem_give(&pdata->sem);
	} else {
		/* wait for tick */
		pdata->ticks_to_trigger	= start / LL_TIMER_PERIOD_US;
		task->state = SOF_TASK_STATE_QUEUED;
	}

	scheduler_dp_unlock(lock_key);

	return 0;
}

static struct scheduler_ops schedule_dp_ops = {
	.schedule_task		= scheduler_dp_task_shedule,
	.schedule_task_cancel	= scheduler_dp_task_cancel,
	.schedule_task_free	= scheduler_dp_task_free,
};

int scheduler_dp_init_secondary_core(void)
{
	if (!dp_sch)
		return -ENOMEM;

	/* register the scheduler instance for secondary core */
	scheduler_init(SOF_SCHEDULE_DP, &schedule_dp_ops, dp_sch);

	return 0;
}

int scheduler_dp_init(void)
{
	dp_sch = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*dp_sch));
	if (!dp_sch)
		return -ENOMEM;

	list_init(&dp_sch->tasks);

	scheduler_init(SOF_SCHEDULE_DP, &schedule_dp_ops, dp_sch);

	notifier_register(NULL, NULL, NOTIFIER_ID_LL_POST_RUN, scheduler_dp_ll_tick, 0);

	return 0;
}

int scheduler_dp_task_init(struct task **task,
			   const struct sof_uuid_entry *uid,
			   const struct task_ops *ops,
			   void *data,
			   uint16_t core,
			   size_t stack_size,
			   uint32_t task_priority)
{
	void __sparse_cache *p_stack = NULL;

	/* memory allocation helper structure */
	struct {
		struct task task;
		struct task_dp_pdata pdata;
		struct k_thread thread;
	} *task_memory;

	k_tid_t thread_id = NULL;
	int ret;

	/*
	 * allocate memory
	 * to avoid multiple malloc operations allocate all required memory as a single structure
	 * and return pointer to task_memory->task
	 */
	task_memory = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*task_memory));
	if (!task_memory) {
		tr_err(&dp_tr, "zephyr_dp_task_init(): memory alloc failed");
		return -ENOMEM;
	}

	/* allocate stack - must be aligned so a separate alloc */
	stack_size = Z_KERNEL_STACK_SIZE_ADJUST(stack_size);
	p_stack = (__sparse_force void __sparse_cache *)
		rballoc_align(0, SOF_MEM_CAPS_RAM, stack_size, Z_KERNEL_STACK_OBJ_ALIGN);
	if (!p_stack) {
		tr_err(&dp_tr, "zephyr_dp_task_init(): stack alloc failed");
		ret = -ENOMEM;
		goto err;
	}

	/* create a zephyr thread for the task */
	thread_id = k_thread_create(&task_memory->thread, (__sparse_force void *)p_stack,
				    stack_size, dp_thread_fn, &task_memory->task, NULL, NULL,
				    task_priority, K_USER, K_FOREVER);
	if (!thread_id)	{
		ret = -EFAULT;
		tr_err(&dp_tr, "zephyr_dp_task_init(): zephyr thread create failed");
		goto err;
	}
	/* pin the thread to specific core */
	ret = k_thread_cpu_pin(thread_id, core);
	if (ret < 0) {
		ret = -EFAULT;
		tr_err(&dp_tr, "zephyr_dp_task_init(): zephyr task pin to core failed");
		goto err;
	}

	/* internal SOF task init */
	ret = schedule_task_init(&task_memory->task, uid, SOF_SCHEDULE_DP, 0, ops->run,
				 data, core, 0);
	if (ret < 0) {
		tr_err(&dp_tr, "zephyr_dp_task_init(): schedule_task_init failed");
		goto err;
	}

	/* initialize other task structures */
	task_memory->task.ops.complete = ops->complete;
	task_memory->task.ops.get_deadline = ops->get_deadline;
	task_memory->task.state = SOF_TASK_STATE_INIT;
	task_memory->task.core = core;

	/* initialize semaprhore */
	k_sem_init(&task_memory->pdata.sem, 0, 1);

	/* success, fill the structures */
	task_memory->task.priv_data = &task_memory->pdata;
	task_memory->pdata.thread_id = thread_id;
	task_memory->pdata.p_stack = p_stack;
	*task = &task_memory->task;

	/* start the thread - it will immediately stop at a semaphore */
	k_thread_start(thread_id);

	return 0;
err:
	/* cleanup - free all allocated resources */
	if (thread_id)
		k_thread_abort(thread_id);
	rfree((__sparse_force void *)p_stack);
	rfree(task_memory);
	return ret;
}
