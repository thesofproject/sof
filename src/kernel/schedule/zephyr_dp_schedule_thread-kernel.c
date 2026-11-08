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

/* Kernel part of the thread variant of the DP scheduler: the DP thread creation and
 * the scheduler_dp_internal_free() system call. The DP thread function is in
 * src/schedule/zephyr_dp_schedule_thread.c
 */

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
	k_event_init(pdata->event);

	/* success, fill the structures */
	pdata->p_stack = p_stack;
	pdata->mod = mod;

	/* create a zephyr thread for the task */
	pdata->thread_id = k_thread_create(pdata->thread, (__sparse_force void *)p_stack,
					   stack_size, dp_thread_fn, &task_memory->task, NULL, NULL,
					   CONFIG_DP_THREAD_PRIORITY, task_memory->task.flags,
					   K_FOREVER);

#ifdef CONFIG_SCHED_CPU_MASK
	/* pin the thread to specific core */
	ret = k_thread_cpu_pin(pdata->thread_id, core);
	if (ret < 0) {
		tr_err(&dp_tr, "zephyr task pin to core failed");
		goto e_thread;
	}
#endif

#ifdef CONFIG_USERSPACE
	if (options & K_USER) {
		k_thread_access_grant(pdata->thread_id, pdata->event);
		scheduler_dp_grant(pdata->thread_id, core);

		ret = k_mem_domain_add_thread(pdata->mod->user_ctx->comp_dom, pdata->thread_id);
		if (ret < 0) {
			tr_err(&dp_tr, "k_mem_domain_add_thread() failed %d", ret);
			goto e_thread;
		}

#if IS_ENABLED(CONFIG_SOF_USERSPACE_MOD_IPC_BY_DP_THREAD)
		pdata->ipc_work_item = userspace_proxy_register_ipc_handler(mod, pdata->event);
		assert(pdata->ipc_work_item);
#endif
	}
#endif /* CONFIG_USERSPACE */

	/* start the thread, it should immediately stop at an event */
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

void z_impl_scheduler_dp_internal_free(struct task *task)
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

#ifdef CONFIG_USERSPACE
#include <zephyr/internal/syscall_handler.h>

void z_vrfy_scheduler_dp_internal_free(struct task *task)
{
	/*
	 * With the thread DP scheduler variant scheduler_dp_internal_free() is
	 * never called from the userspace context
	 */
	K_OOPS(true);
}
#include <zephyr/syscalls/scheduler_dp_internal_free_mrsh.c>
#endif
