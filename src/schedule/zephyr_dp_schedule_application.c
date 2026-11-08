// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2025 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Szkudlinski
 */

#include <rtos/task.h>
#include <rtos/userspace_helper.h>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component.h>
#include <sof/common.h>
#include <sof/debug/telemetry/telemetry.h>
#include <sof/ipc/common.h>
#include <sof/lib/mailbox.h>
#include <sof/list.h>
#include <sof/llext_manager.h>
#include <sof/objpool.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/ll_schedule_domain.h>

#include <zephyr/app_memory/mem_domain.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/slist.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <sof/schedule/zephyr_dp_schedule.h>

LOG_MODULE_DECLARE(dp_schedule, CONFIG_SOF_LOG_LEVEL);
extern struct tr_ctx dp_tr;

/* The DP thread creation and the scheduler_dp_internal_free() system call, which run
 * in kernel mode, are in src/kernel/schedule/zephyr_dp_schedule_application-kernel.c
 */

/* Pack IPC input data */
static int ipc_thread_flatten(unsigned int cmd, const union scheduler_dp_thread_ipc_param *param,
			      struct ipc4_flat *flat)
{
	flat->cmd = cmd;

	/*
	 * FIXME: SOF_IPC4_MOD_* and SOF_IPC4_GLB_* aren't fully orthogonal, but
	 * so far none of the used ones overlap
	 */
	switch (cmd) {
	case SOF_IPC4_MOD_BIND:
	case SOF_IPC4_MOD_UNBIND:
		flat->bind.bu = *param->bind_data->ipc4_data;
		flat->bind.type = param->bind_data->bind_type;
		break;
	case SOF_IPC4_MOD_INIT_INSTANCE:
		if (param->ext_data)
			flat->init_instance = *param->ext_data;
		else
			flat->init_instance = (struct module_ext_init_data){ 0 };
		break;
	case SOF_IPC4_GLB_SET_PIPELINE_STATE:
		flat->pipeline_state.trigger_cmd = param->pipeline_state.trigger_cmd;
		switch (param->pipeline_state.trigger_cmd) {
		case COMP_TRIGGER_STOP:
			break;
		case COMP_TRIGGER_PREPARE:
			if (param->pipeline_state.n_sources > CONFIG_MODULE_MAX_CONNECTIONS ||
			    param->pipeline_state.n_sinks > CONFIG_MODULE_MAX_CONNECTIONS)
				return -ENOMEM;

			flat->pipeline_state.state = param->pipeline_state.state;
			flat->pipeline_state.n_sources = param->pipeline_state.n_sources;
			flat->pipeline_state.n_sinks = param->pipeline_state.n_sinks;
			/* Up to 2 * CONFIG_MODULE_MAX_CONNECTIONS */
			memcpy(flat->pipeline_state.source, param->pipeline_state.sources,
			       flat->pipeline_state.n_sources *
			       sizeof(flat->pipeline_state.source[0]));
			memcpy(flat->pipeline_state.sink, param->pipeline_state.sinks,
			       flat->pipeline_state.n_sinks *
			       sizeof(flat->pipeline_state.sink[0]));
		}
		break;
	case SOF_IPC4_MOD_LARGE_CONFIG_SET:
		flat->set_config.param_id = param->set_config.param_id;
		flat->set_config.position = param->set_config.position;
		flat->set_config.data_offset_size = param->set_config.data_offset_size;
		flat->set_config.fragment_size = param->set_config.fragment_size;
		flat->set_config.data = param->set_config.data;
		break;
	case SOF_IPC4_MOD_LARGE_CONFIG_GET:
		flat->get_config.param_id = param->get_config.param_id;
		flat->get_config.data_offset_size = param->get_config.data_offset_size;
		flat->get_config.fragment_size = param->get_config.fragment_size;
		flat->get_config.data = NULL/*param->get_config.data*/;
	}

	return 0;
}

/* Unpack IPC data and execute a callback */
static void ipc_thread_unflatten_run(struct processing_module *pmod, struct ipc4_flat *flat)
{
	const struct module_interface *const ops = pmod->dev->drv->adapter_ops;
	struct scheduler_dp_task_memory *task_mem = container_of(flat,
								 struct scheduler_dp_task_memory,
								 flat);

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
	case SOF_IPC4_MOD_UNBIND:
		if (ops->unbind) {
			struct bind_info bind_data = {
				.ipc4_data = &flat->bind.bu,
				.bind_type = flat->bind.type,
			};

			flat->ret = ops->unbind(pmod, &bind_data);
		} else {
			flat->ret = 0;
		}
		break;
	case SOF_IPC4_MOD_DELETE_INSTANCE:
		flat->ret = ops->free(pmod);
		break;
	case SOF_IPC4_MOD_INIT_INSTANCE:
		/*
		 * Repoint ext_data at the copy ipc_thread_flatten() made in DP-thread-
		 * accessible memory; the original caller-stack copy is out of reach here.
		 */
		pmod->priv.cfg.ext_data = &flat->init_instance;
		flat->ret = ops->init(pmod);
		pmod->priv.cfg.ext_data = NULL;
		break;
	case SOF_IPC4_GLB_SET_PIPELINE_STATE:
		switch (flat->pipeline_state.trigger_cmd) {
		case COMP_TRIGGER_STOP:
			flat->ret = ops->reset(pmod);
			break;
		case COMP_TRIGGER_PREPARE:
			flat->ret = ops->prepare(pmod,
						 flat->pipeline_state.source,
						 flat->pipeline_state.n_sources,
						 flat->pipeline_state.sink,
						 flat->pipeline_state.n_sinks);
		}
		break;
	case SOF_IPC4_MOD_LARGE_CONFIG_SET:
		flat->ret = ops->set_configuration(pmod, flat->set_config.param_id,
						   flat->set_config.position,
						   flat->set_config.data_offset_size,
						   (const uint8_t *)flat->set_config.data,
						   flat->set_config.fragment_size, NULL, 0);
		break;
	case SOF_IPC4_MOD_LARGE_CONFIG_GET:
		flat->ret = ops->get_configuration(pmod, flat->get_config.param_id,
						   &flat->get_config.data_offset_size,
						   task_mem->pdata.ipc_config_data,
						   flat->get_config.fragment_size);
		if (!flat->ret)
			flat->get_config.data = task_mem->pdata.ipc_config_data;
	}
}

#define DP_THREAD_IPC_TIMEOUT K_MSEC(100)

/* Signal an IPC and wait for processing completion */
int scheduler_dp_thread_ipc(struct processing_module *pmod, unsigned int cmd,
			    union scheduler_dp_thread_ipc_param *param)
{
	if (!pmod) {
		tr_err(&dp_tr, "no thread module");
		return -EINVAL;
	}

	struct task_dp_pdata *pdata = pmod->dev->task->priv_data;
	unsigned int core = pmod->dev->task->core;
	int ret;

	if (cmd == SOF_IPC4_MOD_INIT_INSTANCE) {
		/* Wait for the DP thread to start */
		ret = k_sem_take(&dp_sync[core], DP_THREAD_IPC_TIMEOUT);
		if (ret < 0) {
			tr_err(&dp_tr, "Failed waiting for DP thread to start: %d", ret);
			return ret;
		}
	}

	unsigned int lock_key = scheduler_dp_lock(core);

	/* IPCs are serialised */
	pdata->flat->ret = -ENOSYS;

	ret = ipc_thread_flatten(cmd, param, pdata->flat);
	if (!ret)
		k_event_post(pdata->event, DP_TASK_EVENT_IPC);

	scheduler_dp_unlock(lock_key);

	if (ret < 0)
		return ret;

	/* Wait for completion */
	ret = k_sem_take(&dp_sync[core], DP_THREAD_IPC_TIMEOUT);
	if (ret < 0) {
		tr_err(&dp_tr, "Failed waiting for DP thread: %d", ret);
		return ret;
	}

	if (!pdata->flat->ret && cmd == SOF_IPC4_MOD_LARGE_CONFIG_GET) {
		/* A single case of returning data */
		param->get_config.data_offset_size = pdata->flat->get_config.data_offset_size;
		return memcpy_s(param->get_config.data, SOF_IPC_MSG_MAX_SIZE,
				pdata->flat->get_config.data, param->get_config.fragment_size);
	}

	return pdata->flat->ret;
}

/* Go through all DP tasks and recalculate their readiness and deadlines
 * NOT REENTRANT, called with scheduler_dp_lock() held
 */
void scheduler_dp_recalculate(struct scheduler_dp_data *dp_sch)
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
		if (mod->dp_startup_delay && pdata->ll_cycles_to_start) {
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
			k_event_post(pdata->event, DP_TASK_EVENT_PROCESS);
		}

		if (curr_task->state == SOF_TASK_STATE_RUNNING) {
			/* (re) calculate deadline for all running tasks */
			/* get module deadline in us */
			uint32_t deadline = module_get_deadline(mod);

			if (!trigger_task && !mod->dp_startup_delay &&
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

/* Thread function called in component context, on target core */
void dp_thread_fn(void *p1, void *p2, void *p3)
{
	struct task *task = p1;
	struct task_dp_pdata *task_pdata = task->priv_data;
	struct processing_module *pmod = task_pdata->mod;
	unsigned int lock_key;
	enum task_state state;
	bool task_stop;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	/* The IPC thread is waiting for the thread to be started, it can proceed now. */
	k_sem_give(&dp_sync[task->core]);
	comp_info(pmod->dev, "userspace thread started");

	do {
		uint32_t mask = k_event_wait_safe(task_pdata->event,
						  DP_TASK_EVENT_PROCESS | DP_TASK_EVENT_CANCEL |
						  DP_TASK_EVENT_IPC, false, K_FOREVER);

		if (mask & DP_TASK_EVENT_IPC) {
			/* handle IPC */
			tr_dbg(&dp_tr, "got IPC wake up for %p state %d", pmod, task->state);
			ipc_thread_unflatten_run(pmod, task_pdata->flat);
			k_sem_give(&dp_sync[task->core]);
		}

		if (mask & DP_TASK_EVENT_PROCESS) {
			bool ready;

			if (task->state == SOF_TASK_STATE_RUNNING) {
				ready = module_is_ready_to_process(pmod, pmod->sources,
								   pmod->num_of_sources,
								   pmod->sinks, pmod->num_of_sinks);
			} else {
				state = task->state;	/* to avoid undefined variable warning */
				ready = false;
			}

			if (ready) {
				if (pmod->dp_startup_delay && !task_pdata->ll_cycles_to_start) {
					/* first time run - use delayed start */
					task_pdata->ll_cycles_to_start =
						module_get_lpt(pmod) / LL_TIMER_PERIOD_US;

					/* in case LPT < LL cycle - delay at least cycle */
					if (!task_pdata->ll_cycles_to_start)
						task_pdata->ll_cycles_to_start = 1;
				}

				state = task_run(task);
			}

			lock_key = scheduler_dp_lock(task->core);
			/*
			 * check if task is still running, may have been canceled by external call
			 * if not, set the state returned by run procedure
			 */
			if (ready && task->state == SOF_TASK_STATE_RUNNING) {
				if (!pmod->dp_startup_delay &&
				    task_pdata->deadline_clock_ticks)
					scheduler_dp_check_deadline(task->core,
								    task_pdata->deadline_clock_ticks);
				task_pdata->deadline_clock_ticks = 0;
				task->state = state;
				switch (state) {
				case SOF_TASK_STATE_RESCHEDULE:
					/* mark to reschedule, schedule time is already calculated */
					task->state = SOF_TASK_STATE_QUEUED;
					break;

				case SOF_TASK_STATE_CANCEL:
				case SOF_TASK_STATE_COMPLETED:
					/* task already removed from scheduling */
					break;

				default:
					/* illegal state, serious defect, won't happen */
					k_oops();
				}
			} else {
				task_pdata->deadline_clock_ticks = 0;
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
