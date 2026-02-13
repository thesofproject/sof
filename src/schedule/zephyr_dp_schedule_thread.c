// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2025 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Szkudlinski
 */

#include <rtos/task.h>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/common.h>
#include <sof/list.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/trace/trace.h>

#include <zephyr/kernel.h>

#include <stdbool.h>
#include <stdint.h>

#include "zephyr_dp_schedule.h"

LOG_MODULE_DECLARE(dp_schedule, CONFIG_SOF_LOG_LEVEL);
extern struct tr_ctx dp_tr;

/* Go through all DP tasks and recalculate their readiness and deadlines
 * NOT REENTRANT, should be called with scheduler_dp_lock()
 */
void scheduler_dp_recalculate(struct scheduler_dp_data *dp_sch, bool is_ll_post_run)
{
	struct list_item *tlist;
	struct task *curr_task;
	struct task_dp_pdata *pdata;

	list_for_item(tlist, &dp_sch->tasks) {
		curr_task = container_of(tlist, struct task, list);
		pdata = curr_task->priv_data;
		struct processing_module *mod = pdata->mod;
		bool trigger_task = false;

		/* decrease number of LL ticks/cycles left till the module reaches its deadline */
		if (mod->dp_startup_delay && is_ll_post_run && pdata->ll_cycles_to_start) {
			pdata->ll_cycles_to_start--;
			if (!pdata->ll_cycles_to_start)
				/* delayed start complete, clear startup delay flag.
				 * see dp_startup_delay comment for details
				 */
				mod->dp_startup_delay = false;
		}

		if (curr_task->state == SOF_TASK_STATE_QUEUED) {
			bool mod_ready;

			mod_ready = module_is_ready_to_process(mod, mod->sources,
							       mod->num_of_sources,
							       mod->sinks,
							       mod->num_of_sinks);
			if (mod_ready) {
				/* trigger the task */
				curr_task->state = SOF_TASK_STATE_RUNNING;
				if (mod->dp_startup_delay && !pdata->ll_cycles_to_start) {
					/* first time run - use delayed start */
					pdata->ll_cycles_to_start =
						module_get_lpt(pdata->mod) / LL_TIMER_PERIOD_US;

					/* in case LPT < LL cycle - delay at least cycle */
					if (!pdata->ll_cycles_to_start)
						pdata->ll_cycles_to_start = 1;
				}
				trigger_task = true;
				k_event_post(pdata->event, DP_TASK_EVENT_PROCESS);
			}
		}
		if (curr_task->state == SOF_TASK_STATE_RUNNING) {
			/* (re) calculate deadline for all running tasks */
			/* get module deadline in us*/
			uint32_t deadline = module_get_deadline(mod);

			/* if a deadline cannot be calculated, use a fixed value relative to its
			 * first start
			 */
			if (deadline >= UINT32_MAX / 2 && trigger_task)
				deadline = module_get_lpt(mod);

			if (deadline < UINT32_MAX) {
				/* round down to 1ms */
				deadline = deadline / 1000;

				/* calculate number of ticks */
				deadline = deadline * (CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC / 1000);

				/* add to "NOW", overflows are OK  */
				deadline = dp_sch->last_ll_tick_timestamp + deadline;

				/* set in Zephyr. Note that it may be in past, it does not matter,
				 * Zephyr still will schedule the thread with earlier deadline
				 * first
				 */
				k_thread_absolute_deadline_set(pdata->thread_id, deadline);
			}
		}
	}
}

/* Thread function called in component context, on target core */
void dp_thread_fn(void *p1, void *p2, void *p3)
{
	struct task *task = p1;
	(void)p2;
	(void)p3;
	struct task_dp_pdata *task_pdata = task->priv_data;
	struct scheduler_dp_data *dp_sch = NULL;
	unsigned int lock_key;
	enum task_state state;
	bool task_stop;

	if (!(task->flags & K_USER))
		dp_sch = scheduler_get_data(SOF_SCHEDULE_DP);

	do {
		/*
		 * the thread is started immediately after creation, it will stop on event.
		 * Event will be signalled once the task is ready to process.
		 */
		k_event_wait_safe(task_pdata->event, DP_TASK_EVENT_PROCESS | DP_TASK_EVENT_CANCEL,
				  false, K_FOREVER);

		if (task->state == SOF_TASK_STATE_RUNNING)
			state = task_run(task);
		else
			state = task->state;	/* to avoid undefined variable warning */

		lock_key = scheduler_dp_lock(task->core);
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

		/* if true exit the while loop, terminate the thread */
		task_stop = task->state == SOF_TASK_STATE_COMPLETED ||
			task->state == SOF_TASK_STATE_CANCEL;
		/* recalculate all DP tasks readiness and deadlines
		 * TODO: it should be for all tasks, for all cores
		 * currently its limited to current core only
		 */
		if (dp_sch)
			scheduler_dp_recalculate(dp_sch, false);

		scheduler_dp_unlock(lock_key);
	} while (!task_stop);

	/* call task_complete  */
	if (task->state == SOF_TASK_STATE_COMPLETED)
		task_complete(task);
}

int scheduler_dp_task_init(struct task **task,
			   const struct sof_uuid_entry *uid,
			   const struct task_ops *ops,
			   struct processing_module *mod,
			   uint16_t core,
			   size_t stack_size,
			   uint32_t options)
{
	void __sparse_cache *p_stack = NULL;
	struct k_heap *const user_heap = mod->dev->drv->user_heap;

	/* memory allocation helper structure */
	struct {
		struct task task;		/* keep first, used for freeing below */
		struct task_dp_pdata pdata;
	} *task_memory;

	int ret;

	/* must be called on the same core the task will be binded to */
	assert(cpu_get_id() == core);

	/*
	 * allocate memory
	 * to avoid multiple malloc operations allocate all required memory as a single structure
	 * and return pointer to task_memory->task
	 * As the structure contains zephyr kernel specific data, it must be located in
	 * shared, non cached memory
	 */
	task_memory = sof_heap_alloc(user_heap, SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT,
				     sizeof(*task_memory), 0);
	if (!task_memory) {
		tr_err(&dp_tr, "memory alloc failed");
		return -ENOMEM;
	}

	memset(task_memory, 0, sizeof(*task_memory));
	/* allocate stack - must be aligned and cached so a separate alloc */
	p_stack = user_stack_allocate(stack_size, options);
	if (!p_stack) {
		tr_err(&dp_tr, "stack alloc failed");
		ret = -ENOMEM;
		goto err;
	}

	/* internal SOF task init */
	ret = schedule_task_init(&task_memory->task, uid, SOF_SCHEDULE_DP, 0, ops->run,
				 mod, core, options);
	if (ret < 0) {
		tr_err(&dp_tr, "schedule_task_init failed");
		goto err;
	}

	struct task_dp_pdata *pdata = &task_memory->pdata;

	/* Point to event_struct event for kernel threads synchronization */
	/* It will be overwritten for K_USER threads to dynamic ones.  */
	pdata->event = &pdata->event_struct;
	pdata->thread = &pdata->thread_struct;

#ifdef CONFIG_USERSPACE
	if (options & K_USER) {
		pdata->event = k_object_alloc(K_OBJ_EVENT);
		if (!pdata->event) {
			tr_err(&dp_tr, "Event object allocation failed");
			ret = -ENOMEM;
			goto err;
		}

		pdata->thread = k_object_alloc(K_OBJ_THREAD);
		if (!pdata->thread) {
			tr_err(&dp_tr, "Thread object allocation failed");
			ret = -ENOMEM;
			goto err;
		}
	}
#endif /* CONFIG_USERSPACE */

	/* initialize other task structures */
	task_memory->task.ops.complete = ops->complete;
	task_memory->task.ops.get_deadline = ops->get_deadline;
	task_memory->task.state = SOF_TASK_STATE_INIT;
	task_memory->task.core = core;
	task_memory->task.priv_data = pdata;

	/* success, fill the structures */
	pdata->p_stack = p_stack;
	pdata->mod = mod;

	/* create a zephyr thread for the task */
	pdata->thread_id = k_thread_create(pdata->thread, (__sparse_force void *)p_stack,
					   stack_size, dp_thread_fn, &task_memory->task, NULL, NULL,
					   CONFIG_DP_THREAD_PRIORITY, task_memory->task.flags,
					   K_FOREVER);

	k_thread_access_grant(pdata->thread_id, pdata->event);
	scheduler_dp_grant(pdata->thread_id, cpu_get_id());

	/* pin the thread to specific core */
	ret = k_thread_cpu_pin(pdata->thread_id, core);
	if (ret < 0) {
		tr_err(&dp_tr, "zephyr task pin to core failed");
		goto e_thread;
	}

#ifdef CONFIG_USERSPACE
	if (task_memory->task.flags & K_USER) {
		ret = user_memory_init_shared(pdata->thread_id, pdata->mod);
		if (ret < 0) {
			tr_err(&dp_tr, "user_memory_init_shared() failed");
			goto e_thread;
		}
	}
#endif /* CONFIG_USERSPACE */

	/* start the thread, it should immediately stop at an event */
	k_event_init(pdata->event);
	k_thread_start(pdata->thread_id);

	/* success, fill output parameter */
	*task = &task_memory->task;
	return 0;

e_thread:
	k_thread_abort(pdata->thread_id);
err:
	/* cleanup - free all allocated resources */
	if (user_stack_free((__sparse_force void *)p_stack))
		tr_err(&dp_tr, "user_stack_free failed!");

	/* k_object_free looks for a pointer in the list, any invalid value can be passed */
	k_object_free(task_memory->pdata.event);
	k_object_free(task_memory->pdata.thread);
	sof_heap_free(user_heap, task_memory);
	return ret;
}

void scheduler_dp_internal_free(struct task *task)
{
	struct task_dp_pdata *pdata = task->priv_data;

#ifdef CONFIG_USERSPACE
	if (pdata->event != &pdata->event_struct)
		k_object_free(pdata->event);
	if (pdata->thread != &pdata->thread_struct)
		k_object_free(pdata->thread);
#endif

	/* task is the first member in task_memory above */
	sof_heap_free(pdata->mod->dev->drv->user_heap, task);
}
