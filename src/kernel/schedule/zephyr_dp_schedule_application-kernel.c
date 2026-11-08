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

/* Kernel part of the application variant of the DP scheduler: the DP thread creation,
 * its memory domain, the kernel-only record of its kernel objects and the
 * scheduler_dp_internal_free() system call. The DP thread function and the IPC
 * forwarding to it are in src/schedule/zephyr_dp_schedule_application.c
 */

static struct objpool_head dp_mdom_head = {.list = LIST_INIT(dp_mdom_head.list)};

/* Synchronization semaphore for the scheduler thread to wait for DP startup */
#define DP_SYNC_INIT(i, _)	Z_SEM_INITIALIZER(dp_sync[i], 0, 1)
#define DP_SYNC_INIT_LIST	LISTIFY(CONFIG_CORE_COUNT, DP_SYNC_INIT, (,))
STRUCT_SECTION_ITERABLE_ARRAY(k_sem, dp_sync, CONFIG_CORE_COUNT) = { DP_SYNC_INIT_LIST };

/*
 * Kernel-only record of the kernel objects owned by a DP task.
 *
 * struct scheduler_dp_task_memory lives in the module's heap, which is mapped
 * into the DP thread's memory domain, and is also writable by the LL and IPC
 * user threads. Freeing must therefore never use the pointers stored there:
 * scheduler_dp_internal_free() takes them from this record instead. The task
 * pointer is only used as a lookup key.
 */
struct scheduler_dp_task_kobj {
	struct list_item list;
	struct task *task;
	struct processing_module *mod;
	struct k_event *event;
	struct k_thread *thread;
	struct k_mem_domain *mdom;
	struct k_mem_partition mpart[SOF_DP_PART_TYPE_COUNT];
	uint32_t comp_id;
	uint32_t gen;			/* unique per record, see dp_task_kobj_take() */
};

static struct list_item dp_task_kobj_list = LIST_INIT(dp_task_kobj_list);
static struct k_spinlock dp_task_kobj_lock;
/* generation of the last added record, under dp_task_kobj_lock */
static uint32_t dp_task_kobj_gen;

static void dp_task_kobj_add(struct scheduler_dp_task_kobj *kobj)
{
	k_spinlock_key_t key = k_spin_lock(&dp_task_kobj_lock);

	kobj->gen = ++dp_task_kobj_gen;
	list_item_append(&kobj->list, &dp_task_kobj_list);
	k_spin_unlock(&dp_task_kobj_lock, key);
}

/* Find the record for @task, caller holds dp_task_kobj_lock */
static struct scheduler_dp_task_kobj *dp_task_kobj_find(const struct task *task)
{
	struct scheduler_dp_task_kobj *kobj;
	struct list_item *item;

	list_for_item(item, &dp_task_kobj_list) {
		kobj = container_of(item, struct scheduler_dp_task_kobj, list);
		if (kobj->task == task)
			return kobj;
	}

	return NULL;
}

/* Copy the record for @task to @copy without unlinking it */
static bool dp_task_kobj_peek(const struct task *task, struct scheduler_dp_task_kobj *copy)
{
	k_spinlock_key_t key = k_spin_lock(&dp_task_kobj_lock);
	struct scheduler_dp_task_kobj *kobj = dp_task_kobj_find(task);

	if (kobj)
		*copy = *kobj;

	k_spin_unlock(&dp_task_kobj_lock, key);

	return kobj != NULL;
}

/*
 * Find the record for @task and unlink it, so it can only be freed once.
 * If @expected is given, only take the record it was copied from, so a
 * record replaced since @expected was peeked is left alone.
 */
static struct scheduler_dp_task_kobj *
dp_task_kobj_take(const struct task *task, const struct scheduler_dp_task_kobj *expected)
{
	k_spinlock_key_t key = k_spin_lock(&dp_task_kobj_lock);
	struct scheduler_dp_task_kobj *kobj = dp_task_kobj_find(task);

	/*
	 * Compare the generation, not the object addresses: the task could
	 * have been freed and recreated at the same addresses meanwhile.
	 */
	if (kobj && expected && kobj->gen != expected->gen)
		kobj = NULL;

	if (kobj)
		list_item_del(&kobj->list);

	k_spin_unlock(&dp_task_kobj_lock, key);

	return kobj;
}

/*
 * Safe to call with partial successful initialisation,
 * k_mem_domain_remove_partition() then just returns -ENOENT
 */
static void scheduler_dp_domain_free(struct processing_module *pmod, struct k_mem_domain *mdom,
				     struct k_mem_partition *mpart, uint32_t comp_id)
{
	llext_manager_rm_domain(comp_id, mdom);

	k_mem_domain_remove_partition(mdom, mpart + SOF_DP_PART_HEAP);
	k_mem_domain_remove_partition(mdom, mpart + SOF_DP_PART_HEAP_CACHE);
	k_mem_domain_remove_partition(mdom, mpart + SOF_DP_PART_CFG);
	k_mem_domain_remove_partition(mdom, mpart + SOF_DP_PART_CFG_CACHE);

	if (mdom->num_partitions) {
		/* Violation: all partitions should be freed by now */
		unsigned int n = mdom->num_partitions;

		LOG_WRN("%u domain partition(s) not freed, force-freeing them all", n);

		for (unsigned int i = 0; i < arch_mem_domain_max_partitions_get() && n; i++) {
			struct k_mem_partition *dpart = &mdom->partitions[i];

			if (dpart->size) {
				k_mem_domain_remove_partition(mdom, dpart);
				n--;
			}
		}
	}

	/* All partitions removed, the domain can be freed now */
	pmod->mdom = NULL;
	k_mem_domain_deinit(mdom);
	objpool_free(&dp_mdom_head, mdom);
}

/*
 * Free the kernel objects of @task, using only the kernel-only record.
 *
 * @return false if @task has no record (unknown task or already freed)
 */
static bool scheduler_dp_internal_free_kobj(struct task *task,
					    const struct scheduler_dp_task_kobj *expected)
{
	struct scheduler_dp_task_kobj *kobj = dp_task_kobj_take(task, expected);

	if (!kobj)
		return false;

	/* normally already terminated by scheduler_dp_task_free(), no-op then */
	k_thread_abort(kobj->thread);
	k_object_free(kobj->event);
	k_object_free(kobj->thread);
	scheduler_dp_domain_free(kobj->mod, kobj->mdom, kobj->mpart, kobj->comp_id);

	mod_free(kobj->mod, container_of(task, struct scheduler_dp_task_memory, task));
	rfree(kobj);

	return true;
}

void z_impl_scheduler_dp_internal_free(struct task *task)
{
	if (!scheduler_dp_internal_free_kobj(task, NULL))
		tr_err(&dp_tr, "unknown DP task %p", task);
}

#ifdef CONFIG_THREAD_NAME
static void scheduler_dp_thread_name_set(k_tid_t thread_id, struct processing_module *mod)
{
	char name[CONFIG_THREAD_MAX_NAME_LEN];

	snprintf(name, sizeof(name), "DP:%#x", mod->dev->ipc_config.id);

	k_thread_name_set(thread_id, name);
}
#else
/* k_thread_name_set() is a no-op so skip constructing a thread name */
#define scheduler_dp_thread_name_set(x, y)
#endif

/* Called only in IPC context in kernel mode (this can change) */
int scheduler_dp_task_init(struct task **task, const struct sof_uuid_entry *uid,
			   const struct task_ops *ops, struct processing_module *mod,
			   uint16_t core, size_t stack_size, uint32_t options)
{
	k_thread_stack_t *p_stack;
	struct scheduler_dp_task_memory *task_memory;
	uint8_t *ipc_config_data = NULL;
	k_tid_t thread_id;
	uint32_t comp_id;

	int ret;

	/* must be called on the same core the task will be bound to */
	assert(cpu_get_id() == core);
	assert(!k_is_user_context());

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

	const struct comp_driver *drv = mod->dev->drv;

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

	/*
	 * task_memory, including pdata, is in the module heap, which user
	 * threads can write while this function runs. Keep everything the
	 * kernel uses here or at teardown in kobj and locals, and only mirror
	 * it into pdata for the DP thread.
	 */
	struct task_dp_pdata *pdata = &task_memory->pdata;
	struct scheduler_dp_task_kobj *kobj = rmalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT,
						      sizeof(*kobj));

	if (!kobj) {
		tr_err(&dp_tr, "task record alloc failed");
		ret = -ENOMEM;
		goto e_stack;
	}

	kobj->task = ptask;
	kobj->mod = mod;
	comp_id = mod->dev->ipc_config.id;
	kobj->comp_id = comp_id;

	if (mod->dev->drv->adapter_ops->get_configuration) {
		ipc_config_data = mod_alloc_ext(mod, SOF_MEM_FLAG_USER, SOF_IPC_MSG_MAX_SIZE, 0);
		if (!ipc_config_data) {
			tr_err(&dp_tr, "Failed to allocate memory for get-config");
			ret = -ENOMEM;
			goto e_kobj;
		}
	}
	pdata->ipc_config_data = ipc_config_data;

	pdata->flat = &task_memory->flat;

	kobj->event = k_object_alloc(K_OBJ_EVENT);
	if (!kobj->event) {
		tr_err(&dp_tr, "Event object allocation failed");
		ret = -ENOMEM;
		goto e_config;
	}

	pdata->event = kobj->event;

	kobj->thread = k_object_alloc(K_OBJ_THREAD);
	if (!kobj->thread) {
		tr_err(&dp_tr, "Thread object allocation failed");
		ret = -ENOMEM;
		goto e_event;
	}
	memset(&kobj->thread->arch, 0, sizeof(kobj->thread->arch));
	pdata->thread = kobj->thread;

	/* success, fill the structures */
	pdata->p_stack = p_stack;
	pdata->mod = mod;

	/* initialize other task structures */
	ptask->ops.complete = ops->complete;
	ptask->ops.get_deadline = ops->get_deadline;
	ptask->priv_data = pdata;
	list_init(&ptask->list);

	/* create a zephyr thread for the task */
	thread_id = k_thread_create(kobj->thread, p_stack,
				    stack_size, dp_thread_fn, ptask, NULL, NULL,
				    CONFIG_DP_THREAD_PRIORITY, ptask->flags, K_FOREVER);
	pdata->thread_id = thread_id;
	scheduler_dp_thread_name_set(thread_id, mod);

	unsigned int pidx;
	size_t size;
	uintptr_t start;
	bool on_pool = false;

#ifdef CONFIG_SCHED_CPU_MASK
	/* pin the thread to specific core */
	ret = k_thread_cpu_pin(thread_id, core);
	if (ret < 0) {
		tr_err(&dp_tr, "zephyr task pin to core failed");
		goto e_thread;
	}
#endif

	k_thread_access_grant(thread_id, kobj->event, &dp_sync[core]);
	scheduler_dp_grant(thread_id, core);
#if CONFIG_SOF_USERSPACE_LL
	struct k_thread *thread_ipc = ipc_thread_user(core);

	k_thread_access_grant(thread_ipc, kobj->event, thread_id, p_stack, &dp_sync[core]);
	scheduler_dp_grant(thread_ipc, core);
	scheduler_dp_grant(zephyr_ll_domain_thread(), core);
#endif

	struct k_mem_domain *mdom = objpool_alloc(&dp_mdom_head, sizeof(*mdom),
						  SOF_MEM_FLAG_COHERENT);

	if (!mdom) {
		tr_err(&dp_tr, "objpool allocation failed");
		ret = -ENOMEM;
		goto e_thread;
	}

	on_pool = true;

	mod->mdom = mdom;
	kobj->mdom = mdom;

	if (!mdom->arch.ptables) {
		ret = k_mem_domain_init(mdom, 0, NULL);
		if (ret < 0)
			goto e_thread;
	}

	/* Module heap partition */
	mod_heap_info(mod, &size, &start);
	kobj->mpart[SOF_DP_PART_HEAP] = (struct k_mem_partition){
		.start = (uintptr_t)sys_cache_uncached_ptr_get((void *)start),
		.size = size,
		.attr = K_MEM_PARTITION_P_RW_U_RW,
	};
	kobj->mpart[SOF_DP_PART_HEAP_CACHE] = (struct k_mem_partition){
		.start = start,
		.size = size,
		.attr = K_MEM_PARTITION_P_RW_U_RW | XTENSA_MMU_CACHED_WB,
	};
	/* Host mailbox partition for additional IPC parameters: read-only */
	kobj->mpart[SOF_DP_PART_CFG] = (struct k_mem_partition){
		.start = (uintptr_t)sys_cache_uncached_ptr_get((void *)MAILBOX_HOSTBOX_BASE),
		.size = MAILBOX_HOSTBOX_SIZE,
		.attr = K_MEM_PARTITION_P_RO_U_RO,
	};
	kobj->mpart[SOF_DP_PART_CFG_CACHE] = (struct k_mem_partition){
		.start = (uintptr_t)MAILBOX_HOSTBOX_BASE,
		.size = MAILBOX_HOSTBOX_SIZE,
		.attr = K_MEM_PARTITION_P_RO_U_RO | XTENSA_MMU_CACHED_WB,
	};

	for (pidx = 0; pidx < SOF_DP_PART_TYPE_COUNT; pidx++) {
		ret = k_mem_domain_add_partition(mdom, kobj->mpart + pidx);
		if (ret < 0)
			goto e_dom;
	}

	memcpy(pdata->mpart, kobj->mpart, sizeof(pdata->mpart));

	ret = llext_manager_add_domain(comp_id, mdom);
	if (ret < 0) {
		tr_err(&dp_tr, "failed to add LLEXT to domain %d", ret);
		goto e_dom;
	}

	/*
	 * Keep this call last, able to fail, otherwise domain will be removed
	 * before its thread
	 */
	ret = k_mem_domain_add_thread(mdom, thread_id);
	if (ret < 0) {
		tr_err(&dp_tr, "failed to add thread to domain %d", ret);
		goto e_dom;
	}

	/* publish the record before the thread can run */
	dp_task_kobj_add(kobj);

	/* start the thread, it should immediately stop at the semaphore */
	k_event_init(kobj->event);
	k_thread_start(thread_id);

	/* ptask points into task_memory, so only publish it once it cannot be freed */
	*task = ptask;

	return 0;

e_dom:
	scheduler_dp_domain_free(mod, mdom, kobj->mpart, comp_id);
	on_pool = false;
e_thread:
	if (on_pool)
		objpool_free(&dp_mdom_head, mdom);
	k_thread_abort(thread_id);
	k_object_free(kobj->thread);
e_event:
	k_object_free(kobj->event);
e_config:
	mod_free(mod, ipc_config_data);
e_kobj:
	rfree(kobj);
e_stack:
	user_stack_free(p_stack);
e_tmem:
	/* the copy lives in task_memory, so stop pointing at it before freeing */
	mod->dev->drv = drv;
	mod_free(mod, task_memory);
	return ret;
}

#ifdef CONFIG_USERSPACE
#include <zephyr/internal/syscall_handler.h>
#include <sof/userspace/syscall_caller.h>

/*
 * mod_free() follows the module's resource and allocator pointers in
 * supervisor mode. They are user-writable, so at least check that they
 * point to memory the caller can access.
 */
static void scheduler_dp_mod_vrfy(struct processing_module *mod)
{
	K_OOPS(K_SYSCALL_MEMORY_WRITE(mod, sizeof(*mod)));
	K_OOPS(K_SYSCALL_MEMORY_WRITE(mod->dev, sizeof(*mod->dev)));
	K_OOPS(K_SYSCALL_MEMORY_READ(mod->dev->drv, sizeof(*mod->dev->drv)));

	struct mod_alloc_ctx *alloc = mod->priv.resources.alloc;

	K_OOPS(K_SYSCALL_MEMORY_WRITE(alloc, sizeof(*alloc)));
	if (alloc->heap) {
		size_t h_size = 0;
		uintptr_t h_start;

		mod_heap_info(mod, &h_size, &h_start);
		if (h_size)
			K_OOPS(K_SYSCALL_MEMORY_WRITE(h_start, h_size));
	}
	if (alloc->vreg) {
		size_t vr_size = 0;
		uintptr_t vr_start;

		vregion_mem_info(alloc->vreg, &vr_size, &vr_start);
		K_OOPS(K_SYSCALL_MEMORY_WRITE(vr_start, vr_size));
	}
}

void z_vrfy_scheduler_dp_internal_free(struct task *task)
{
	struct scheduler_dp_task_kobj kobj;

	SOF_SYSCALL_CALLER_CHECK(SOF_SYSUSER_INFRA);

	/*
	 * Do not read task->priv_data: the kernel objects come from the
	 * kernel-only record, and an unknown task pointer is rejected.
	 */
	K_OOPS(!dp_task_kobj_peek(task, &kobj));

	/*
	 * A DP thread holds its own thread and event objects, so it would pass
	 * the checks below for its own task. Freeing would then abort the
	 * caller after the record is unlinked, leaking everything it holds.
	 */
	K_OOPS(kobj.thread == k_current_get());

	/*
	 * The caller must have been granted the task's objects. Check before
	 * unlinking the record, so an unauthorized call leaves it intact.
	 */
	K_OOPS(K_SYSCALL_OBJ(kobj.event, K_OBJ_EVENT));
	K_OOPS(K_SYSCALL_OBJ_INIT(kobj.thread, K_OBJ_THREAD));
	scheduler_dp_mod_vrfy(kobj.mod);

	K_OOPS(!scheduler_dp_internal_free_kobj(task, &kobj));
}
#include <zephyr/syscalls/scheduler_dp_internal_free_mrsh.c>
#endif /* CONFIG_USERSPACE */
