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
#include <rtos/userspace_helper.h>
#include <utilities/array.h>
#include <zephyr/sys/sem.h>
#include <sof/audio/module_adapter/module/generic.h>

#include <sof/audio/module_adapter/library/userspace_proxy.h>
#include <rimage/sof/user/manifest.h>

LOG_MODULE_REGISTER(userspace_proxy, CONFIG_SOF_LOG_LEVEL);

/* 6f6b6f4b-6f73-7466-20e1e62b9779f003 */
SOF_DEFINE_REG_UUID(userspace_proxy);

DECLARE_TR_CTX(userspace_proxy_tr, SOF_UUID(userspace_proxy_uuid), LOG_LEVEL_INFO);

static const struct module_interface userspace_proxy_interface;

static int userspace_proxy_memory_init(struct userspace_context *user,
				       const struct comp_driver *drv)
{
	/* Add module private heap to memory partitions */
	struct k_mem_partition heap_part = { .attr = K_MEM_PARTITION_P_RW_U_RW };

	k_mem_region_align(&heap_part.start, &heap_part.size,
			   POINTER_TO_UINT(drv->user_heap->init_mem),
			   drv->user_heap->init_bytes, CONFIG_MM_DRV_PAGE_SIZE);

	tr_err(&userspace_proxy_tr, "Heap partition %p + %#zx, attr = %u",
	       UINT_TO_POINTER(heap_part.start), heap_part.size, heap_part.attr);

#if !defined(CONFIG_XTENSA_MMU_DOUBLE_MAP) && defined(CONFIG_SOF_ZEPHYR_HEAP_CACHED)
#define HEAP_PART_CACHED
	/* Add cached module private heap to memory partitions */
	struct k_mem_partition heap_cached_part = { .attr = K_MEM_PARTITION_P_RW_U_RW };

	k_mem_region_align(&heap_cached_part.start, &heap_cached_part.size,
			   POINTER_TO_UINT(sys_cache_cached_ptr_get(drv->user_heap->init_mem)),
			   drv->user_heap->init_bytes, CONFIG_MM_DRV_PAGE_SIZE);

	tr_err(&userspace_proxy_tr, "Cached heap partition %p + %#zx, attr = %u",
	       UINT_TO_POINTER(heap_cached_part.start), heap_cached_part.size,
	       heap_cached_part.attr);
#endif

	struct k_mem_partition *parts_ptr[] = {
#ifdef HEAP_PART_CACHED
		&heap_part_cached,
#endif
		&heap_part
	};

	return k_mem_domain_init(user->comp_dom, ARRAY_SIZE(parts_ptr), parts_ptr);
}

static int userspace_proxy_add_sections(struct userspace_context *user, uint32_t instance_id,
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

		ret = k_mem_domain_add_partition(user->comp_dom, &mem_partition);

		tr_err(&userspace_proxy_tr, "Add mod partition %p + %#zx, attr = %u, ret = %d",
		       UINT_TO_POINTER(mem_partition.start), mem_partition.size,
		       mem_partition.attr, ret);

		if (ret < 0)
			return ret;
	}

	lib_manager_get_instance_bss_address(instance_id, mod, &va_base, &mem_partition.size);
	mem_partition.start = POINTER_TO_UINT(va_base);
	mem_partition.attr = K_MEM_PARTITION_P_RW_U_RW;
	ret = k_mem_domain_add_partition(user->comp_dom, &mem_partition);

	tr_err(&userspace_proxy_tr, "Add bss partition %p + %#zx, attr = %u, ret = %d",
	       UINT_TO_POINTER(mem_partition.start), mem_partition.size,
	       mem_partition.attr, ret);

	return ret;
}

int userspace_proxy_create(struct userspace_context **user_ctx, const struct comp_driver *drv,
			   const struct sof_man_module *manifest, system_agent_start_fn start_fn,
			   const struct system_agent_params *agent_params,
			   const void **agent_interface, const struct module_interface **ops)
{
	struct userspace_context *user;
	struct k_mem_domain *domain;
	int ret;

	tr_err(&userspace_proxy_tr, "userspace create");

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

	ret = userspace_proxy_add_sections(user, agent_params->instance_id, manifest);
	if (ret)
		goto error_dom;

	/* Start the system agent, if provided. */
	if (start_fn) {
		ret = start_fn(agent_params, agent_interface);

		if (ret) {
			tr_err(&userspace_proxy_tr, "System agent failed with error %d.", ret);
			goto error_dom;
		}
	}

	*user_ctx = user;

	/* Store a pointer to the module's interface. For the LMDK modules, the agent places a
	 * pointer to the module interface at the address specified by agent_interface. Since this
	 * points to ops, the assignment of the module interface used by this proxy must occur
	 * after the agent has been started. For other module types, the ops parameter points to a
	 * valid module interface.
	 */
	user->interface = *ops;

	/* All calls to the module interface must pass through the proxy. Set up our own interface.
	 */
	*ops = &userspace_proxy_interface;

	return 0;

error_dom:
	rfree(domain);
error:
	sys_heap_free(drv->user_heap, user);
	return ret;
}

void userspace_proxy_destroy(const struct comp_driver *drv, struct userspace_context *user_ctx)
{
	tr_err(&userspace_proxy_tr, "userspace proxy destroy");
	rfree(user_ctx->comp_dom);
	sys_heap_free(drv->user_heap, user_ctx);
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
	comp_dbg(mod->dev, "start");

	/* Return status from module code operation. */
	return mod->user_ctx->interface->init(mod);
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
	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->prepare)
		return 0;

	return mod->user_ctx->interface->prepare(mod, sources, num_of_sources, sinks, num_of_sinks);
}

/**
 * Copy parameters to user worker accessible space.
 * Queue module init() operation and return its result.
 * Module init() code is performed in user workqueue.
 *
 * @param mod - pointer to processing module structure.
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
	if (!mod->user_ctx->interface->reset)
		return 0;

	return mod->user_ctx->interface->reset(mod);
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
	int ret = 0;

	comp_dbg(mod->dev, "start");

	if (mod->user_ctx->interface->free)
		ret = mod->user_ctx->interface->free(mod);

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
	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->set_configuration)
		return 0;

	return mod->user_ctx->interface->set_configuration(mod, config_id, pos, data_offset_size,
							   fragment, fragment_size,
							   response, response_size);
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
	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->get_configuration)
		return -EIO;

	return mod->user_ctx->interface->get_configuration(mod, config_id, data_offset_size,
							   fragment, fragment_size);
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
	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->set_processing_mode)
		return 0;

	return mod->user_ctx->interface->set_processing_mode(mod, mode);
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
	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->get_processing_mode)
		return -EIO;

	return mod->user_ctx->interface->get_processing_mode(mod);
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
	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->is_ready_to_process)
		return generic_module_is_ready_to_process(mod, sources, num_of_sources, sinks,
							  num_of_sinks);

	return mod->user_ctx->interface->is_ready_to_process(mod, sources, num_of_sources,
							     sinks, num_of_sinks);
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
	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->bind)
		return 0;

	return mod->user_ctx->interface->bind(mod, bind_data);
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
	comp_dbg(mod->dev, "start");

	if (!mod->user_ctx->interface->unbind)
		return 0;

	return mod->user_ctx->interface->unbind(mod, unbind_data);
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
	int ret = 0;

	comp_dbg(mod->dev, "start");

	if (mod->user_ctx->interface->trigger)
		ret = mod->user_ctx->interface->trigger(mod, cmd);

	if (!ret)
		ret = module_adapter_set_state(mod, mod->dev, cmd);

	/* Return status from module code operation. */
	return ret;
}

/* Userspace Module Adapter API */
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
