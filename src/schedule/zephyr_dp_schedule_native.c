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
#include <sof/schedule/dp_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/trace/trace.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <stdbool.h>
#include <stdint.h>

#include "zephyr_dp_schedule.h"

LOG_MODULE_DECLARE(dp_schedule, CONFIG_SOF_LOG_LEVEL);
extern struct tr_ctx dp_tr;

/* Synchronization semaphore for the scheduler thread to wait for DP startup */
#define DP_SYNC_INIT(i, _)	Z_SEM_INITIALIZER(dp_sync[i], 0, 1)
#define DP_SYNC_INIT_LIST	LISTIFY(CONFIG_CORE_COUNT, DP_SYNC_INIT, (,))
static STRUCT_SECTION_ITERABLE_ARRAY(k_sem, dp_sync, CONFIG_CORE_COUNT) = { DP_SYNC_INIT_LIST };

/* TODO: make this a shared kernel->module buffer for IPC parameters */
static uint8_t ipc_buf[4096];

struct ipc4_flat {
	unsigned int cmd;
	int ret;
	union {
		struct {
			struct ipc4_module_bind_unbind bu;
			enum bind_type type;
		} bind;
		struct {
			unsigned int trigger_cmd;
			enum ipc4_pipeline_state state;
			int n_sources;
			int n_sinks;
			void *source_sink[];
		} pipeline_state;
	};
};

/* Pack IPC input data */
static int ipc_thread_flatten(unsigned int cmd, union scheduler_dp_thread_ipc_param *param,
			      struct ipc4_flat *flat)
{
	flat->cmd = cmd;

	/*
	 * FIXME: SOF_IPC4_MOD_* and SOF_IPC4_GLB_* aren't fully orthogonal, but
	 * so far none of the used ones overlap
	 */
	switch (cmd) {
	case SOF_IPC4_MOD_BIND:
		flat->bind.bu = *param->bind_data->ipc4_data;
		flat->bind.type = param->bind_data->bind_type;
		break;
	case SOF_IPC4_GLB_SET_PIPELINE_STATE:
		flat->pipeline_state.trigger_cmd = param->pipeline_state.trigger_cmd;
		switch (param->pipeline_state.trigger_cmd) {
		case COMP_TRIGGER_STOP:
			break;
		case COMP_TRIGGER_PREPARE:
			if (sizeof(flat->cmd) + sizeof(flat->ret) + sizeof(flat->pipeline_state) +
			    sizeof(void *) * (param->pipeline_state.n_sources +
					      param->pipeline_state.n_sinks) >
			    sizeof(ipc_buf))
				return -ENOMEM;

			flat->pipeline_state.state = param->pipeline_state.state;
			flat->pipeline_state.n_sources = param->pipeline_state.n_sources;
			flat->pipeline_state.n_sinks = param->pipeline_state.n_sinks;
			memcpy(flat->pipeline_state.source_sink, param->pipeline_state.sources,
			       flat->pipeline_state.n_sources *
			       sizeof(flat->pipeline_state.source_sink[0]));
			memcpy(flat->pipeline_state.source_sink + flat->pipeline_state.n_sources,
			       param->pipeline_state.sinks,
			       flat->pipeline_state.n_sinks *
			       sizeof(flat->pipeline_state.source_sink[0]));
		}
	}

	return 0;
}

/* Unpack IPC data and execute a callback */
static void ipc_thread_unflatten_run(struct processing_module *pmod, struct ipc4_flat *flat)
{
	const struct module_interface *const ops = pmod->dev->drv->adapter_ops;

	switch (flat->cmd) {
	case SOF_IPC4_MOD_BIND:
		if (ops->bind) {
			struct bind_info bind_data = {
				.ipc4_data = &flat->bind.bu,
				.bind_type = flat->bind.type,
			};

			flat->ret = ops->bind(pmod, &bind_data);
		} else {
			flat->ret = 0;
		}
		break;
	case SOF_IPC4_MOD_DELETE_INSTANCE:
		flat->ret = ops->free(pmod);
		break;
	case SOF_IPC4_MOD_INIT_INSTANCE:
		flat->ret = ops->init(pmod);
		break;
	case SOF_IPC4_GLB_SET_PIPELINE_STATE:
		switch (flat->pipeline_state.trigger_cmd) {
		case COMP_TRIGGER_STOP:
			flat->ret = ops->reset(pmod);
			break;
		case COMP_TRIGGER_PREPARE:
			flat->ret = ops->prepare(pmod,
				(struct sof_source **)flat->pipeline_state.source_sink,
				flat->pipeline_state.n_sources,
				(struct sof_sink **)(flat->pipeline_state.source_sink +
						     flat->pipeline_state.n_sources),
				flat->pipeline_state.n_sinks);
		}
	}
}

/* Signal an IPC and wait for processing completion */
int scheduler_dp_thread_ipc(struct processing_module *pmod, enum sof_ipc4_module_type cmd,
			    union scheduler_dp_thread_ipc_param *param)
{
	struct task_dp_pdata *pdata = pmod->dev->task->priv_data;
	int ret;

	if (!pmod) {
		tr_err(&dp_tr, "no thread module");
		return -EINVAL;
	}

	if (cmd == SOF_IPC4_MOD_INIT_INSTANCE) {
		/* Wait for the DP thread to start */
		ret = k_sem_take(&dp_sync[pmod->dev->task->core], K_MSEC(100));
		if (ret == -EAGAIN)
			return -ETIMEDOUT;
	}

	unsigned int lock_key = scheduler_dp_lock(pmod->dev->task->core);

	struct ipc4_flat *flat = (struct ipc4_flat *)ipc_buf;

	/* IPCs are serialised */
	flat->ret = -ENOSYS;

	ret = ipc_thread_flatten(cmd, param, flat);
	if (!ret) {
		pdata->pend_ipc++;
		k_sem_give(pdata->sem);
	}

	scheduler_dp_unlock(lock_key);

	if (!ret) {
		/* Wait for completion */
		k_sem_take(&dp_sync[cpu_get_id()], K_FOREVER);
		ret = flat->ret;
	}

	return ret;
}

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

		if (curr_task->state == SOF_TASK_STATE_QUEUED &&
		    mod->dev->state >= COMP_STATE_ACTIVE) {
			/* trigger the task */
			curr_task->state = SOF_TASK_STATE_RUNNING;
			trigger_task = true;

			pdata->pend_proc++;
			k_sem_give(pdata->sem);
		}
		if (curr_task->state == SOF_TASK_STATE_RUNNING) {
			/* (re) calculate deadline for all running tasks */
			/* get module deadline in us */
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
	struct task_dp_pdata *task_pdata = task->priv_data;
	struct processing_module *pmod = task_pdata->mod;
	unsigned int lock_key;
	enum task_state state;
	bool first = true;
	bool task_stop;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	do {
		if (first) {
			/*
			 * The IPC thread is waiting for the thread to be
			 * started, it can proceed now.
			 */
			first = false;
			k_sem_give(&dp_sync[cpu_get_id()]);
		}

		/*
		 * The thread is started immediately after creation, it stops on
		 * semaphore. Semaphore is signaled to handle IPC or process audio
		 */
		k_sem_take(task_pdata->sem, K_FOREVER);

		lock_key = scheduler_dp_lock(task->core);

		unsigned char pend_ipc = task_pdata->pend_ipc,
			pend_proc = task_pdata->pend_proc;

		task_pdata->pend_proc = 0;
		task_pdata->pend_ipc = 0;

		scheduler_dp_unlock(lock_key);

		/* Only 0:1, 1:0 and 1:1 are valid */
		if (pend_ipc > 1 || pend_proc > 1) {
			tr_err(&dp_tr, "Invalid wake up %u:%u", pend_proc, pend_ipc);
			continue;
		}

		if (pend_ipc) {
			/* handle IPC */
			tr_dbg(&dp_tr, "got IPC wake up for %p state %d", pmod, task->state);
			ipc_thread_unflatten_run(pmod, (struct ipc4_flat *)ipc_buf);
			k_sem_give(&dp_sync[cpu_get_id()]);
		}

		if (pend_proc) {
			bool processing = false;

			if (task->state != SOF_TASK_STATE_RUNNING) {
				state = task->state;    /* to avoid undefined variable warning */
			} else if (module_is_ready_to_process(pmod, pmod->sources,
							      pmod->num_of_sources, pmod->sinks,
							      pmod->num_of_sinks)) {
				if (pmod->dp_startup_delay && !task_pdata->ll_cycles_to_start) {
					/* first time run - use delayed start */
					task_pdata->ll_cycles_to_start =
						module_get_lpt(pmod) / LL_TIMER_PERIOD_US;

					/* in case LPT < LL cycle - delay at least cycle */
					if (!task_pdata->ll_cycles_to_start)
						task_pdata->ll_cycles_to_start = 1;
				}

				processing = true;
				state = task_run(task);
			}

			lock_key = scheduler_dp_lock(task->core);
			/*
			 * check if task is still running, may have been canceled by
			 * external call if not, set the state returned by run procedure
			 */
			if (processing && task->state == SOF_TASK_STATE_RUNNING) {
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
			} else {
				task->state = SOF_TASK_STATE_QUEUED;
			}
		} else {
			lock_key = scheduler_dp_lock(task->core);
		}

		/* if true exit the while loop, terminate the thread */
		task_stop = task->state == SOF_TASK_STATE_COMPLETED ||
			task->state == SOF_TASK_STATE_CANCEL;

		scheduler_dp_unlock(lock_key);
	} while (!task_stop);

	/* call task_complete  */
	if (task->state == SOF_TASK_STATE_COMPLETED)
		task_complete(task);
}
