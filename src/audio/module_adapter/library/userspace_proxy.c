// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//
// Author: Jaroslaw Stelter <jaroslaw.stelter@intel.com>
// Author: Adrian Warecki <adrian.warecki@intel.com>

/**
 * \file audio/module_adapter/library/userspace_proxy.c
 * \brief Userspace proxy. Acts as an intermediary between SOF and a userspace module.
 * \brief Responsible for preparing the memory domain required for userspace execution
 * \brief and forwarding API calls. The proxy invokes corresponding module methods
 * \brief in userspace context. Enables execution of any module implementing module_interface
 * \brief as a userspace module.
 * \authors Adrian Warecki
 */

#include <sof/common.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/memory.h>
#include <rtos/string.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <sof/lib_manager.h>
#include <sof/audio/component.h>
#include <sof/schedule/dp_schedule.h>
#include <rtos/userspace_helper.h>
#include <utilities/array.h>
#include <zephyr/sys/sem.h>
#include <sof/audio/module_adapter/module/generic.h>

#include <sof/audio/module_adapter/library/userspace_proxy.h>
#include <sof/audio/module_adapter/library/userspace_proxy_user.h>
#include <rimage/sof/user/manifest.h>

 /* Assume that all the code runs in supervisor mode and don't make system calls. */
#define __ZEPHYR_SUPERVISOR__

LOG_MODULE_REGISTER(userspace_proxy, CONFIG_SOF_LOG_LEVEL);

/* 6f6b6f4b-6f73-7466-20e1e62b9779f003 */
SOF_DEFINE_REG_UUID(userspace_proxy);

DECLARE_TR_CTX(userspace_proxy_tr, SOF_UUID(userspace_proxy_uuid), LOG_LEVEL_INFO);

static const struct module_interface userspace_proxy_interface;

/* IPC requests targeting userspace modules are handled through a user work queue.
 * Each userspace module provides its own work item that carries the IPC request parameters.
 * The worker thread is switched into the module's memory domain and receives the work item.
 * It invokes the appropriate module function in userspace context and writes the operation
 * result back into the work item.
 *
 * There is only a single work queue, which is shared by all userspace modules. It is created
 * dynamically when needed. Because SOF uses a single dedicated thread for handling IPC, there
 * is no need to perform any additional serialization when accessing the worker.
 */
struct user_worker {
	k_tid_t thread_id;			/* ipc worker thread ID			*/
	uint32_t reference_count;		/* module reference count		*/
	void *stack_ptr;			/* pointer to worker stack		*/
	struct k_work_user_q work_queue;
	struct k_event event;
};

static struct user_worker worker;

static int user_worker_get(void)
{
	if (worker.reference_count) {
		worker.reference_count++;
		return 0;
	}

	worker.stack_ptr = user_stack_allocate(CONFIG_SOF_USERSPACE_PROXY_WORKER_STACK_SIZE,
					       K_USER);
	if (!worker.stack_ptr) {
		tr_err(&userspace_proxy_tr, "Userspace worker stack allocation failed.");
		return -ENOMEM;
	}

	k_event_init(&worker.event);
	k_work_user_queue_start(&worker.work_queue, worker.stack_ptr,
				CONFIG_SOF_USERSPACE_PROXY_WORKER_STACK_SIZE, 0, NULL);

	worker.thread_id = k_work_user_queue_thread_get(&worker.work_queue);

	k_thread_access_grant(worker.thread_id, &worker.event);

	worker.reference_count++;
	return 0;
}

static void user_worker_put(void)
{
	/* Module removed so decrement counter */
	worker.reference_count--;

	/* Free worker resources if no more active user space modules */
	if (worker.reference_count == 0) {
		k_thread_abort(worker.thread_id);
		user_stack_free(worker.stack_ptr);
	}
}

static int user_work_item_init(struct userspace_context *user_ctx, struct k_heap *user_heap)
{
	struct user_work_item *work_item = NULL;
	int ret;

	ret = user_worker_get();
	if (ret)
		return ret;

	/* We have only a single userspace IPC worker. It handles requests for all userspace
	 * modules, which may run on different cores. Because the worker processes work items
	 * coming from any core, the work item must be allocated in coherent memory.
	 */
	work_item = sof_heap_alloc(user_heap, SOF_MEM_FLAG_COHERENT, sizeof(*work_item), 0);
	if (!work_item) {
		user_worker_put();
		return -ENOMEM;
	}

	k_work_user_init(&work_item->work_item, userspace_proxy_worker_handler);

	work_item->event = &worker.event;
	work_item->params.context = user_ctx;
	user_ctx->work_item = work_item;

	return 0;
}

static void user_work_item_free(struct userspace_context *user_ctx, struct k_heap *user_heap)
{
	sof_heap_free(user_heap, user_ctx->work_item);
	user_worker_put();
}

static inline struct module_params *user_work_get_params(struct userspace_context *user_ctx)
{
	return &user_ctx->work_item->params;
}

BUILD_ASSERT(IS_ALIGNED(MAILBOX_HOSTBOX_BASE, CONFIG_MMU_PAGE_SIZE),
	     "MAILBOX_HOSTBOX_BASE is not page aligned");

BUILD_ASSERT(IS_ALIGNED(MAILBOX_HOSTBOX_SIZE, CONFIG_MMU_PAGE_SIZE),
	     "MAILBOX_HOSTBOX_SIZE is not page aligned");

static int userspace_proxy_invoke(struct userspace_context *user_ctx, uint32_t cmd,
				  bool ipc_payload_access)
{
	struct module_params *params = user_work_get_params(user_ctx);
	const uintptr_t ipc_req_buf = (uintptr_t)MAILBOX_HOSTBOX_BASE;
	struct k_mem_partition ipc_part = {
		.start = ipc_req_buf,
		.size = MAILBOX_HOSTBOX_SIZE,
		.attr = user_get_partition_attr(ipc_req_buf) | K_MEM_PARTITION_P_RO_U_RO,
	};
	int ret, ret2;

	params->cmd = cmd;

	if (ipc_payload_access) {
		ret = k_mem_domain_add_partition(user_ctx->comp_dom, &ipc_part);
		if (ret < 0) {
			tr_err(&userspace_proxy_tr, "Add mailbox to domain error: %d", ret);
			return ret;
		}
	}

	/* Switch worker thread to module memory domain */
	ret = k_mem_domain_add_thread(user_ctx->comp_dom, worker.thread_id);
	if (ret < 0) {
		tr_err(&userspace_proxy_tr, "Failed to switch memory domain, error: %d", ret);
		goto done;
	}

	/* Pin worker thread to the same core as the module */
	ret = k_thread_cpu_pin(worker.thread_id, cpu_get_id());
	if (ret < 0) {
		tr_err(&userspace_proxy_tr, "Failed to pin cpu, error: %d", ret);
		goto done;
	}

	ret = k_work_user_submit_to_queue(&worker.work_queue, &user_ctx->work_item->work_item);
	if (ret < 0) {
		tr_err(&userspace_proxy_tr, "Submit to queue error: %d", ret);
		goto done;
	}

	/* Timeout value is aligned with the ipc_wait_for_compound_msg function */
	if (!k_event_wait_safe(&worker.event, DP_TASK_EVENT_IPC_DONE, false,
			       Z_TIMEOUT_US(250 * 20))) {
		tr_err(&userspace_proxy_tr, "IPC processing timedout.");
		ret = -ETIMEDOUT;
	}

done:
	if (ipc_payload_access) {
		ret2 = k_mem_domain_remove_partition(user_ctx->comp_dom, &ipc_part);
		if (ret2 < 0) {
			tr_err(&userspace_proxy_tr, "Mailbox remove from domain error: %d", ret);

			if (!ret)
				ret = ret2;
		}
	}

	return ret;
}

extern struct k_mem_partition common_partition;

static int userspace_proxy_memory_init(struct userspace_context *user_ctx,
				       const struct comp_driver *drv)
{
	/* Add module private heap to memory partitions */
	struct k_mem_partition heap_part = { .attr = K_MEM_PARTITION_P_RW_U_RW };
	struct sys_heap *heap = &drv->user_heap->heap;

	k_mem_region_align(&heap_part.start, &heap_part.size,
			   POINTER_TO_UINT(heap->init_mem),
			   heap->init_bytes, CONFIG_MM_DRV_PAGE_SIZE);

	tr_dbg(&userspace_proxy_tr, "Heap partition %#lx + %zx, attr = %u",
	       heap_part.start, heap_part.size, heap_part.attr);

#if !defined(CONFIG_XTENSA_MMU_DOUBLE_MAP) && defined(CONFIG_SOF_ZEPHYR_HEAP_CACHED)
#define HEAP_PART_CACHED
	/* Add cached module private heap to memory partitions */
	struct k_mem_partition heap_cached_part = {
		.attr = K_MEM_PARTITION_P_RW_U_RW | XTENSA_MMU_CACHED_WB
	};

	k_mem_region_align(&heap_cached_part.start, &heap_cached_part.size,
			   POINTER_TO_UINT(sys_cache_cached_ptr_get(heap->init_mem)),
			   heap->init_bytes, CONFIG_MM_DRV_PAGE_SIZE);

	tr_dbg(&userspace_proxy_tr, "Cached heap partition %#lx + %zx, attr = %u",
	       heap_cached_part.start, heap_cached_part.size, heap_cached_part.attr);
#endif

	struct k_mem_partition *parts_ptr[] = {
		/* The common partition contains sof components accessible to the userspace module.
		 * These include ops structures marked with APP_TASK_DATA.
		 */
		&common_partition,
#ifdef HEAP_PART_CACHED
		&heap_cached_part,
#endif
		&heap_part
	};

	tr_dbg(&userspace_proxy_tr, "Common partition %#lx + %zx, attr = %u",
	       common_partition.start, common_partition.size, common_partition.attr);

	return k_mem_domain_init(user_ctx->comp_dom, ARRAY_SIZE(parts_ptr), parts_ptr);
}

static int userspace_proxy_add_sections(struct userspace_context *user_ctx, uint32_t instance_id,
					const struct sof_man_module *const mod)
{
	struct k_mem_partition mem_partition;
	void *va_base;
	int idx, ret;

	for (idx = 0; idx < ARRAY_SIZE(mod->segment); ++idx) {
		if (!mod->segment[idx].flags.r.load)
			continue;

		if (mod->segment[idx].flags.r.code)
			mem_partition.attr = K_MEM_PARTITION_P_RX_U_RX;
		else if (!mod->segment[idx].flags.r.readonly)
			mem_partition.attr = K_MEM_PARTITION_P_RW_U_RW;
		else
			mem_partition.attr = K_MEM_PARTITION_P_RO_U_RO;

		mem_partition.start = mod->segment[idx].v_base_addr;
		mem_partition.size = mod->segment[idx].flags.r.length * CONFIG_MM_DRV_PAGE_SIZE;
		mem_partition.attr |= user_get_partition_attr(mem_partition.start);

		ret = k_mem_domain_add_partition(user_ctx->comp_dom, &mem_partition);

		tr_dbg(&userspace_proxy_tr, "Add mod partition %#lx + %zx, attr = %u, ret = %d",
		       mem_partition.start, mem_partition.size, mem_partition.attr, ret);

		if (ret < 0)
			return ret;
	}

	lib_manager_get_instance_bss_address(instance_id, mod, &va_base, &mem_partition.size);
	mem_partition.start = POINTER_TO_UINT(va_base);
	mem_partition.attr = user_get_partition_attr(mem_partition.start) |
		K_MEM_PARTITION_P_RW_U_RW;
	ret = k_mem_domain_add_partition(user_ctx->comp_dom, &mem_partition);

	tr_dbg(&userspace_proxy_tr, "Add bss partition %#lx + %zx, attr = %u, ret = %d",
	       mem_partition.start, mem_partition.size, mem_partition.attr, ret);

	return ret;
}

static int userspace_proxy_start_agent(struct userspace_context *user_ctx,
				       system_agent_start_fn start_fn,
				       const struct system_agent_params *agent_params,
				       const void **agent_interface)
{
	const byte_array_t * const mod_cfg = (byte_array_t *)agent_params->mod_cfg;
	struct module_params *params = user_work_get_params(user_ctx);
	int ret;

	params->ext.agent.start_fn = start_fn;
	params->ext.agent.params = *agent_params;
	params->ext.agent.mod_cfg = *mod_cfg;

	ret = userspace_proxy_invoke(user_ctx, USER_PROXY_MOD_CMD_AGENT_START, true);
	if (ret)
		return ret;

	*agent_interface = params->ext.agent.out_interface;
	return params->status;
}

int userspace_proxy_create(struct userspace_context **user_ctx, const struct comp_driver *drv,
			   const struct sof_man_module *manifest, system_agent_start_fn start_fn,
			   const struct system_agent_params *agent_params,
			   const void **agent_interface, const struct module_interface **ops)
{
	struct userspace_context *context;
	struct k_mem_domain *domain;
	int ret;

	tr_dbg(&userspace_proxy_tr, "userspace create");

	context = k_heap_alloc(drv->user_heap, sizeof(struct userspace_context), K_FOREVER);
	if (!context)
		return -ENOMEM;

	/* Allocate memory domain struct */
	domain = rzalloc(SOF_MEM_FLAG_KERNEL, sizeof(*domain));
	if (!domain) {
		ret = -ENOMEM;
		goto error;
	}
	context->comp_dom = domain;

	ret = userspace_proxy_memory_init(context, drv);
	if (ret)
		goto error_dom;

	ret = userspace_proxy_add_sections(context, agent_params->instance_id, manifest);
	if (ret)
		goto error_dom;

	ret = user_work_item_init(context, drv->user_heap);
	if (ret)
		goto error_dom;

	/* Start the system agent, if provided. */

	if (start_fn) {
		ret = userspace_proxy_start_agent(context, start_fn, agent_params, agent_interface);
		if (ret) {
			tr_err(&userspace_proxy_tr, "System agent failed with error %d.", ret);
			goto error_work_item;
		}
	}

	*user_ctx = context;

	/* Store a pointer to the module's interface. For the LMDK modules, the agent places a
	 * pointer to the module interface at the address specified by agent_interface. Since this
	 * points to ops, the assignment of the module interface used by this proxy must occur
	 * after the agent has been started. For other module types, the ops parameter points to a
	 * valid module interface.
	 */
	context->interface = *ops;

	/* All calls to the module interface must pass through the proxy. Set up our own interface.
	 */
	*ops = &userspace_proxy_interface;

	return 0;

error_work_item:
	user_work_item_free(context, drv->user_heap);
error_dom:
	rfree(domain);
error:
	k_heap_free(drv->user_heap, context);
	return ret;
}

void userspace_proxy_destroy(const struct comp_driver *drv, struct userspace_context *user_ctx)
{
	tr_dbg(&userspace_proxy_tr, "userspace proxy destroy");
	user_work_item_free(user_ctx, drv->user_heap);
	rfree(user_ctx->comp_dom);
	k_heap_free(drv->user_heap, user_ctx);
}

/**
 * Copy parameters to user worker accessible space.
 * Queue module init() operation and return its result.
 * Module init() code is performed in user workqueue.
 *
 * @param mod - pointer to processing module structure.
 * @return 0 for success, error otherwise.
 */
static int userspace_proxy_init(struct processing_module *mod)
{
	struct module_params *params = user_work_get_params(mod->user_ctx);
	int ret;

	comp_dbg(mod->dev, "start");

	params->mod = mod;
	ret = userspace_proxy_invoke(mod->user_ctx, USER_PROXY_MOD_CMD_INIT, true);
	if (ret)
		return ret;

	/* Return status from module code operation. */
	return params->status;
}

/**
 * Copy parameters to user worker accessible space.
 * Queue module prepare() operation and return its result.
 * Module prepare() code is performed in user workqueue.
 *
 * @return 0 for success, error otherwise.
 */
static int userspace_proxy_prepare(struct processing_module *mod,
				   struct sof_source **sources, int num_of_sources,
				   struct sof_sink **sinks, int num_of_sinks)
{
	struct module_params *params = user_work_get_params(mod->user_ctx);
	int ret;

	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->prepare)
		return 0;

	params->ext.proc.sources = sources;
	params->ext.proc.num_of_sources = num_of_sources;
	params->ext.proc.sinks = sinks;
	params->ext.proc.num_of_sinks = num_of_sinks;

	ret = userspace_proxy_invoke(mod->user_ctx, USER_PROXY_MOD_CMD_PREPARE, false);
	if (ret)
		return ret;

	/* Return status from module code operation. */
	return params->status;
}

/**
 * Forward processing request to the module's process() implementation.
 *
 * It is invoked by the DP thread running in userspace, so no
 * additional queuing or context switching is performed here.
 *
 * @param mod Pointer to the processing module instance.
 * @param sources Array of input sources for the module.
 * @param num_of_sources Number of input sources.
 * @param sinks Array of output sinks for the module.
 * @param num_of_sinks Number of output sinks.
 *
 * @return 0 on success, negative error code on failure.
 */
static int userspace_proxy_process(struct processing_module *mod, struct sof_source **sources,
				   int num_of_sources, struct sof_sink **sinks, int num_of_sinks)
{
	return mod->user_ctx->interface->process(mod, sources, num_of_sources, sinks, num_of_sinks);
}

/**
 * Copy parameters to user worker accessible space.
 * Queue module reset() operation and return its result.
 * Module reset() code is performed in user workqueue.
 *
 * @param mod - pointer to processing module structure.
 * @return 0 for success, error otherwise.
 */
static int userspace_proxy_reset(struct processing_module *mod)
{
	struct module_params *params = user_work_get_params(mod->user_ctx);
	int ret;

	if (!mod->user_ctx->interface->reset)
		return 0;

	ret = userspace_proxy_invoke(mod->user_ctx, USER_PROXY_MOD_CMD_RESET, false);
	if (ret)
		return ret;

	/* Return status from module code operation. */
	return params->status;
}

/**
 * Copy parameters to user worker accessible space.
 * Queue module free() operation and return its result.
 * Module free() code is performed in user workqueue.
 *
 * @param mod - pointer to processing module structure.
 * @return 0 for success, error otherwise.
 */
static int userspace_proxy_free(struct processing_module *mod)
{
	struct module_params *params = user_work_get_params(mod->user_ctx);
	int ret = 0;

	comp_dbg(mod->dev, "start");

	if (mod->user_ctx->interface->free) {
		ret = userspace_proxy_invoke(mod->user_ctx, USER_PROXY_MOD_CMD_FREE, false);
		if (ret)
			return ret;
		ret = params->status;
	}

	/* Destroy workqueue if this was last active userspace module */
	userspace_proxy_destroy(mod->dev->drv, mod->user_ctx);
	mod->user_ctx = NULL;

	/* Return status from module code operation. */
	return ret;
}

/**
 * Copy parameters to user worker accessible space.
 * Queue module set_configuration() operation and return its result.
 * Module set_configuration() code is performed in user workqueue.
 *
 * @param[in] mod - struct processing_module pointer
 * @param[in] config_id - Configuration ID
 * @param[in] pos - position of the fragment in the large message
 * @param[in] data_offset_size: size of the whole configuration if it is the first fragment or the
 *	      only fragment. Otherwise, it is the offset of the fragment in the whole
 *	      configuration.
 * @param[in] fragment: configuration fragment buffer
 * @param[in] fragment_size: size of @fragment
 * @params[in] response: optional response buffer to fill
 * @params[in] response_size: size of @response
 *
 * @return 0 for success, error otherwise.
 */
static int userspace_proxy_set_configuration(struct processing_module *mod, uint32_t config_id,
					     enum module_cfg_fragment_position pos,
					     uint32_t data_offset_size, const uint8_t *fragment,
					     size_t fragment_size, uint8_t *response,
					     size_t response_size)
{
	struct module_params *params = user_work_get_params(mod->user_ctx);
	int ret;

	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->set_configuration)
		return 0;

	params->ext.set_conf.config_id = config_id;
	params->ext.set_conf.pos = pos;
	params->ext.set_conf.data_off_size = data_offset_size;
	params->ext.set_conf.fragment = fragment;
	params->ext.set_conf.fragment_size = fragment_size;
	params->ext.set_conf.response = response;
	params->ext.set_conf.response_size = response_size;

	ret = userspace_proxy_invoke(mod->user_ctx, USER_PROXY_MOD_CMD_SET_CONF, true);
	if (ret)
		return ret;

	/* Return status from module code operation. */
	return params->status;
}

/**
 * Copy parameters to user worker accessible space.
 * Queue module get_configuration() operation and return its result.
 * Module get_configuration() code is performed in user workqueue.
 *
 * @param[in] mod - struct processing_module pointer
 * @param[in] config_id - Configuration ID
 * @param[in] data_offset_size: size of the whole configuration if it is the first fragment or the
 *	      only fragment. Otherwise, it is the offset of the fragment in the whole
 *	      configuration.
 * @param[in] fragment: configuration fragment buffer
 * @param[in] fragment_size: size of @fragment
 *
 * @return 0 for success, error otherwise.
 */
static int userspace_proxy_get_configuration(struct processing_module *mod, uint32_t config_id,
					     uint32_t *data_offset_size, uint8_t *fragment,
					     size_t fragment_size)
{
	struct module_params *params = user_work_get_params(mod->user_ctx);
	struct k_mem_domain *domain = mod->user_ctx->comp_dom;
	const uintptr_t ipc_resp_buf = POINTER_TO_UINT(ipc_get()->comp_data);

	/* Memory partition exposing the IPC response buffer. This buffer is allocated
	 * by the IPC driver and contains the payload of IPC replies sent to the host.
	 */
	struct k_mem_partition ipc_resp_part = {
		.start = ipc_resp_buf,
		.size = SOF_IPC_MSG_MAX_SIZE,
		.attr = user_get_partition_attr(ipc_resp_buf) | K_MEM_PARTITION_P_RW_U_RW,
	};
	int ret;

	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->get_configuration)
		return -EIO;

	params->ext.get_conf.config_id = config_id;
	params->ext.get_conf.data_off_size = data_offset_size;
	params->ext.get_conf.fragment = fragment;
	params->ext.get_conf.fragment_size = fragment_size;

	ret = k_mem_domain_add_partition(domain, &ipc_resp_part);
	if (ret < 0) {
		comp_err(mod->dev, "add response buffer to domain error: %d", ret);
		return ret;
	}

	ret = userspace_proxy_invoke(mod->user_ctx, USER_PROXY_MOD_CMD_GET_CONF, true);

	k_mem_domain_remove_partition(domain, &ipc_resp_part);

	/* Return status from module code operation. */
	return ret ? ret : params->status;
}

/**
 * Copy parameters to user worker accessible space.
 * Queue module set_processing_mode() operation and return its result.
 * Module set_processing_mode() code is performed in user workqueue.
 *
 * @param mod - pointer to processing module structure.
 * @param mode - processing mode to be set.
 * @return 0 for success, error otherwise.
 */
static int userspace_proxy_set_processing_mode(struct processing_module *mod,
					       enum module_processing_mode mode)
{
	struct module_params *params = user_work_get_params(mod->user_ctx);
	int ret;

	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->set_processing_mode)
		return 0;

	params->ext.proc_mode.mode = mode;
	ret = userspace_proxy_invoke(mod->user_ctx, USER_PROXY_MOD_CMD_SET_PROCMOD, false);
	if (ret)
		return ret;

	/* Return status from module code operation. */
	return params->status;
}

/**
 * Copy parameters to user worker accessible space.
 * Queue module get_processing_mode() operation and return its result.
 * Module get_processing_mode() code is performed in user workqueue.
 *
 * @param mod - pointer to processing module structure.
 * @return processing mode.
 */
static
enum module_processing_mode userspace_proxy_get_processing_mode(struct processing_module *mod)
{
	struct module_params *params = user_work_get_params(mod->user_ctx);
	int ret;

	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->get_processing_mode)
		return -EIO;

	ret = userspace_proxy_invoke(mod->user_ctx, USER_PROXY_MOD_CMD_GET_PROCMOD, false);
	if (ret)
		return ret;

	/* Return status from module code operation. */
	return params->ext.proc_mode.mode;
}

/**
 * Copy parameters to user worker accessible space.
 * Queue module is_ready_to_process() operation and return its result.
 * Module is_ready_to_process() code is performed in user workqueue.
 *
 * @param mod - pointer to processing module structure.
 * @return true if the module is ready to process
 */
static bool userspace_proxy_is_ready_to_process(struct processing_module *mod,
						struct sof_source **sources,
						int num_of_sources,
						struct sof_sink **sinks,
						int num_of_sinks)
{
	struct module_params *params = user_work_get_params(mod->user_ctx);
	int ret;

	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->is_ready_to_process)
		return generic_module_is_ready_to_process(mod, sources, num_of_sources, sinks,
							  num_of_sinks);

	params->ext.proc.sources = sources;
	params->ext.proc.num_of_sources = num_of_sources;
	params->ext.proc.sinks = sinks;
	params->ext.proc.num_of_sinks = num_of_sinks;

	ret = userspace_proxy_invoke(mod->user_ctx, USER_PROXY_MOD_CMD_PROC_READY, false);
	if (ret)
		return generic_module_is_ready_to_process(mod, sources, num_of_sources, sinks,
							  num_of_sinks);

	/* Return status from module code operation. */
	return params->status;
}

/**
 * Copy parameters to user worker accessible space.
 * Queue module bind() operation and return its result.
 * Module bind() code is performed in user workqueue.
 *
 * @param mod - pointer to processing module structure.
 * @param bind_data - pointer to bind_info structure.
 * @return 0 for success, error otherwise.
 */
static int userspace_proxy_bind(struct processing_module *mod, struct bind_info *bind_data)
{
	struct module_params *params = user_work_get_params(mod->user_ctx);
	int ret;

	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->bind)
		return 0;

	params->ext.bind_data = bind_data;
	ret = userspace_proxy_invoke(mod->user_ctx, USER_PROXY_MOD_CMD_BIND, false);
	if (ret)
		return ret;

	/* Return status from module code operation. */
	return params->status;
}

/**
 * Copy parameters to user worker accessible space.
 * Queue module unbind() operation and return its result.
 * Module unbind() code is performed in user workqueue.
 *
 * @param mod - pointer to processing module structure.
 * @param unbind_data - pointer to bind_info structure.
 * @return 0 for success, error otherwise.
 */
static int userspace_proxy_unbind(struct processing_module *mod, struct bind_info *unbind_data)
{
	struct module_params *params = user_work_get_params(mod->user_ctx);
	int ret;

	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->unbind)
		return 0;

	params->ext.bind_data = unbind_data;
	ret = userspace_proxy_invoke(mod->user_ctx, USER_PROXY_MOD_CMD_UNBIND, false);
	if (ret)
		return ret;

	/* Return status from module code operation. */
	return params->status;
}

/**
 * Copy parameters to user worker accessible space.
 * Queue module trigger() operation and return its result.
 * Module trigger() code is performed in user workqueue.
 *
 * @param mod - pointer to processing module structure.
 * @return 0 for success, error otherwise.
 */
static int userspace_proxy_trigger(struct processing_module *mod, int cmd)
{
	struct module_params *params = user_work_get_params(mod->user_ctx);
	int ret = 0;

	comp_dbg(mod->dev, "start");

	if (mod->user_ctx->interface->trigger) {
		params->ext.trigger_data = cmd;
		ret = userspace_proxy_invoke(mod->user_ctx, USER_PROXY_MOD_CMD_TRIGGER, false);
		if (ret)
			return ret;
		ret = params->status;
	}

	if (!ret)
		ret = module_adapter_set_state(mod, mod->dev, cmd);

	/* Return status from module code operation. */
	return ret;
}

/* Userspace Proxy Module API */
APP_TASK_DATA static const struct module_interface userspace_proxy_interface = {
	.init = userspace_proxy_init,
	.is_ready_to_process = userspace_proxy_is_ready_to_process,
	.prepare = userspace_proxy_prepare,
	.process = userspace_proxy_process,
	.set_configuration = userspace_proxy_set_configuration,
	.get_configuration = userspace_proxy_get_configuration,
	.set_processing_mode = userspace_proxy_set_processing_mode,
	.get_processing_mode = userspace_proxy_get_processing_mode,
	.reset = userspace_proxy_reset,
	.free = userspace_proxy_free,
	.bind = userspace_proxy_bind,
	.unbind = userspace_proxy_unbind,
	.trigger = userspace_proxy_trigger,
};
