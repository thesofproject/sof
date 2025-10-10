// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Szkudlinski
 */

#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/llext_manager.h>
#include <rtos/task.h>
#include <stdint.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/trace/trace.h>
#include <rtos/wait.h>
#include <rtos/interrupt.h>
#include <rtos/spinlock.h>
#include <zephyr/kernel.h>
#include <zephyr/sys_clock.h>
#include <sof/lib/notifier.h>
#include <ipc4/base_fw.h>

#include <zephyr/kernel/thread.h>

LOG_MODULE_REGISTER(dp_schedule, CONFIG_SOF_LOG_LEVEL);
SOF_DEFINE_REG_UUID(dp_sched);

DECLARE_TR_CTX(dp_tr, SOF_UUID(dp_sched_uuid), LOG_LEVEL_INFO);

// FIXME: should be per-DP task
K_SEM_DEFINE(dp_thread_ipc_sem, 0, K_SEM_MAX_LIMIT);
K_SEM_DEFINE(dp_thread_sync_sem, 0, 1);

static struct k_spinlock thread_lock;
static struct k_mem_domain dp_mdom;

struct scheduler_dp_data {
	struct list_item tasks;		/* list of active dp tasks */
	struct task ll_tick_src;	/* LL task - source of DP tick */
};

enum sof_dp_part_type {
	SOF_DP_PART_HEAP,
	SOF_DP_PART_IPC,
	SOF_DP_PART_CFG,
	SOF_DP_PART_TYPE_COUNT,
};

struct task_dp_pdata {
	k_tid_t thread_id;		/* zephyr thread ID */
	struct k_thread *thread;	/* memory space for a thread */
	uint32_t deadline_clock_ticks;	/* dp module deadline in Zephyr ticks */
	k_thread_stack_t *p_stack;	/* pointer to thread stack */
	struct processing_module *mod;	/* the module to be scheduled */
	uint32_t ll_cycles_to_start;    /* current number of LL cycles till delayed start */
	unsigned char pend_ipc;
	unsigned char pend_proc;
	struct k_mem_partition mpart[SOF_DP_PART_TYPE_COUNT];
};

/* Single CPU-wide lock
 * as each per-core instance if dp-scheduler has separate structures, it is enough to
 * use irq_lock instead of cross-core spinlocks
 */
static inline unsigned int scheduler_dp_lock(void)
{
	return irq_lock();
}

static inline void scheduler_dp_unlock(unsigned int key)
{
	irq_unlock(key);
}

/* dummy LL task - to start LL on secondary cores */
static enum task_state scheduler_dp_ll_tick_dummy(void *data)
{
	return SOF_TASK_STATE_RESCHEDULE;
}

/*
 * function called after every LL tick
 *
 * This function checks if the queued DP tasks are ready to processing (meaning
 *    the module run by the task has enough data at all sources and enough free space
 *    on all sinks)
 *
 *    if the task becomes ready, a deadline is set allowing Zephyr to schedule threads
 *    in right order
 *
 * TODO: currently there's a limitation - DP module must be surrounded by LL modules.
 * it simplifies algorithm - there's no need to browse through DP chains calculating
 * deadlines for each module in function of all modules execution status.
 * Now is simple - modules deadline is its start + tick time.
 *
 * example:
 *  Lets assume we do have a pipeline:
 *
 *  LL1 -> DP1 -> LL2 -> DP2 -> LL3 -> DP3 -> LL4
 *
 *  all LLs starts in 1ms tick
 *
 *  for simplification lets assume
 *   - all LLs are on primary core, all DPs on secondary (100% CPU is for DP)
 *   - context switching requires 0 cycles
 *
 *  DP1 - starts every 1ms, needs 0.5ms to finish processing
 *  DP2 - starts every 2ms, needs 0.6ms to finish processing
 *  DP3 - starts every 10ms, needs 0.3ms to finish processing
 *
 * TICK0
 *	only LL1 is ready to run
 *		LL1 processing (producing data chunk for DP1)
 *
 * TICK1
 *	LL1 is ready to run
 *	DP1 is ready tu run (has data from LL1) set deadline to TICK2
 *	  LL1 processing (producing second data chunk for DP1)
 *	  DP1 processing for 0.5ms (consuming first data chunk, producing data chunk for LL2)
 *	CPU is idle for 0.5ms
 *
 * TICK2
 *	LL1 is ready to run
 *	DP1 is ready tu run set deadline to TICK3
 *	LL2 is ready to run
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing 50% data chunk for DP2)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *	CPU is idle for 0.5ms
 *
 * TICK3
 *	LL1 is ready to run
 *	DP1 is ready tu run set deadline to TICK4
 *	LL2 is ready to run
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing rest of data chunk for DP2)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *	CPU is idle for 0.5ms
 *
 * TICK4
 *	LL1 is ready to run
 *	DP1 is ready tu run set deadline to TICK5
 *	LL2 is ready to run
 *	DP2 is ready to run set deadline to TICK6
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing 50% of second data chunk for DP2)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *		DP2 processing for 0.5ms (no data produced as DP2 has 0.1ms to go)
 *	100% CPU used
 *
 *	!!!!!! Note here - DP1 must do before DP2 as it MUST finish in this tick. DP2 can wait
 *		>>>>>>> this is what we call EDF - EARIEST DEADLINE FIRST <<<<<<
 *
 * TICK5
 *	LL1 is ready to run
 *	DP1 is ready tu run set deadline to TICK6
 *	LL2 is ready to run
 *	DP2 is in progress, deadline is set to TICK6
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing rest of second data chunk for DP2)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *		DP2 processing for 0.1ms (producing TWO data chunks for LL3)
 *	CPU is idle for 0.4ms (60% used)
 *
 * TICK6
 *	LL1 is ready to run
 *	DP1 is ready tu run set deadline to TICK7
 *	LL2 is ready to run
 *	DP2 is ready to run set deadline to TICK8
 *	LL3 is ready to run
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing 50% of second data chunk for DP2)
 *		LL3 processing (producing 10% of first data chunk for DP3)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *		DP2 processing for 0.5ms (no data produced as DP2 has 0.1ms to go)
 *	100% CPU used
 *
 *
 *
 *   (........ 9 more cycles - LL3 procuces 100% of data for DP3......)
 *
 *
 * TICK15
 *	LL1 is ready to run
 *	DP1 is ready tu run set deadline to TICK16
 *	LL2 is ready to run
 *	DP2 is ready to run set deadline to TICK17
 *	LL3 is ready to run
 *	DP3 is ready to run set deadline to TICK25
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing 50% of data chunk for DP2)
 *		LL3 processing (producing 10% of second data chunk for DP3)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *		DP2 processing for 0.5ms (no data produced as DP2 has 0.1ms to go)
 *	100% CPU used -
 *	!!! note that DP3 is ready but has no chance to get CPU in this cycle
 *
 * TICK16
 *	LL1 is ready to run set deadline to TICK17
 *	DP1 is ready tu run
 *	LL2 is ready to run
 *	DP2 is in progress, deadline is set to TICK17
 *	LL3 is ready to run
 *	DP3 is in progress, deadline is set to TICK25
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing rest of data chunk for DP2)
 *		LL3 processing (producing 10% of second data chunk for DP3)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *		DP2 processing for 0.1ms (producing data)
 *		DP3 processing for 0.2ms (producing 10 data chunks for LL4)
 *	90% CPU used
 *
 * TICK17
 *	LL1 is ready to run
 *	DP1 is ready tu run
 *	LL2 is ready to run
 *	DP2 is ready to run
 *	LL3 is ready to run
 *	LL4 is ready to run
 *			!! NOTE that DP3 is not ready - it will be ready again in TICK25
 *		LL1 processing (producing data chunk for DP1)
 *		LL2 processing (producing rest of data chunk for DP2)
 *		LL3 processing (producing next 10% of second data chunk for DP3)
 *		LL4 processing (consuming 10% of data prepared by DP3)
 *		DP1 processing for 0.5ms (producing data chunk for LL2)
 *		DP2 processing for 0.5ms (no data produced as DP2 has 0.1ms to go)
 *		100% CPU used
 *
 *
 * Now - pipeline is in stable state, CPU used almost in 100% (it would be 100% if DP3
 * needed 1.2ms for processing - but the example would be too complicated)
 */
static void scheduler_dp_ll_tick(void *receiver_data, enum notify_id event_type, void *caller_data)
{
	(void)receiver_data;
	(void)event_type;
	(void)caller_data;
	struct list_item *tlist;
	struct task *curr_task;
	struct task_dp_pdata *pdata;
	unsigned int lock_key;
	struct scheduler_dp_data *dp_sch = scheduler_get_data(SOF_SCHEDULE_DP);

	lock_key = scheduler_dp_lock();
	list_for_item(tlist, &dp_sch->tasks) {
		curr_task = container_of(tlist, struct task, list);
		pdata = curr_task->priv_data;
		struct processing_module *mod = pdata->mod;

		/* decrease number of LL ticks/cycles left till the module reaches its deadline */
		if (pdata->ll_cycles_to_start) {
			pdata->ll_cycles_to_start--;
			if (!pdata->ll_cycles_to_start)
				/* deadline reached, clear startup delay flag.
				 * see dp_startup_delay comment for details
				 */
				mod->dp_startup_delay = false;
		}

		if (curr_task->state != SOF_TASK_STATE_QUEUED ||
		    mod->dev->state < COMP_STATE_ACTIVE)
			continue;

		/* set a deadline for given num of ticks, starting now */
		k_thread_deadline_set(pdata->thread_id,
				      pdata->deadline_clock_ticks);

		/* trigger the task */
		curr_task->state = SOF_TASK_STATE_RUNNING;

		k_spinlock_key_t key = k_spin_lock(&thread_lock);

		pdata->pend_proc++;
		k_sem_give(&dp_thread_ipc_sem);

		k_spin_unlock(&thread_lock, key);
	}
	scheduler_dp_unlock(lock_key);
}

static int scheduler_dp_task_cancel(void *data, struct task *task)
{
	unsigned int lock_key;
	struct scheduler_dp_data *dp_sch = (struct scheduler_dp_data *)data;
	struct task_dp_pdata *pdata = task->priv_data;

	if (!pdata->thread_id)
		return 0;

	/* this is asyn cancel - mark the task as canceled and remove it from scheduling */
	lock_key = scheduler_dp_lock();

	task->state = SOF_TASK_STATE_CANCEL;
	list_item_del(&task->list);

	/* if there're no more DP task, stop LL tick source */
	if (list_is_empty(&dp_sch->tasks))
		schedule_task_cancel(&dp_sch->ll_tick_src);

	/* if the task is waiting on a semaphore - let it run and self-terminate */
	scheduler_dp_unlock(lock_key);

	/* wait till the task has finished, if there was any task created */
	k_thread_abort(pdata->thread_id);
	k_object_free(pdata->thread);

	pdata->thread_id = NULL;

	return 0;
}

static int scheduler_dp_task_free(void *data, struct task *task)
{
	struct task_dp_pdata *pdata = task->priv_data;

	scheduler_dp_task_cancel(data, task);

	pdata->thread_id = NULL;

	/* free task stack */
	k_thread_stack_free(pdata->p_stack);
	pdata->p_stack = NULL;

	k_mem_domain_remove_partition(&dp_mdom, pdata->mpart + SOF_DP_PART_HEAP);
	k_mem_domain_remove_partition(&dp_mdom, pdata->mpart + SOF_DP_PART_IPC);
	k_mem_domain_remove_partition(&dp_mdom, pdata->mpart + SOF_DP_PART_CFG);

	llext_manager_rm_domain(pdata->mod->dev->ipc_config.id, &dp_mdom);

	/* all other memory has been allocated as a single malloc, will be freed later by caller */
	return 0;
}

/* TODO: make this a shared kernel->module buffer for IPC parameters */
static uint8_t ipc_buf[4096] __aligned(4096);

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
		ret = k_sem_take(&dp_thread_sync_sem, K_MSEC(100));
		if (ret < 0)
			return ret;
	}

	k_spinlock_key_t key = k_spin_lock(&thread_lock);

	struct ipc4_flat *flat = (struct ipc4_flat *)ipc_buf;

	/* IPCs are serialised */
	flat->ret = -ENOSYS;

	ret = ipc_thread_flatten(cmd, param, flat);
	if (!ret) {
		pdata->pend_ipc++;
		k_sem_give(&dp_thread_ipc_sem);
	}

	k_spin_unlock(&thread_lock, key);

	if (!ret) {
		/* Wait for completion */
		k_sem_take(&dp_thread_sync_sem, K_FOREVER);
		ret = flat->ret;
	}

	return ret;
}

#include <zephyr/internal/syscall_handler.h>
/* Thread function called in component context, on target core */
static void dp_thread_fn(void *p1, void *p2, void *p3)
{
	struct task *task = p1;
	struct task_dp_pdata *task_pdata = task->priv_data;
	struct processing_module *pmod = task_pdata->mod;
	bool first = true;
	//unsigned int lock_key;
	enum task_state state;
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
			k_sem_give(&dp_thread_sync_sem);
		}

		/*
		 * The thread is started immediately after creation, it stops on
		 * semaphore. Semaphore is signaled once the task is ready to process
		 */
		k_sem_take(&dp_thread_ipc_sem, K_FOREVER);

		//k_spinlock_key_t key = k_spin_lock(&thread_lock);
		unsigned char pend_ipc = task_pdata->pend_ipc,
			pend_proc = task_pdata->pend_proc;

		task_pdata->pend_proc = 0;
		task_pdata->pend_ipc = 0;

		//k_spin_unlock(&thread_lock, key);

		/* Only 0:1, 1:0 and 1:1 are valid */
		if ((unsigned int)(pend_ipc + pend_proc - 1) > 1) {
			tr_err(&dp_tr, "Invalid wake up %u:%u", pend_proc, pend_ipc);
			continue;
		}

		if (pend_ipc) {
			/* handle IPC */
			tr_dbg(&dp_tr, "got IPC wake up for %p state %d", pmod, task->state);
			ipc_thread_unflatten_run(pmod, (struct ipc4_flat *)ipc_buf);
			k_sem_give(&dp_thread_sync_sem);
		}

		if (pend_proc) {
			/* handle audio */
			bool ready = module_is_ready_to_process(pmod,
								pmod->sources, pmod->num_of_sources,
								pmod->sinks, pmod->num_of_sinks);

			if (ready) {
				if (task->state == SOF_TASK_STATE_RUNNING)
					state = task_run(task);
				else
					state = task->state;	/* to avoid undefined variable warning */
			}

			//lock_key = scheduler_dp_lock();

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
		//} else {
		//	lock_key = scheduler_dp_lock();
		}

		/* if true exit the while loop, terminate the thread */
		task_stop = task->state == SOF_TASK_STATE_COMPLETED ||
			task->state == SOF_TASK_STATE_CANCEL;
		//scheduler_dp_unlock(lock_key);
	} while (!task_stop);

	/* call task_complete  */
	if (task->state == SOF_TASK_STATE_COMPLETED)
		task_complete(task);
}

static int scheduler_dp_task_shedule(void *data, struct task *task, uint64_t start,
				     uint64_t period)
{
	struct scheduler_dp_data *dp_sch = (struct scheduler_dp_data *)data;
	struct task_dp_pdata *pdata = task->priv_data;
	unsigned int lock_key;
	uint64_t deadline_clock_ticks;

	lock_key = scheduler_dp_lock();

	if (task->state != SOF_TASK_STATE_INIT &&
	    task->state != SOF_TASK_STATE_CANCEL &&
	    task->state != SOF_TASK_STATE_COMPLETED) {
		scheduler_dp_unlock(lock_key);
		return -EINVAL;
	}

	/* if there's no DP tasks scheduled yet, run ll tick source task */
	if (list_is_empty(&dp_sch->tasks))
		schedule_task(&dp_sch->ll_tick_src, 0, 0);

	/* add a task to DP scheduler list */
	task->state = SOF_TASK_STATE_QUEUED;
	list_item_prepend(&task->list, &dp_sch->tasks);

	deadline_clock_ticks = period * CONFIG_SYS_CLOCK_TICKS_PER_SEC;
	/* period/deadline is in us - convert to seconds in next step
	 * or it always will be zero because of integer calculation
	 */
	deadline_clock_ticks /= 1000000;

	pdata->deadline_clock_ticks = deadline_clock_ticks;
	pdata->ll_cycles_to_start = period / LL_TIMER_PERIOD_US;
	pdata->mod->dp_startup_delay = true;
	scheduler_dp_unlock(lock_key);

	tr_dbg(&dp_tr, "DP task scheduled with period %u [us]", (uint32_t)period);
	return 0;

}

static struct scheduler_ops schedule_dp_ops = {
	.schedule_task		= scheduler_dp_task_shedule,
	.schedule_task_cancel	= scheduler_dp_task_cancel,
	.schedule_task_free	= scheduler_dp_task_free,
};

int scheduler_dp_init(void)
{
	int ret;
	struct scheduler_dp_data *dp_sch = rzalloc(SOF_MEM_FLAG_KERNEL,
						   sizeof(struct scheduler_dp_data));
	if (!dp_sch)
		return -ENOMEM;

	k_spinlock_init(&thread_lock);

	list_init(&dp_sch->tasks);

	scheduler_init(SOF_SCHEDULE_DP, &schedule_dp_ops, dp_sch);

	/* init src of DP tick */
	ret = schedule_task_init_ll(&dp_sch->ll_tick_src,
				    SOF_UUID(dp_sched_uuid),
				    SOF_SCHEDULE_LL_TIMER,
				    0, scheduler_dp_ll_tick_dummy, dp_sch,
				    cpu_get_id(), 0);

	if (ret)
		return ret;

	notifier_register(NULL, NULL, NOTIFIER_ID_LL_POST_RUN, scheduler_dp_ll_tick, 0);

	return 0;
}

extern sys_slist_t xtensa_domain_list;

int scheduler_dp_task_init(struct task **task,
			   const struct sof_uuid_entry *uid,
			   const struct task_ops *ops,
			   struct processing_module *mod,
			   uint16_t core,
			   size_t stack_size)
{
	k_thread_stack_t *p_stack;

	/* memory allocation helper structure */
	struct {
		struct task task;
		struct task_dp_pdata pdata;
		struct comp_driver drv;
		struct module_interface ops;
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
	task_memory = mod_alloc_ext(mod, SOF_MEM_FLAG_COHERENT, sizeof(*task_memory), 0);
	if (!task_memory) {
		tr_err(&dp_tr, "memory alloc failed");
		return -ENOMEM;
	}

	memset(task_memory, 0, sizeof(*task_memory));

	task_memory->drv = *mod->dev->drv;
	task_memory->ops = *mod->dev->drv->adapter_ops;
	task_memory->drv.adapter_ops = &task_memory->ops;
	mod->dev->drv = &task_memory->drv;

	/* allocate stack - must be aligned and cached so a separate alloc */
	stack_size = Z_KERNEL_STACK_SIZE_ADJUST(stack_size);
	p_stack = k_thread_stack_alloc(stack_size, K_USER);
	if (!p_stack) {
		tr_err(&dp_tr, "stack alloc failed");
		ret = -ENOMEM;
		goto err;
	}

	struct task *ptask = &task_memory->task;

	/* internal SOF task init */
	ret = schedule_task_init(ptask, uid, SOF_SCHEDULE_DP, 0, ops->run, mod, core, 0);
	if (ret < 0) {
		tr_err(&dp_tr, "schedule_task_init failed");
		goto err;
	}

	struct task_dp_pdata *pdata = &task_memory->pdata;

	/* initialize other task structures */
	ptask->ops.complete = ops->complete;
	ptask->ops.get_deadline = ops->get_deadline;
	ptask->priv_data = pdata;
	pdata->p_stack = p_stack;
	pdata->mod = mod;

	*task = ptask;

	/* create a zephyr thread for the task */
	pdata->thread = k_object_alloc(K_OBJ_THREAD);
	if (!pdata->thread)
		goto err;
	memset(&pdata->thread->arch, 0, sizeof(pdata->thread->arch));
	pdata->thread_id = k_thread_create(pdata->thread, p_stack, stack_size, dp_thread_fn,
					   ptask, NULL, NULL, CONFIG_DP_THREAD_PRIORITY,
					   K_USER, K_FOREVER);

	k_thread_access_grant(pdata->thread, &dp_thread_sync_sem, &dp_thread_ipc_sem);

	/* pin the thread to specific core */
	ret = k_thread_cpu_pin(pdata->thread_id, core);
	if (ret < 0) {
		tr_err(&dp_tr, "zephyr task pin to core failed");
		goto e_thread;
	}

	int pidx;
	size_t size;
	uintptr_t start;
	struct k_mem_partition *ppart[SOF_DP_PART_TYPE_COUNT];

	for (pidx = 0; pidx < ARRAY_SIZE(ppart); pidx++)
		ppart[pidx] = pdata->mpart + pidx;

	mod_heap_info(mod, &size, &start);
	pdata->mpart[SOF_DP_PART_HEAP] = (struct k_mem_partition){
		.start = start,
		.size = size,
		.attr = K_MEM_PARTITION_P_RW_U_RW,
	};
	pdata->mpart[SOF_DP_PART_IPC] = (struct k_mem_partition){
		.start = (uintptr_t)&ipc_buf,
		.size = sizeof(ipc_buf),
		.attr = K_MEM_PARTITION_P_RW_U_RW,
	};
	pdata->mpart[SOF_DP_PART_CFG] = (struct k_mem_partition){
		.start = (uintptr_t)MAILBOX_HOSTBOX_BASE,
		.size = 4096,
		.attr = K_MEM_PARTITION_P_RO_U_RO,
	};

	/* Create a memory domain, add the thread and its stack and heap to it */
	if (dp_mdom.arch.ptables) {
		bool found = sys_slist_find_and_remove(&xtensa_domain_list, &dp_mdom.arch.node);

		tr_dbg(&dp_tr, "found %u domain %p", found, &dp_mdom);
		memset(&dp_mdom, 0, sizeof(dp_mdom));
	}

	ret = k_mem_domain_init(&dp_mdom, SOF_DP_PART_TYPE_COUNT, ppart);
	if (ret < 0) {
		tr_err(&dp_tr, "failed to init mem domain %d", ret);
		goto e_thread;
	}

	ret = llext_manager_add_domain(mod->dev->ipc_config.id, &dp_mdom);
	if (ret < 0) {
		tr_err(&dp_tr, "failed to add LLEXT to domain %d", ret);
		goto e_thread;
	}

	ret = k_mem_domain_add_thread(&dp_mdom, pdata->thread_id);
	if (ret < 0) {
		tr_err(&dp_tr, "failed to add thread to domain %d", ret);
		goto e_thread;
	}

	k_thread_start(pdata->thread_id);

	return 0;

e_thread:
	k_thread_abort(pdata->thread_id);
	k_object_free(pdata->thread);
err:
	/* cleanup - free all allocated resources */
	k_thread_stack_free(p_stack);
	mod_free(mod, task_memory);
	return ret;
}

void scheduler_get_task_info_dp(struct scheduler_props *scheduler_props, uint32_t *data_off_size)
{
	unsigned int lock_key;

	scheduler_props->processing_domain = COMP_PROCESSING_DOMAIN_DP;
	struct scheduler_dp_data *dp_sch =
		(struct scheduler_dp_data *)scheduler_get_data(SOF_SCHEDULE_DP);

	lock_key = scheduler_dp_lock();
	scheduler_get_task_info(scheduler_props, data_off_size,  &dp_sch->tasks);
	scheduler_dp_unlock(lock_key);
}
