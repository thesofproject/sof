// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//
// Author: Jaroslaw Stelter <jaroslaw.stelter@intel.com>
// Author: Adrian Warecki <adrian.warecki@intel.com>
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
#include <rtos/userspace_helper.h>
#include <utilities/array.h>
#include <zephyr/sys/sem.h>
#include <sof/audio/module_adapter/module/generic.h>

#include <sof/audio/module_adapter/library/userspace_proxy.h>
#include <rimage/sof/user/manifest.h>


LOG_MODULE_REGISTER(modules_user_tr, CONFIG_SOF_LOG_LEVEL);

/* 7c42ce8b-0108-43d0-9137-56d660478c55 */
SOF_DEFINE_REG_UUID(modules_user);

DECLARE_TR_CTX(modules_user_tr, SOF_UUID(modules_user_uuid), LOG_LEVEL_INFO);

/* IPC works synchronously so single message queue is ok */
#define MSGQ_LEN	1

K_APPMEM_PARTITION_DEFINE(ipc_partition);
#define MAX_PARAM_SIZE	0x200

struct user_worker_data {
	struct k_msgq *ipc_in_msg_q;		/* pointer to input message queue	*/
	struct k_msgq *ipc_out_msg_q;		/* pointer to output message queue	*/
	struct k_work_user work_item;		/* ipc worker workitem			*/
	k_tid_t ipc_worker_tid;			/* ipc worker thread ID			*/
	uint8_t ipc_params[MAX_PARAM_SIZE];	/* ipc parameter buffer			*/
	uint32_t module_ref_cnt;		/* module reference count		*/
	void *p_worker_stack;			/* pointer to worker stack		*/
};

struct user_security_domain {
	struct k_work_user_q ipc_user_work_q;
	struct k_msgq in_msgq;
	struct k_msgq out_msgq;
};

static struct user_security_domain security_domain[CONFIG_SEC_DOMAIN_MAX_NUMBER];
/*
 * There is one instance of IPC user worker per security domain.
 * Keep pointer to it here
 */
APP_USER_BSS static struct user_worker_data *worker_data[CONFIG_SEC_DOMAIN_MAX_NUMBER];

struct module_agent_params {
	system_agent_start_fn start_fn;
	uintptr_t entry_point;
	uint32_t module_id;
	uint32_t instance_id;
	uint32_t core_id;
	uint32_t log_handle;
	byte_array_t mod_cfg;
	const void *iface;
};

struct module_large_cfg_set_params {
	uint32_t config_id;
	enum module_cfg_fragment_position pos;
	uint32_t data_off_size;
	const uint8_t *fragment;
	size_t fragment_size;
	uint8_t *response;
	size_t response_size;
};

struct module_large_cfg_get_params {
	uint32_t config_id;
	uint32_t *data_off_size;
	uint8_t *fragment;
	size_t fragment_size;
};

struct module_processing_mode_params {
	enum module_processing_mode mode;
};

struct module_process_params {
	struct sof_source **sources;
	int num_of_sources;
	struct sof_sink **sinks;
	int num_of_sinks;
};

struct module_params {
	uint32_t cmd;
	int status;
	struct processing_module *mod;
	struct userspace_context *user;
	union {
		struct module_agent_params		agent;
		struct module_large_cfg_set_params	set_conf;
		struct module_large_cfg_get_params	get_conf;
		struct module_processing_mode_params	proc_mode;
		struct module_process_params		proc;
		struct bind_info			*bind_data;
		int					trigger_data;
	} ext;
};

static const struct module_interface userspace_proxy_adapter_interface;

enum {
	MODULE_CMD_AGENT_START,
	MODULE_CMD_INIT,
	MODULE_CMD_PREPARE,
	MODULE_CMD_PROC_READY,
	MODULE_CMD_PROCESS,
	MODULE_CMD_SET_PROCMOD,
	MODULE_CMD_GET_PROCMOD,
	MODULE_CMD_SET_CONF,
	MODULE_CMD_GET_CONF,
	MODULE_CMD_BIND,
	MODULE_CMD_UNBIND,
	MODULE_CMD_RESET,
	MODULE_CMD_FREE,
	MODULE_CMD_TRIGGER
};

static void user_worker_handler(struct k_work_user *work_item)
{
	const struct module_interface *ops;

	struct user_worker_data *wd = CONTAINER_OF(work_item, struct user_worker_data, work_item);
	struct module_params *params = (struct module_params *)wd->ipc_params;
	while(1) {
		k_msgq_get(wd->ipc_in_msg_q, params, K_FOREVER);
		ops = params->user->interface;

		switch(params->cmd) {
		case MODULE_CMD_AGENT_START:
			params->status = params->ext.agent.start_fn(params->ext.agent.entry_point,
								    params->ext.agent.module_id,
								    params->ext.agent.instance_id,
								    params->ext.agent.core_id,
								    params->ext.agent.log_handle,
								    &params->ext.agent.mod_cfg,
								    &params->ext.agent.iface);
			break;

		case MODULE_CMD_INIT:
			params->status = ops->init(params->mod);
			break;

		case MODULE_CMD_PREPARE:
			params->status = ops->prepare(params->mod, params->ext.proc.sources,
						      params->ext.proc.num_of_sources,
						      params->ext.proc.sinks,
						      params->ext.proc.num_of_sinks);
			break;

		case MODULE_CMD_PROC_READY:
			params->status = ops->is_ready_to_process(params->mod,
								  params->ext.proc.sources,
								  params->ext.proc.num_of_sources,
								  params->ext.proc.sinks,
								  params->ext.proc.num_of_sinks);
			break;

		case MODULE_CMD_BIND:
			params->status = ops->bind(params->mod, params->ext.bind_data);
			break;

		case MODULE_CMD_UNBIND:
			params->status = ops->unbind(params->mod, params->ext.bind_data);
			break;

		case MODULE_CMD_RESET:
			params->status = ops->reset(params->mod);
			break;

		case MODULE_CMD_FREE:
			params->status = ops->free(params->mod);
			break;

		case MODULE_CMD_SET_CONF:
			params->status = ops->set_configuration(params->mod,
								params->ext.set_conf.config_id,
								params->ext.set_conf.pos,
								params->ext.set_conf.data_off_size,
								params->ext.set_conf.fragment,
								params->ext.set_conf.fragment_size,
								params->ext.set_conf.response,
								params->ext.set_conf.response_size);
			break;

		case MODULE_CMD_GET_CONF:
			params->status = ops->get_configuration(params->mod,
								params->ext.get_conf.config_id,
								params->ext.get_conf.data_off_size,
								params->ext.get_conf.fragment,
								params->ext.get_conf.fragment_size);
			break;

		case MODULE_CMD_SET_PROCMOD:
			params->status = ops->set_processing_mode(params->mod,
								  params->ext.proc_mode.mode);
			break;

		case MODULE_CMD_GET_PROCMOD:
			params->ext.proc_mode.mode = ops->get_processing_mode(params->mod);
			break;

		case MODULE_CMD_TRIGGER:
			params->status = ops->trigger(params->mod, params->ext.trigger_data);
			break;

		default:
			params->status = EINVAL;
			break;
		}
		k_msgq_put(wd->ipc_out_msg_q, params, K_FOREVER);

		k_yield();
	}
}

static int user_worker_msgq_alloc(struct user_security_domain *security_domain,
				  struct user_worker_data *worker_data)
{
	char *buffer;

	buffer = rzalloc(SOF_MEM_FLAG_USER_SHARED_BUFFER, MAX_PARAM_SIZE * 2);
	if (!buffer)
		return -ENOMEM;

	k_msgq_init(&security_domain->in_msgq, buffer, MAX_PARAM_SIZE, 1);
	k_msgq_init(&security_domain->out_msgq, (buffer + MAX_PARAM_SIZE), MAX_PARAM_SIZE, 1);

	worker_data->ipc_in_msg_q = &security_domain->in_msgq;
	worker_data->ipc_out_msg_q = &security_domain->out_msgq;

	return 0;
}

static void user_worker_msgq_free(struct user_worker_data *worker_data)
{
	/* ipc_in_msg_q->buffer_start points to whole buffer allocated for message queues */
	rfree(worker_data->ipc_in_msg_q->buffer_start);
}
static int user_worker_init(struct userspace_context *user)
{
	uint32_t sd_id = user->security_domain_id;
	struct user_worker_data *wd = worker_data[sd_id];
	struct user_security_domain *sd = &security_domain[sd_id];
	int ret;
	tr_dbg(&modules_user_tr, "start");

	if (sd_id >= CONFIG_SEC_DOMAIN_MAX_NUMBER) {
		tr_err(&modules_user_tr, "Invalid security_domain_id");
		return -EINVAL;
	}

	/* If worker_data not initialized this is first userspace module in security domain  */
	if (!wd) {
		wd = rzalloc(SOF_MEM_FLAG_USER_SHARED_BUFFER, sizeof(*wd));
		if (!wd)
			return -ENOMEM;

		/* Store worker data in processing module structure */
		worker_data[sd_id] = wd;
		user->wrk_ctx = wd;

		ret = user_worker_msgq_alloc(sd, wd);
		if (ret < 0)
			goto err_worker;

		wd->p_worker_stack = user_stack_allocate(CONFIG_SOF_STACK_SIZE, K_USER);
		if (!wd->p_worker_stack) {
			tr_err(&modules_user_tr, "stack alloc failed");
			ret = -ENOMEM;
			goto err_msgq;
		}

		/* Create User Mode work queue start before submit. */
		k_work_user_queue_start(&sd->ipc_user_work_q, wd->p_worker_stack,
					CONFIG_SOF_STACK_SIZE, 0, NULL);

		wd->ipc_worker_tid =
			k_work_user_queue_thread_get(&sd->ipc_user_work_q);
	}

	/* Init module memory domain. */
	ret = k_mem_domain_add_thread(user->comp_dom, wd->ipc_worker_tid);
	if (ret < 0) {
		tr_err(&modules_user_tr, "failed to add memory domain: error: %d", ret);
		goto err_init;
	}

	if (!wd->module_ref_cnt) {
		k_work_user_init(&wd->work_item, user_worker_handler);

		/* Grant a thread access to a kernel object (IPC msg. data). */
		k_thread_access_grant(wd->ipc_worker_tid,
				      wd->ipc_in_msg_q,
				      wd->ipc_out_msg_q);

		/* Submit work item to the queue */
		ret = k_work_user_submit_to_queue(&sd->ipc_user_work_q, &wd->work_item);
		if (ret < 0)
			goto err_init;
	}
	wd->module_ref_cnt++;
	return 0;

err_init:
	/* Exit if another module is already using this worker. */
	if (wd->module_ref_cnt)
		return ret;

	k_thread_abort(wd->ipc_worker_tid);
	user_stack_free(wd->p_worker_stack);

err_msgq:
	user_worker_msgq_free(wd);

err_worker:
	rfree(wd);
	worker_data[sd_id] = NULL;
	return ret;
}

static void user_worker_free(struct userspace_context *user)
{
	uint32_t sd_id = user->security_domain_id;
	struct user_worker_data *wd = worker_data[sd_id];

	if (sd_id >= CONFIG_SEC_DOMAIN_MAX_NUMBER || !wd) {
		tr_err(&modules_user_tr, "Invalid security_domain_id");
		return;
	}

	/* Module removed so decrement counter */
	wd->module_ref_cnt--;
	/* Free worker resources if no more active user space modules */
	if (wd->module_ref_cnt == 0) {
		k_thread_abort(wd->ipc_worker_tid);
		user_stack_free(wd->p_worker_stack);
		k_msgq_cleanup(wd->ipc_in_msg_q);
		k_msgq_cleanup(wd->ipc_out_msg_q);
		user_worker_msgq_free(wd);
		rfree(wd);
		worker_data[sd_id] = NULL;
	}
}

static int user_worker_call(struct userspace_context *user, struct processing_module *mod,
			    uint32_t cmd, struct module_params *params)
{
	struct user_worker_data *wr_data = user->wrk_ctx;
	int ret;

	params->user = user;
	params->mod = mod;
	params->cmd = cmd;

	/* Switch worker thread to module memory domain */
	ret = k_mem_domain_add_thread(user->comp_dom, wr_data->ipc_worker_tid);
	if (ret < 0) {
		comp_err(mod->dev, "failed to switch memory domain: error: %d", ret);
		return ret;
	}

	ret = k_msgq_put(wr_data->ipc_in_msg_q, params, K_FOREVER);
	if (ret < 0) {
		comp_err(mod->dev, "k_msgq_put(): error: %d", ret);
		return ret;
	}


	ret = k_msgq_get(wr_data->ipc_out_msg_q, params, K_FOREVER);
	if (ret < 0)
		comp_err(mod->dev, "k_msgq_get(): error: %d", ret);

	return ret;
}

static int userspace_proxy_memory_init(struct userspace_context *user,
				       const struct comp_driver *drv)
{
	const uintptr_t shd_addr = get_shared_buffer_heap_start();
	const size_t shd_size = get_shared_buffer_heap_size();
	uintptr_t addr_aligned;
	size_t size_aligned;

	/* Add shared heap uncached and cached space to memory partitions */
	struct k_mem_partition parts[3];
	struct k_mem_partition *parts_ptr[] = {
			&ipc_partition,
			&parts[0],
#ifndef CONFIG_XTENSA_MMU_DOUBLE_MAP
			&parts[1],
#endif /* CONFIG_XTENSA_MMU_DOUBLE_MAP */
			&parts[2]
		};
	k_mem_region_align(&addr_aligned, &size_aligned, shd_addr, shd_size,
			   CONFIG_MM_DRV_PAGE_SIZE);
	parts[0].start = addr_aligned;
	parts[0].size = size_aligned;
	parts[0].attr = K_MEM_PARTITION_P_RW_U_RW;
	
#ifndef CONFIG_XTENSA_MMU_DOUBLE_MAP
	k_mem_region_align(&addr_aligned, &size_aligned,
			   POINTER_TO_UINT(sys_cache_cached_ptr_get(UINT_TO_POINTER(shd_addr))),
			   shd_size, CONFIG_MM_DRV_PAGE_SIZE);
	parts[1].start = addr_aligned;
	parts[1].size = size_aligned;
	parts[1].attr = K_MEM_PARTITION_P_RW_U_RW;
#endif /* CONFIG_XTENSA_MMU_DOUBLE_MAP */

	/* Add module private heap to memory partitions */
	k_mem_region_align(&addr_aligned, &size_aligned, POINTER_TO_UINT(drv->user_heap->init_mem),
			   DRV_HEAP_SIZE, CONFIG_MM_DRV_PAGE_SIZE);
	parts[2].start = addr_aligned;
	parts[2].size = size_aligned;
	parts[2].attr = K_MEM_PARTITION_P_RW_U_RW;

	return k_mem_domain_init(user->comp_dom, ARRAY_SIZE(parts_ptr), parts_ptr);
}

static int userspace_proxy_add_sections(struct userspace_context *user, uint32_t instance_id,
					 const struct sof_man_module *const mod)
{
	uint32_t mem_partition;
	void* va_base;
	size_t size;
	int idx, ret;

	for (idx = 0; idx < ARRAY_SIZE(mod->segment); ++idx) {
		if (!mod->segment[idx].flags.r.load)
			continue;

		mem_partition = K_MEM_PARTITION_P_RO_U_RO;

		if (mod->segment[idx].flags.r.code)
			mem_partition = K_MEM_PARTITION_P_RX_U_RX;
		else if (!mod->segment[idx].flags.r.readonly)
			mem_partition = K_MEM_PARTITION_P_RW_U_RW;

		va_base = UINT_TO_POINTER(mod->segment[idx].v_base_addr);
		size = mod->segment[idx].flags.r.length * CONFIG_MM_DRV_PAGE_SIZE;

		ret = user_add_memory(user->comp_dom, POINTER_TO_UINT(va_base), size, mem_partition);
		if (ret < 0)
			return ret;
	}

	lib_manager_get_instance_bss_address(instance_id, mod, &va_base, &size);
	return user_add_memory(user->comp_dom, POINTER_TO_UINT(va_base), size,
			       K_MEM_PARTITION_P_RW_U_RW);
}

int userspace_proxy_create(struct userspace_context **user_ctx, const struct comp_driver *drv,
			   const struct sof_man_module *manifest, system_agent_start_fn start_fn,
			   uintptr_t entry_point, uint32_t module_id, uint32_t instance_id,
			   uint32_t core_id, uint32_t log_handle, byte_array_t *mod_cfg,
			   const void **adapter)
{
	struct userspace_context *user;
	struct k_mem_domain *domain;
	int ret;

	tr_dbg(&modules_user_tr, "userspace create");

	user = sys_heap_alloc(drv->user_heap, sizeof(struct userspace_context));
	if (!user)
		return -ENOMEM;

	/* Allocate memory domain struct */
	domain = rzalloc(SOF_MEM_FLAG_KERNEL, sizeof(*domain));
	if (!domain) {
		ret = -ENOMEM;
		goto error;
	}
	user->comp_dom = domain;

	ret = userspace_proxy_memory_init(user, drv);
	if (ret)
		goto error_dom;

	ret = userspace_proxy_add_sections(user, instance_id, manifest);
	if (ret)
		goto error_dom;
	
	ret = user_worker_init(user);
	if (ret)
		goto error_dom;
	
	struct user_worker_data *wr_data = user->wrk_ctx;
	struct module_params *params = (struct module_params *)wr_data->ipc_params;
	params->ext.agent.start_fn = start_fn;
	params->ext.agent.entry_point = entry_point;
	params->ext.agent.module_id = module_id;
	params->ext.agent.instance_id = instance_id;
	params->ext.agent.core_id = core_id;
	params->ext.agent.log_handle = log_handle;
	params->ext.agent.mod_cfg = *mod_cfg;

	/* Add cfg buffer to memory domain */
	ret = user_add_memory(domain, POINTER_TO_UINT(mod_cfg->data), mod_cfg->size << 2,
			      K_MEM_PARTITION_P_RW_U_RW);
	if (ret)
		goto error_worker;

	ret = user_worker_call(user, NULL, MODULE_CMD_AGENT_START, params);
	if (ret)
		goto error_worker;

	/* Remove cfg buffer from memory domain (Is this needed?) */
	ret = user_remove_memory(domain, POINTER_TO_UINT(mod_cfg->data),
				 mod_cfg->size << 2);
	if (ret)
		goto error_worker;

	*adapter = params->ext.agent.iface;
	*user_ctx = user;

	/* TODO: Must be after assign of adapter! */
	user->interface = drv->adapter_ops;
	((struct comp_driver *)drv)->adapter_ops = &userspace_proxy_adapter_interface;

	return params->status;

error_worker:
	user_worker_free(user);
error_dom:
	rfree(domain);
error:
	sys_heap_free(drv->user_heap, user);
	return ret;
}

void userspace_proxy_destroy(const struct comp_driver *drv, struct userspace_context *user_ctx)
{
	tr_dbg(&modules_user_tr, "userspace proxy destroy");
	user_worker_free(user_ctx);
	sys_heap_free(drv->user_heap, user_ctx);
}

/**
 * Copy parameters to user worker accessible space.
 * Queue module init() operation and return its result.
 * Module init() code is performed in user workqueue.
 *
 * @param mod - pointer to processing module structure.
 * @param module_entry_point - module entry point address from
 *		its manifest
 * @param module_id - module id
 * @param instance_id - instance id
 * @param log_handle - log handler for module
 * @param mod_cfg - module configuration
 * @return 0 for success, error otherwise.
 */
static int userspace_proxy_init(struct processing_module *mod)
{
	struct user_worker_data *wr_data = mod->user_ctx->wrk_ctx;
	struct module_data *md = &mod->priv;
	struct module_params *params = (struct module_params *)wr_data->ipc_params;
	struct k_mem_domain *domain = mod->user_ctx->comp_dom;
	int ret;

	comp_dbg(mod->dev, "start");

	/* Add cfg buffer to memory domain */
	ret = user_add_memory(domain, POINTER_TO_UINT(md->cfg.init_data),
			      sizeof(struct module_config), K_MEM_PARTITION_P_RW_U_RW);
	if (ret < 0)
		return ret;

	ret = user_worker_call(mod->user_ctx, mod, MODULE_CMD_INIT, params);
	if (ret < 0)
		return ret;

	/* Remove cfg buffer from memory domain (Is this needed?) */
	ret = user_remove_memory(domain, POINTER_TO_UINT(md->cfg.init_data),
				 sizeof(struct module_config));
	if (ret < 0)
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
	struct user_worker_data *wr_data = mod->user_ctx->wrk_ctx;
	struct module_params *params = (struct module_params *)wr_data->ipc_params;
	int ret;

	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->prepare)
		return 0;

	params->ext.proc.sources = sources;
	params->ext.proc.num_of_sources = num_of_sources;
	params->ext.proc.sinks = sinks;
	params->ext.proc.num_of_sinks = num_of_sinks;

	ret = user_worker_call(mod->user_ctx, mod, MODULE_CMD_PREPARE, params);
	if (ret < 0)
		return ret;

	/* Return status from module code operation. */
	return params->status;
}

/**
 * Copy parameters to user worker accessible space.
 * Queue module init() operation and return its result.
 * Module init() code is performed in user workqueue.
 *
 * @param mod - pointer to processing module structure.
 * @param module_entry_point - module entry point address from
 *		its manifest
 * @param module_id - module id
 * @param instance_id - instance id
 * @param log_handle - log handler for module
 * @param mod_cfg - module configuration
 * @return 0 for success, error otherwise.
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
	struct user_worker_data *wr_data = mod->user_ctx->wrk_ctx;
	struct module_params *params = (struct module_params *)wr_data->ipc_params;
	int ret;

	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->reset)
		return 0;

	ret = user_worker_call(mod->user_ctx, mod, MODULE_CMD_RESET, params);
	if (ret < 0)
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
	struct user_worker_data *wr_data = mod->user_ctx->wrk_ctx;
	struct module_params *params = (struct module_params *)wr_data->ipc_params;
	int ret;

	comp_dbg(mod->dev, "start");
	params->status = 0;

	if (mod->user_ctx->interface->free) {
		ret = user_worker_call(mod->user_ctx, mod, MODULE_CMD_FREE, params);
		if (ret < 0)
			return ret;
	}

	/* Destroy workqueue if this was last active userspace module */
	userspace_proxy_destroy(mod->dev->drv, mod->user_ctx);
	mod->user_ctx = NULL;

	/* Return status from module code operation. */
	return params->status;
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
	struct user_worker_data *wr_data = mod->user_ctx->wrk_ctx;
	struct module_params *params = (struct module_params *)wr_data->ipc_params;
	struct k_mem_domain *domain = mod->user_ctx->comp_dom;
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

	/* Give read access to the fragment buffer (It should be 4kB)*/
	ret = user_add_memory(domain, POINTER_TO_UINT(fragment), fragment_size,
			      K_MEM_PARTITION_P_RO_U_RO);
	if (ret < 0) {
		comp_err(mod->dev, "add fragment to domain error: %d", ret);
		return ret;
	}

	/* Give write access to the response buffer (It should be 4kB)*/
	ret = user_add_memory(domain, POINTER_TO_UINT(response), response_size,
			      K_MEM_PARTITION_P_RW_U_RW);
	if (ret < 0) {
		comp_err(mod->dev, "add response to domain error: %d", ret);
		goto err_resp;
	}

	ret = user_worker_call(mod->user_ctx, mod, MODULE_CMD_SET_CONF, params);
	if (!ret)
		ret = params->status;

	/* Remove access to buffers */
	user_remove_memory(domain, POINTER_TO_UINT(response), response_size);
err_resp:
	user_remove_memory(domain, POINTER_TO_UINT(fragment), fragment_size);

	/* Return status from module code operation. */
	return ret;
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
	struct user_worker_data *wr_data = mod->user_ctx->wrk_ctx;
	struct module_params *params = (struct module_params *)wr_data->ipc_params;
	struct k_mem_domain *domain = mod->user_ctx->comp_dom;
	int ret;

	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->get_configuration)
		return -EIO;

	params->ext.get_conf.config_id = config_id;
	params->ext.get_conf.data_off_size = data_offset_size;
	params->ext.get_conf.fragment = fragment;
	params->ext.get_conf.fragment_size = fragment_size;

	/* Give write access to the fragment buffer (It should be 4kB)*/
	ret = user_add_memory(domain, POINTER_TO_UINT(fragment), fragment_size,
			      K_MEM_PARTITION_P_RW_U_RW);
	if (ret < 0) {
		comp_err(mod->dev, "add fragment to domain error: %d", ret);
		return ret;
	}

	ret = user_worker_call(mod->user_ctx, mod, MODULE_CMD_GET_CONF, params);

	/* Remove access to the buffer */
	user_remove_memory(domain, POINTER_TO_UINT(fragment), fragment_size);

	if (ret < 0)
		return ret;

	/* Return status from module code operation. */
	return params->status;
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
	struct user_worker_data *wr_data = mod->user_ctx->wrk_ctx;
	struct module_params *params = (struct module_params *)wr_data->ipc_params;
	int ret;

	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->set_processing_mode)
		return 0;

	params->ext.proc_mode.mode = mode;
	ret = user_worker_call(mod->user_ctx, mod, MODULE_CMD_SET_PROCMOD, params);
	if (ret < 0)
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
static enum module_processing_mode userspace_proxy_get_processing_mode(struct processing_module *mod)
{
	struct user_worker_data *wr_data = mod->user_ctx->wrk_ctx;
	struct module_params *params = (struct module_params *)wr_data->ipc_params;
	int ret;

	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->get_processing_mode)
		return -EIO;

	ret = user_worker_call(mod->user_ctx, mod, MODULE_CMD_GET_PROCMOD, params);
	if (ret < 0)
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
	struct user_worker_data *wr_data = mod->user_ctx->wrk_ctx;
	struct module_params *params = (struct module_params *)wr_data->ipc_params;
	int ret;

	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->is_ready_to_process)
		return generic_module_is_ready_to_process(mod, sources, num_of_sources, sinks,
							  num_of_sinks);

	params->ext.proc.sources = sources;
	params->ext.proc.num_of_sources = num_of_sources;
	params->ext.proc.sinks = sinks;
	params->ext.proc.num_of_sinks = num_of_sinks;

	ret = user_worker_call(mod->user_ctx, mod, MODULE_CMD_PROC_READY, params);
	if (ret < 0)
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
	struct user_worker_data *wr_data = mod->user_ctx->wrk_ctx;
	struct module_params *params = (struct module_params *)wr_data->ipc_params;
	int ret;

	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->bind)
		return 0;

	params->ext.bind_data = bind_data;
	ret = user_worker_call(mod->user_ctx, mod, MODULE_CMD_BIND, params);
	if (ret < 0)
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
	struct user_worker_data *wr_data = mod->user_ctx->wrk_ctx;
	struct module_params *params = (struct module_params *)wr_data->ipc_params;
	int ret;

	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->unbind)
		return 0;

	params->ext.bind_data = unbind_data;
	ret = user_worker_call(mod->user_ctx, mod, MODULE_CMD_UNBIND, params);
	if (ret < 0)
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
	struct user_worker_data *wr_data = mod->user_ctx->wrk_ctx;
	struct module_params *params = (struct module_params *)wr_data->ipc_params;
	int ret = 0;

	comp_dbg(mod->dev, "start");

	if (mod->user_ctx->interface->trigger) {
		params->ext.trigger_data = cmd;
		ret = user_worker_call(mod->user_ctx, mod, MODULE_CMD_TRIGGER, params);
		if (ret < 0)
			return ret;
		ret = params->status;
	}

	if (!ret)
		ret = module_adapter_set_state(mod, mod->dev, cmd);

	/* Return status from module code operation. */
	return ret;
}

/* Userspace Module Adapter API */
APP_USER_DATA static const struct module_interface userspace_proxy_adapter_interface = {
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
