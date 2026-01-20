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
#include <sof/llext_manager.h>
#include <sof/objpool.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>

#include <zephyr/app_memory/mem_domain.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/slist.h>

#include <stdbool.h>
#include <stdint.h>

#include "zephyr_dp_schedule.h"

LOG_MODULE_DECLARE(dp_schedule, CONFIG_SOF_LOG_LEVEL);
extern struct tr_ctx dp_tr;

static struct objpool_head dp_mdom_head = {.list = LIST_INIT(dp_mdom_head.list)};

/* Synchronization semaphore for the scheduler thread to wait for DP startup */
#define DP_SYNC_INIT(i, _)	Z_SEM_INITIALIZER(dp_sync[i], 0, 1)
#define DP_SYNC_INIT_LIST	LISTIFY(CONFIG_CORE_COUNT, DP_SYNC_INIT, (,))
static STRUCT_SECTION_ITERABLE_ARRAY(k_sem, dp_sync, CONFIG_CORE_COUNT) = { DP_SYNC_INIT_LIST };

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
			struct sof_source *source[CONFIG_MODULE_MAX_CONNECTIONS];
			struct sof_sink *sink[CONFIG_MODULE_MAX_CONNECTIONS];
		} pipeline_state;
	};
};

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
		flat->ret = ops->init(pmod);
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
	}
}

#define DP_THREAD_IPC_TIMEOUT K_MSEC(100)

/* Signal an IPC and wait for processing completion */
int scheduler_dp_thread_ipc(struct processing_module *pmod, unsigned int cmd,
			    const union scheduler_dp_thread_ipc_param *param)
{
	struct task_dp_pdata *pdata = pmod->dev->task->priv_data;
	int ret;

	if (!pmod) {
		tr_err(&dp_tr, "no thread module");
		return -EINVAL;
	}

	if (cmd == SOF_IPC4_MOD_INIT_INSTANCE) {
		/* Wait for the DP thread to start */
		ret = k_sem_take(&dp_sync[pmod->dev->task->core], DP_THREAD_IPC_TIMEOUT);
		if (ret < 0) {
			tr_err(&dp_tr, "Failed waiting for DP thread to start: %d", ret);
			return ret;
		}
	}

	unsigned int lock_key = scheduler_dp_lock(pmod->dev->task->core);

	/* IPCs are serialised */
	pdata->flat->ret = -ENOSYS;

	ret = ipc_thread_flatten(cmd, param, pdata->flat);
	if (!ret)
		k_event_post(pdata->event, DP_TASK_EVENT_IPC);

	scheduler_dp_unlock(lock_key);

	if (!ret) {
		/* Wait for completion */
		ret = k_sem_take(&dp_sync[cpu_get_id()], DP_THREAD_IPC_TIMEOUT);
		if (ret < 0)
			tr_err(&dp_tr, "Failed waiting for DP thread: %d", ret);
		else
			ret = pdata->flat->ret;
	}

	return ret;
}

/* Go through all DP tasks and recalculate their readiness and deadlines
 * NOT REENTRANT, called with scheduler_dp_lock() held
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
			k_event_post(pdata->event, DP_TASK_EVENT_PROCESS);
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

/*
 * Safe to call with partial successful initialisation,
 * k_mem_domain_remove_partition() then just returns -ENOENT
 */
static void scheduler_dp_domain_free(struct task_dp_pdata *pdata)
{
	struct processing_module *pmod = pdata->mod;
	struct k_mem_domain *mdom = pmod->mdom;

	llext_manager_rm_domain(pmod->dev->ipc_config.id, mdom);

	k_mem_domain_remove_partition(mdom, pdata->mpart + SOF_DP_PART_HEAP);
	k_mem_domain_remove_partition(mdom, pdata->mpart + SOF_DP_PART_HEAP_CACHE);
	k_mem_domain_remove_partition(mdom, pdata->mpart + SOF_DP_PART_CFG);
	k_mem_domain_remove_partition(mdom, pdata->mpart + SOF_DP_PART_CFG_CACHE);

	pmod->mdom = NULL;
	objpool_free(&dp_mdom_head, mdom);
}

/* memory allocation helper structure */
struct scheduler_dp_task_memory {
	struct task task;
	struct task_dp_pdata pdata;
	struct comp_driver drv;
	struct ipc4_flat flat;
};

void scheduler_dp_internal_free(struct task *task)
{
	struct task_dp_pdata *pdata = task->priv_data;

	k_object_free(pdata->event);
	k_object_free(pdata->thread);
	scheduler_dp_domain_free(pdata);

	mod_free(pdata->mod, container_of(task, struct scheduler_dp_task_memory, task));
}

/* Called only in IPC context */
int scheduler_dp_task_init(struct task **task, const struct sof_uuid_entry *uid,
			   const struct task_ops *ops, struct processing_module *mod,
			   uint16_t core, size_t stack_size, uint32_t options)
{
	k_thread_stack_t *p_stack;
	struct scheduler_dp_task_memory *task_memory;

	int ret;

	/* must be called on the same core the task will be bound to */
	assert(cpu_get_id() == core);

	/*
	 * allocate memory
	 * to avoid multiple malloc operations allocate all required memory as a single structure
	 * and return pointer to task_memory->task
	 * As the structure contains zephyr kernel specific data, it must be located in
	 * shared, non cached memory
	 */
	task_memory = mod_alloc_ext(mod, SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT,
				    sizeof(*task_memory), 0);
	if (!task_memory) {
		tr_err(&dp_tr, "memory alloc failed");
		return -ENOMEM;
	}

	memset(task_memory, 0, sizeof(*task_memory));

	task_memory->drv = *mod->dev->drv;
	mod->dev->drv = &task_memory->drv;

	/* allocate stack - must be aligned and cached so a separate alloc */
	p_stack = user_stack_allocate(stack_size, options);
	if (!p_stack) {
		tr_err(&dp_tr, "stack alloc failed");
		ret = -ENOMEM;
		goto e_tmem;
	}

	struct task *ptask = &task_memory->task;

	/* internal SOF task init */
	ret = schedule_task_init(ptask, uid, SOF_SCHEDULE_DP, 0, ops->run, mod, core, options);
	if (ret < 0) {
		tr_err(&dp_tr, "schedule_task_init failed");
		goto e_stack;
	}

	struct task_dp_pdata *pdata = &task_memory->pdata;

	pdata->flat = &task_memory->flat;

	pdata->event = k_object_alloc(K_OBJ_EVENT);
	if (!pdata->event) {
		tr_err(&dp_tr, "Event object allocation failed");
		ret = -ENOMEM;
		goto e_stack;
	}

	pdata->thread = k_object_alloc(K_OBJ_THREAD);
	if (!pdata->thread) {
		tr_err(&dp_tr, "Thread object allocation failed");
		ret = -ENOMEM;
		goto e_kobj;
	}
	memset(&pdata->thread->arch, 0, sizeof(pdata->thread->arch));

	/* success, fill the structures */
	pdata->p_stack = p_stack;
	pdata->mod = mod;

	/* initialize other task structures */
	ptask->ops.complete = ops->complete;
	ptask->ops.get_deadline = ops->get_deadline;
	ptask->priv_data = pdata;
	list_init(&ptask->list);
	*task = ptask;

	/* create a zephyr thread for the task */
	pdata->thread_id = k_thread_create(pdata->thread, p_stack,
					   stack_size, dp_thread_fn, ptask, NULL, NULL,
					   CONFIG_DP_THREAD_PRIORITY, ptask->flags, K_FOREVER);

	/* pin the thread to specific core */
	ret = k_thread_cpu_pin(pdata->thread_id, core);
	if (ret < 0) {
		tr_err(&dp_tr, "zephyr task pin to core failed");
		goto e_thread;
	}

	k_thread_access_grant(pdata->thread_id, pdata->event, &dp_sync[core]);
	scheduler_dp_grant(pdata->thread_id, core);

	unsigned int pidx;
	size_t size;
	uintptr_t start;
	struct k_mem_domain *mdom = objpool_alloc(&dp_mdom_head, sizeof(*mdom),
						  SOF_MEM_FLAG_COHERENT);

	if (!mdom) {
		tr_err(&dp_tr, "objpool allocation failed");
		ret = -ENOMEM;
		goto e_thread;
	}

	mod->mdom = mdom;

	if (!mdom->arch.ptables) {
		ret = k_mem_domain_init(mdom, 0, NULL);
		if (ret < 0)
			goto e_dom;
	}

	/* Module heap partition */
	mod_heap_info(mod, &size, &start);
	pdata->mpart[SOF_DP_PART_HEAP] = (struct k_mem_partition){
		.start = start,
		.size = size,
		.attr = K_MEM_PARTITION_P_RW_U_RW,
	};
	pdata->mpart[SOF_DP_PART_HEAP_CACHE] = (struct k_mem_partition){
		.start = (uintptr_t)sys_cache_cached_ptr_get((void *)start),
		.size = size,
		.attr = K_MEM_PARTITION_P_RW_U_RW | XTENSA_MMU_CACHED_WB,
	};
	/* Host mailbox partition for additional IPC parameters: read-only */
	pdata->mpart[SOF_DP_PART_CFG] = (struct k_mem_partition){
		.start = (uintptr_t)sys_cache_uncached_ptr_get((void *)MAILBOX_HOSTBOX_BASE),
		.size = 4096,
		.attr = K_MEM_PARTITION_P_RO_U_RO,
	};
	pdata->mpart[SOF_DP_PART_CFG_CACHE] = (struct k_mem_partition){
		.start = (uintptr_t)MAILBOX_HOSTBOX_BASE,
		.size = 4096,
		.attr = K_MEM_PARTITION_P_RO_U_RO | XTENSA_MMU_CACHED_WB,
	};

	for (pidx = 0; pidx < SOF_DP_PART_TYPE_COUNT; pidx++) {
		ret = k_mem_domain_add_partition(mdom, pdata->mpart + pidx);
		if (ret < 0)
			goto e_dom;
	}

	ret = llext_manager_add_domain(mod->dev->ipc_config.id, mdom);
	if (ret < 0) {
		tr_err(&dp_tr, "failed to add LLEXT to domain %d", ret);
		goto e_dom;
	}

	/*
	 * Keep this call last, able to fail, otherwise domain will be removed
	 * before its thread
	 */
	ret = k_mem_domain_add_thread(mdom, pdata->thread_id);
	if (ret < 0) {
		tr_err(&dp_tr, "failed to add thread to domain %d", ret);
		goto e_dom;
	}

	/* start the thread, it should immediately stop at the semaphore */
	k_event_init(pdata->event);
	k_thread_start(pdata->thread_id);

	return 0;

e_dom:
	scheduler_dp_domain_free(pdata);
e_thread:
	k_thread_abort(pdata->thread_id);
e_kobj:
	/* k_object_free looks for a pointer in the list, any invalid value can be passed */
	k_object_free(pdata->thread);
	k_object_free(pdata->event);
e_stack:
	user_stack_free(p_stack);
e_tmem:
	mod_free(mod, task_memory);
	return ret;
}
