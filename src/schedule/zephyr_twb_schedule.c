// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Bonislawski <adrian.bonislawski@intel.com>
 */

#include <rtos/task.h>
#include <stdint.h>
#include <sof/lib/uuid.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/twb_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/trace/trace.h>
#include <rtos/wait.h>
#include <rtos/interrupt.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/kernel/thread.h>
#include <zephyr/sys_clock.h>

LOG_MODULE_REGISTER(twb_schedule, CONFIG_SOF_LOG_LEVEL);
SOF_DEFINE_REG_UUID(twb_sched);

DECLARE_TR_CTX(twb_tr, SOF_UUID(twb_sched_uuid), LOG_LEVEL_INFO);

struct scheduler_twb_data {
	struct list_item tasks;		/* list of active TWB tasks */
	struct task ll_tick_src;	/* LL task - source of TWB tick */
};

struct task_twb_data {
	k_tid_t thread_id;		/* zephyr thread ID */
	k_thread_stack_t __sparse_cache *p_stack; /* pointer to thread stack */
	struct k_sem sem;		/* thread semaphore */
	int32_t thread_prio;		/* thread default priority */
	uint32_t cycles_granted;	/* cycles budget for the task */
	uint32_t cycles_consumed;	/* cycles consumed by the task */
	uint64_t cycles_ref;		/* reference cycles for the task */
};

/* Single CPU-wide lock
 * as each per-core instance if TWB-scheduler has separate structures, it is enough to
 * use irq_lock instead of cross-core spinlocks
 */
static inline unsigned int scheduler_twb_lock(void)
{
	return irq_lock();
}

static inline void scheduler_twb_unlock(unsigned int key)
{
	irq_unlock(key);
}

/**
 * @brief Callback function for the TWB scheduler task.
 *
 * This function is called when the TWB scheduler task runs out of budget.
 * It lowers the priority of the thread and sets the time slice to 0.
 *
 * @param thread Pointer to the thread structure.
 * @param data   Pointer to additional data (not used).
 */
static void scheduler_twb_task_cb(struct k_thread *thread, void *data)
{
	tr_dbg(&twb_tr, "TWB task %p out of budget, lowering priority", data);

	k_thread_priority_set(thread, CONFIG_TWB_THREAD_LOW_PRIORITY);
	k_thread_time_slice_set(thread, 0, &scheduler_twb_task_cb, data);
}

/**
 * @brief Executes the LL tick of the TWB scheduler.
 *
 * This function is responsible for executing the LL tick of the TWB scheduler.
 * It iterates through the list of tasks and performs the necessary operations
 * based on the task's state.
 * If a task is in the QUEUED state, it is transitioned to the RUNNING state
 * and its associated thread is resumed.
 * If a task is in the RUNNING state, its thread's priority and time slice
 * are set based on the task's budget.
 * The function also retrieves the runtime statistics of the thread and
 * updates the task's reference cycle count.
 * If a task is in the CANCEL or COMPLETED state, it is removed from the list.
 *
 * @param data Pointer to the scheduler_twb_data structure.
 * @return The state of the task after the LL tick execution.
 *         If no task requires further execution, SOF_TASK_STATE_COMPLETED is returned.
 *         Otherwise, SOF_TASK_STATE_RESCHEDULE is returned.
 */
static enum task_state scheduler_twb_ll_tick(void *data)
{
	struct scheduler_twb_data *twb_sch = data;
	k_thread_runtime_stats_t rt_stats_thread;
	struct list_item *tlist, *tmp;
	struct task_twb_data *pdata;
	struct task *curr_task;
	unsigned int lock_key;
	bool keep_ll_tick_src = false;

	lock_key = scheduler_twb_lock();

	/* Iterate through the list of tasks */
	list_for_item_safe(tlist, tmp, &twb_sch->tasks) {
		curr_task = container_of(tlist, struct task, list);
		pdata = curr_task->priv_data;

		/* Reset consumed cycles */
		pdata->cycles_consumed = 0;

		switch (curr_task->state) {
		case SOF_TASK_STATE_QUEUED:
			curr_task->state = SOF_TASK_STATE_RUNNING;
			k_sem_give(&pdata->sem);
			COMPILER_FALLTHROUGH;
		case SOF_TASK_STATE_RUNNING:
			if (pdata->cycles_granted) {
				/* Reset thread's priority and time slice based on task's budget */
				k_thread_priority_set(pdata->thread_id, pdata->thread_prio);
				k_thread_time_slice_set(pdata->thread_id, pdata->cycles_granted,
							&scheduler_twb_task_cb, curr_task);

				/* Retrieve runtime statistics of the thread & update ref cycle */
				k_thread_runtime_stats_get(pdata->thread_id, &rt_stats_thread);
				pdata->cycles_ref = rt_stats_thread.execution_cycles;
			}
			keep_ll_tick_src = true;
			break;
		case SOF_TASK_STATE_CANCEL:
		case SOF_TASK_STATE_COMPLETED:
			/* Finally remove task from the list */
			list_item_del(&curr_task->list);
			break;
		default:
			break;
		}
	}

	scheduler_twb_unlock(lock_key);

	if (!keep_ll_tick_src)
		return SOF_TASK_STATE_COMPLETED;

	return SOF_TASK_STATE_RESCHEDULE;
}

/**
 * @brief Thread function for a TWB task.
 *
 * This function is responsible for executing the TWB task in a loop.
 * It checks the state of the task, runs the task if it is in the running state,
 * and handles different task states such as rescheduling, cancellation, and completion.
 * It also updates the runtime statistics of the thread and suspends the
 * thread if the task is not in the running state.
 *
 * @param p1 Pointer to the task structure.
 * @param p2 Unused parameter.
 * @param p3 Unused parameter.
 */
static void twb_thread_fn(void *p1, void *p2, void *p3)
{
	struct task *task = p1;
	struct task *ll_tick_src = p2;
	(void)p3;
	struct task_twb_data *pdata = task->priv_data;
	k_thread_runtime_stats_t rt_stats_thread;
	enum task_state state;
	unsigned int lock_key;

	while (1) {
		if (ll_tick_src->state == SOF_TASK_STATE_INIT || ll_tick_src->state == SOF_TASK_STATE_FREE)
			schedule_task(ll_tick_src, 0, 0);

		if (task->state == SOF_TASK_STATE_RUNNING) {
			state = task_run(task);
		} else {
			state = task->state;	/* to avoid undefined variable warning */
		}

		lock_key = scheduler_twb_lock();
		/*
		 * check if task is still running, may have been canceled by external call
		 * if not, set the state returned by run procedure
		 */
		if (task->state == SOF_TASK_STATE_RUNNING) {
			if (pdata->cycles_granted) {
				k_thread_runtime_stats_get(pdata->thread_id, &rt_stats_thread);
				pdata->cycles_consumed += rt_stats_thread.execution_cycles - pdata->cycles_ref;
				pdata->cycles_ref = rt_stats_thread.execution_cycles;
			}
			switch (state) {
			case SOF_TASK_STATE_RESCHEDULE:
				/* mark to reschedule, schedule time is already calculated */
				task->state = SOF_TASK_STATE_QUEUED;
				break;
			case SOF_TASK_STATE_CANCEL:
				task->state = SOF_TASK_STATE_CANCEL;
				break;
			case SOF_TASK_STATE_COMPLETED:
				break;

			default:
				/* illegal state, serious defect, won't happen */
				scheduler_twb_unlock(lock_key);
				k_panic();
			}
		}

		scheduler_twb_unlock(lock_key);

		if (state == SOF_TASK_STATE_COMPLETED) {
			task->state = SOF_TASK_STATE_COMPLETED;
			task_complete(task);
		}

		if (state != SOF_TASK_STATE_RUNNING)
			k_sem_take(&pdata->sem, K_FOREVER);
	};
	/* never be here */
}

/**
 * Schedule a task in the TWB scheduler.
 *
 * This function adds a task to the TWB scheduler list,
 * recalculate budget and starts the thread associated with the task.
 * If there are no TWB tasks scheduled yet, it also runs the LL tick source task.
 *
 * @param data Pointer to the TWB scheduler data.
 * @param task Pointer to the task to be scheduled.
 * @param start The start time of the task.
 * @param period The period of the task.
 * @return 0 on success, or a negative error code on failure.
 */
static int scheduler_twb_task_shedule(void *data, struct task *task, uint64_t start,
				      uint64_t period)
{
	struct scheduler_twb_data *twb_sch = (struct scheduler_twb_data *)data;
	struct task_twb_data *pdata = task->priv_data;
	struct list_item *tlist;
	struct task *task_iter;
	unsigned int lock_key;
	bool list_prepend = true;
	bool thread_started = true;
	uint32_t budget_left = 0;

	lock_key = scheduler_twb_lock();

	if (task->state != SOF_TASK_STATE_INIT &&
	    task->state != SOF_TASK_STATE_CANCEL &&
	    task->state != SOF_TASK_STATE_COMPLETED) {
		scheduler_twb_unlock(lock_key);
		return -EINVAL;
	} else if (task->state == SOF_TASK_STATE_INIT) {
		thread_started = false;
	}

	/* add a task to TWB scheduler list */
	task->state = SOF_TASK_STATE_RUNNING;

	/* if there's no TWB tasks scheduled yet, run ll tick source task */
	if (list_is_empty(&twb_sch->tasks)) {
		if (!k_is_in_isr())
			schedule_task(&twb_sch->ll_tick_src, 0, 0);
	} else {
		list_for_item(tlist, &twb_sch->tasks) {
			task_iter = container_of(tlist, struct task, list);
			if (task == task_iter) {
				list_prepend = false;
				break;
			}
		}
	}

	if (list_prepend)
		list_item_prepend(&task->list, &twb_sch->tasks);

	/* If the task has a cycles budget, calculate the budget_left and set the thread priority */
	if (pdata->cycles_granted) {
		if (pdata->cycles_consumed < SYS_TICKS_TO_HW_CYCLES(pdata->cycles_granted)) {
			budget_left = HW_CYCLES_TO_SYS_TICKS(SYS_TICKS_TO_HW_CYCLES(pdata->cycles_granted) - pdata->cycles_consumed);
			k_thread_priority_set(pdata->thread_id, pdata->thread_prio);
		} else {
			k_thread_priority_set(pdata->thread_id, CONFIG_TWB_THREAD_LOW_PRIORITY);
		}
		k_thread_time_slice_set(pdata->thread_id, budget_left, &scheduler_twb_task_cb, task);
	}

	tr_dbg(&twb_tr, "TWB task %p scheduled with budget %d/%d", task, budget_left, pdata->cycles_granted);

	/* start the thread */
	if (!thread_started)
		k_thread_start(pdata->thread_id);
	else
		k_sem_give(&pdata->sem);

	scheduler_twb_unlock(lock_key);

	return 0;
}

static int scheduler_twb_task_cancel(void *data, struct task *task)
{
	struct task_twb_data *task_pdata = task->priv_data;
	k_thread_runtime_stats_t rt_stats_thread;
	unsigned int lock_key;

	lock_key = scheduler_twb_lock();

	if (task_pdata->cycles_granted) {
		/* Get the stats and update the consumed cycles */
		k_thread_runtime_stats_get(task_pdata->thread_id, &rt_stats_thread);
		task_pdata->cycles_consumed += rt_stats_thread.execution_cycles - task_pdata->cycles_ref;
		task_pdata->cycles_ref = rt_stats_thread.execution_cycles;
	}

	/* Set the task state to CANCEL */
	task->state = SOF_TASK_STATE_CANCEL;

	scheduler_twb_unlock(lock_key);

	return 0;
}

static int scheduler_twb_task_free(void *data, struct task *task)
{
	struct task_twb_data *pdata = task->priv_data;

	scheduler_twb_task_cancel(data, task);

	list_item_del(&task->list);

	/* abort the execution of the thread */
	k_thread_abort(pdata->thread_id);

	/* free task stack */
	rfree((__sparse_force void *)pdata->p_stack);

	/* all other memory has been allocated as a single malloc, will be freed later by caller */
	return 0;
}

static struct scheduler_ops schedule_twb_ops = {
	.schedule_task		= scheduler_twb_task_shedule,
	.schedule_task_cancel	= scheduler_twb_task_cancel,
	.schedule_task_free	= scheduler_twb_task_free,
};

int scheduler_twb_init(void)
{
	struct scheduler_twb_data *twb_sch = rzalloc(SOF_MEM_FLAG_KERNEL,
						   sizeof(struct scheduler_twb_data));
	int ret;

	if (!twb_sch)
		return -ENOMEM;

	list_init(&twb_sch->tasks);

	scheduler_init(SOF_SCHEDULE_TWB, &schedule_twb_ops, twb_sch);

	/* init src of TWB tick */
	ret = schedule_task_init_ll(&twb_sch->ll_tick_src,
				    SOF_UUID(twb_sched_uuid),
				    SOF_SCHEDULE_LL_TIMER,
				    0, scheduler_twb_ll_tick, twb_sch,
				    cpu_get_id(), 0);

	return ret;
}

int scheduler_twb_task_init(struct task **task,
			    const struct sof_uuid_entry *uid,
			    const struct task_ops *ops,
			    void *data,
			    int32_t core,
			    const char *name,
			    size_t stack_size,
			    int32_t thread_priority,
			    uint32_t cycles_granted)
{
	struct scheduler_twb_data *twb_sch;
	void __sparse_cache *p_stack = NULL;
	k_tid_t thread_id = NULL;
	int ret;

	/* memory allocation helper structure */
	struct {
		struct task task;
		struct task_twb_data pdata;
		struct k_thread thread;
	} *task_memory;

	twb_sch = scheduler_get_data(SOF_SCHEDULE_TWB);
	if (!twb_sch) {
		tr_err(&twb_tr, "TWB not initialized");
		return -EINVAL;
	}

	/* must be called on the same core the task will be binded to */
	assert(cpu_get_id() == core);

	if (thread_priority < 0) {
		tr_err(&twb_tr, "non preemptible priority");
		return -EINVAL;
	}

	/*
	 * allocate memory
	 * to avoid multiple malloc operations allocate all required memory as a single structure
	 * and return pointer to task_memory->task
	 * As the structure contains zephyr kernel specific data, it must be located in
	 * shared, non cached memory
	 */
	task_memory = rzalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT,
			      sizeof(*task_memory));
	if (!task_memory) {
		tr_err(&twb_tr, "memory alloc failed");
		return -ENOMEM;
	}

	/* allocate stack - must be aligned and cached so a separate alloc */
	stack_size = Z_KERNEL_STACK_SIZE_ADJUST(stack_size);
	p_stack = (__sparse_force void __sparse_cache *)
		rballoc_align(SOF_MEM_FLAG_KERNEL, stack_size, Z_KERNEL_STACK_OBJ_ALIGN);
	if (!p_stack) {
		tr_err(&twb_tr, "stack alloc failed");
		ret = -ENOMEM;
		goto err;
	}

	/* create a zephyr thread for the task */
	thread_id = k_thread_create(&task_memory->thread, (__sparse_force void *)p_stack,
				    stack_size, twb_thread_fn, &task_memory->task, &twb_sch->ll_tick_src, NULL,
				    thread_priority, K_USER, K_FOREVER);
	if (!thread_id)	{
		ret = -EFAULT;
		tr_err(&twb_tr, "zephyr thread create failed");
		goto err;
	}

	/* pin the thread to specific core */
	ret = k_thread_cpu_pin(thread_id, core);
	if (ret < 0) {
		ret = -EFAULT;
		tr_err(&twb_tr, "zephyr task pin to core %d failed", core);
		goto err;
	}

	/* set the thread name */
	if (name) {
		ret = k_thread_name_set(thread_id, name);
		if (ret < 0)
			tr_warn(&twb_tr, "failed to set thread name");
	}

	/* internal SOF task init */
	ret = schedule_task_init(&task_memory->task, uid, SOF_SCHEDULE_TWB, thread_priority,
				 ops->run, data, core, 0);
	if (ret < 0) {
		tr_err(&twb_tr, "schedule_task_init failed");
		goto err;
	}

	/* unlimited mcps budget */
	if (cycles_granted >= ZEPHYR_TWB_BUDGET_MAX)
		cycles_granted = 0;

	/* initialize other task structures */
	task_memory->task.ops.complete = ops->complete;
	task_memory->task.ops.get_deadline = ops->get_deadline;

	/* success, fill the structures */
	task_memory->task.priv_data = &task_memory->pdata;
	task_memory->pdata.thread_id = thread_id;
	task_memory->pdata.p_stack = p_stack;
	task_memory->pdata.thread_prio = thread_priority;
	task_memory->pdata.cycles_granted = cycles_granted;
	task_memory->pdata.cycles_consumed = 0;
	task_memory->pdata.cycles_ref = 0;
	*task = &task_memory->task;

	k_sem_init(&task_memory->pdata.sem, 0, 10);

	tr_dbg(&twb_tr, "TWB task %p initialized: thread: %p, core: %d, prio: %d, budget: %d",
	       task, thread_id, core, thread_priority, cycles_granted);

	return 0;
err:
	/* cleanup - free all allocated resources */
	if (thread_id)
		k_thread_abort(thread_id);

	rfree((__sparse_force void *)p_stack);
	rfree(task_memory);
	return ret;
}
