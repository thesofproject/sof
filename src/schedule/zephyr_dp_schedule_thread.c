// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2025 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Szkudlinski
 *	   Adrian Warecki
 */

#include <rtos/task.h>
#include <rtos/userspace_helper.h>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/module_adapter/library/userspace_proxy.h>
#include <sof/audio/module_adapter/library/userspace_proxy_user.h>
#include <sof/common.h>
#include <sof/debug/telemetry/telemetry.h>
#include <sof/list.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/trace/trace.h>
#include <sof/schedule/schedule.h>

#include <zephyr/kernel.h>

#include <stdbool.h>
#include <stdint.h>

#include <sof/schedule/zephyr_dp_schedule.h>

LOG_MODULE_DECLARE(dp_schedule, CONFIG_SOF_LOG_LEVEL);
extern struct tr_ctx dp_tr;

/* The DP thread creation and the scheduler_dp_internal_free() system call, which run
 * in kernel mode, are in src/kernel/schedule/zephyr_dp_schedule_thread-kernel.c
 */

/* Go through all DP tasks and recalculate their readiness and deadlines
 * NOT REENTRANT, should be called with scheduler_dp_lock()
 */
static void scheduler_dp_recalculate_thread(struct scheduler_dp_data *dp_sch, bool is_ll_post_run)
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

			if (!trigger_task && is_ll_post_run && !mod->dp_startup_delay &&
			    pdata->deadline_clock_ticks &&
			    (deadline == 0 ||
			     (int32_t)(dp_sch->last_ll_tick_timestamp -
				       pdata->deadline_clock_ticks) > 0)) {
				telemetry_notify_dp_deadline_miss(curr_task->core);
				pdata->deadline_clock_ticks = 0;
			}

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
				if (trigger_task)
					pdata->deadline_clock_ticks = deadline ? deadline : 1U;

				/* set in Zephyr. Note that it may be in past, it does not matter,
				 * Zephyr still will schedule the thread with earlier deadline
				 * first
				 */
				k_thread_absolute_deadline_set(pdata->thread_id, deadline);
			}
		}
	}
}

void scheduler_dp_recalculate(struct scheduler_dp_data *dp_sch)
{
	scheduler_dp_recalculate_thread(dp_sch, true);
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
	bool task_stop = false;
	uint32_t event;

	if (!(task->flags & K_USER))
		dp_sch = scheduler_get_data(SOF_SCHEDULE_DP);

	do {
		/*
		 * the thread is started immediately after creation, it will stop on event.
		 * Event will be signalled once the task is ready to process.
		 */
		event = k_event_wait_safe(task_pdata->event, DP_TASK_EVENT_PROCESS |
					  DP_TASK_EVENT_CANCEL | DP_TASK_EVENT_IPC, false,
					  K_FOREVER);

#if IS_ENABLED(CONFIG_SOF_USERSPACE_MOD_IPC_BY_DP_THREAD)
		if (event & DP_TASK_EVENT_IPC) {
			assert(task_pdata->ipc_work_item);
			userspace_proxy_worker_handler(task_pdata->ipc_work_item);
		}
#endif

		if (event & DP_TASK_EVENT_PROCESS) {
			state = task->state;	/* to avoid undefined variable warning */
			if (task->state == SOF_TASK_STATE_RUNNING && event & DP_TASK_EVENT_PROCESS)
				state = task_run(task);

			lock_key = scheduler_dp_lock(task->core);
			/*
			 * check if task is still running, may have been canceled by external call
			 * if not, set the state returned by run procedure
			 */
			if (task->state == SOF_TASK_STATE_RUNNING) {
				if (!task_pdata->mod->dp_startup_delay &&
				    task_pdata->deadline_clock_ticks)
					scheduler_dp_check_deadline(task->core,
								    task_pdata->deadline_clock_ticks);
				task_pdata->deadline_clock_ticks = 0;
				task->state = state;
				switch (state) {
				case SOF_TASK_STATE_RESCHEDULE:
					/* mark to reschedule, schedule time is already calculated
					 */
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
				scheduler_dp_recalculate_thread(dp_sch, false);

			scheduler_dp_unlock(lock_key);
		}

		if (event & DP_TASK_EVENT_CANCEL)
			task_stop = true;
	} while (!task_stop);

	/* call task_complete  */
	if (task->state == SOF_TASK_STATE_COMPLETED)
		task_complete(task);
}
