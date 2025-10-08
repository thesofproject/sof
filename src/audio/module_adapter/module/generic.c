// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>

/*
 * \file generic.c
 * \brief Generic Codec API
 * \author Marcin Rajwa <marcin.rajwa@linux.intel.com>
 *
 */

#include <rtos/symbol.h>

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/data_blob.h>
#include <sof/lib/fast-get.h>

/* The __ZEPHYR__ condition is to keep cmocka tests working */
#if CONFIG_MODULE_MEMORY_API_DEBUG && defined(__ZEPHYR__)
#define MEM_API_CHECK_THREAD(res) __ASSERT((res)->rsrc_mngr == k_current_get(), \
		"Module memory API operation from wrong thread")
#else
#define MEM_API_CHECK_THREAD(res)
#endif

LOG_MODULE_DECLARE(module_adapter, CONFIG_SOF_LOG_LEVEL);

int module_load_config(struct comp_dev *dev, const void *cfg, size_t size)
{
	int ret;
	struct module_config *dst;
	/* loadable module must use module adapter */
	struct processing_module *mod = comp_mod(dev);
	struct module_data *md = &mod->priv;

	comp_dbg(dev, "entry");

	if (!cfg || !size) {
		comp_err(dev, "wrong input params! dev %zx, cfg %zx size %zu",
			 (size_t)dev, (size_t)cfg, size);
		return -EINVAL;
	}

	dst = &md->cfg;

	if (!dst->data) {
		/* No space for config available yet, allocate now */
		dst->data = rballoc(SOF_MEM_FLAG_USER, size);
	} else if (dst->size != size) {
		/* The size allocated for previous config doesn't match the new one.
		 * Free old container and allocate new one.
		 */
		rfree(dst->data);
		dst->data = rballoc(SOF_MEM_FLAG_USER, size);
	}
	if (!dst->data) {
		comp_err(dev, "failed to allocate space for setup config.");
		return -ENOMEM;
	}

	ret = memcpy_s(dst->data, size, cfg, size);
	assert(!ret);

	/* Config loaded, mark it as valid */
	dst->size = size;
	dst->avail = true;

	comp_dbg(dev, "done");
	return ret;
}

int module_init(struct processing_module *mod)
{
	int ret;
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	const struct module_interface *const interface = dev->drv->adapter_ops;

	comp_dbg(dev, "entry");

#if CONFIG_IPC_MAJOR_3
	if (mod->priv.state == MODULE_INITIALIZED)
		return 0;
	if (mod->priv.state > MODULE_INITIALIZED)
		return -EPERM;
#endif
	if (!interface) {
		comp_err(dev, "module interface not defined");
		return -EIO;
	}

	/* check interface, there must be one and only one of processing procedure */
	if (!interface->init ||
	    (!!interface->process + !!interface->process_audio_stream +
	     !!interface->process_raw_data < 1)) {
		comp_err(dev, "comp is missing mandatory interfaces");
		return -EIO;
	}

	/* Init memory list */
	list_init(&md->resources.res_list);
	list_init(&md->resources.free_cont_list);
	list_init(&md->resources.cont_chunk_list);
	md->resources.heap_usage = 0;
	md->resources.heap_high_water_mark = 0;
#if CONFIG_MODULE_MEMORY_API_DEBUG && defined(__ZEPHYR__)
	md->resources.rsrc_mngr = k_current_get();
#endif
	/* Now we can proceed with module specific initialization */
	ret = interface->init(mod);
	if (ret) {
		comp_err(dev, "error %d: module specific init failed", ret);
		return ret;
	}

	comp_dbg(dev, "done");
#if CONFIG_IPC_MAJOR_3
	md->state = MODULE_INITIALIZED;
#endif

	return 0;
}

struct container_chunk {
	struct list_item chunk_list;
	struct module_resource containers[CONFIG_MODULE_MEMORY_API_CONTAINER_CHUNK_SIZE];
};

static struct module_resource *container_get(struct processing_module *mod)
{
	struct module_resources *res = &mod->priv.resources;
	struct module_resource *container;

	if (list_is_empty(&res->free_cont_list)) {
		struct container_chunk *chunk = rzalloc(SOF_MEM_FLAG_USER, sizeof(*chunk));
		int i;

		if (!chunk) {
			comp_err(mod->dev, "allocating more containers failed");
			return NULL;
		}

		list_item_append(&chunk->chunk_list, &res->cont_chunk_list);
		for (i = 0; i < ARRAY_SIZE(chunk->containers); i++)
			list_item_append(&chunk->containers[i].list, &res->free_cont_list);
	}

	container = list_first_item(&res->free_cont_list, struct module_resource, list);
	list_item_del(&container->list);
	return container;
}

static void container_put(struct processing_module *mod, struct module_resource *container)
{
	struct module_resources *res = &mod->priv.resources;

	list_item_append(&container->list, &res->free_cont_list);
}

/**
 * Allocates aligned buffer memory block for module.
 * @param mod		Pointer to the module this memory block is allocatd for.
 * @param bytes		Size in bytes.
 * @param alignment	Alignment in bytes.
 * @return Pointer to the allocated memory or NULL if failed.
 *
 * The allocated memory is automatically freed when the module is
 * unloaded. The back-end, rballoc(), always aligns the memory to
 * PLATFORM_DCACHE_ALIGN at the minimum.
 */
void *mod_balloc_align(struct processing_module *mod, size_t size, size_t alignment)
{
	struct module_resources *res = &mod->priv.resources;
	struct module_resource *container;
	void *ptr;

	MEM_API_CHECK_THREAD(res);

	container = container_get(mod);
	if (!container)
		return NULL;

	if (!size) {
		comp_err(mod->dev, "requested allocation of 0 bytes.");
		container_put(mod, container);
		return NULL;
	}

#if CONFIG_SOF_PACOVR
	/* do we need to use the dynamic heap or the static heap? */
	if (mod->priv.state != MODULE_INITIALIZED) {
		/* static allocator */
		ptr = pacovr_static_alloc(mod->dev->pipeline->pacovr, size);
	} else {
		/* dynamic allocator */
		ptr = pacovr_dynamic_alloc_align(mod->dev->pipeline->pacovr, size, alignment);
	}
#else
	/* Allocate memory for module */
	ptr = rballoc_align(SOF_MEM_FLAG_USER, size, alignment);
#endif

	if (!ptr) {
		comp_err(mod->dev, "Failed to alloc %zu bytes %zu alignment for comp %#x.",
			 size, alignment, dev_comp_id(mod->dev));
		container_put(mod, container);
		return NULL;
	}
	/* Store reference to allocated memory */
	container->ptr = ptr;
	container->size = size;
	container->type = MOD_RES_HEAP;
	list_item_prepend(&container->list, &res->res_list);

	res->heap_usage += size;
	if (res->heap_usage > res->heap_high_water_mark)
		res->heap_high_water_mark = res->heap_usage;

	return ptr;
}
EXPORT_SYMBOL(mod_balloc_align);

/**
 * Allocates aligned memory block for module.
 * @param mod		Pointer to the module this memory block is allocatd for.
 * @param bytes		Size in bytes.
 * @param alignment	Alignment in bytes.
 * @return Pointer to the allocated memory or NULL if failed.
 *
 * The allocated memory is automatically freed when the module is unloaded.
 */
void *mod_alloc_align(struct processing_module *mod, size_t size, size_t alignment)
{
	struct module_resources *res = &mod->priv.resources;
	struct module_resource *container;
	void *ptr;

	MEM_API_CHECK_THREAD(res);

	container = container_get(mod);
	if (!container)
		return NULL;

	if (!size) {
		comp_err(mod->dev, "requested allocation of 0 bytes.");
		container_put(mod, container);
		return NULL;
	}

#if CONFIG_SOF_PACOVR
	/* do we need to use the dynamic heap or the static heap? */
	if (mod->priv.state != MODULE_INITIALIZED) {
		/* static allocator */
		ptr = pacovr_static_alloc(mod->dev->pipeline->pacovr, size);
	} else {
		/* dynamic allocator */
		ptr = pacovr_dynamic_alloc_align(mod->dev->pipeline->pacovr, size, alignment);
	}
#else
	/* Allocate memory for module */
	ptr = rmalloc_align(SOF_MEM_FLAG_USER, size, alignment);
#endif

	if (!ptr) {
		comp_err(mod->dev, "Failed to alloc %zu bytes %zu alignment for comp %#x.",
			 size, alignment, dev_comp_id(mod->dev));
		container_put(mod, container);
		return NULL;
	}
	/* Store reference to allocated memory */
	container->ptr = ptr;
	container->size = size;
	container->type = MOD_RES_HEAP;
	list_item_prepend(&container->list, &res->res_list);

	res->heap_usage += size;
	if (res->heap_usage > res->heap_high_water_mark)
		res->heap_high_water_mark = res->heap_usage;

	return ptr;
}
EXPORT_SYMBOL(mod_alloc_align);

/**
 * Creates a blob handler and releases it when the module is unloaded
 * @param mod	Pointer to module this memory block is allocated for.
 * @return Pointer to the created data blob handler
 *
 * Like comp_data_blob_handler_new() but the handler is automatically freed.
 */
#if CONFIG_COMP_BLOB
struct comp_data_blob_handler *
mod_data_blob_handler_new(struct processing_module *mod)
{
	struct module_resources *res = &mod->priv.resources;
	struct comp_data_blob_handler *bhp;
	struct module_resource *container;

	MEM_API_CHECK_THREAD(res);

	container = container_get(mod);
	if (!container)
		return NULL;

	bhp = comp_data_blob_handler_new_ext(mod->dev, false, NULL, NULL);
	if (!bhp) {
		container_put(mod, container);
		return NULL;
	}

	container->bhp = bhp;
	container->size = 0;
	container->type = MOD_RES_BLOB_HANDLER;
	list_item_prepend(&container->list, &res->res_list);

	return bhp;
}
EXPORT_SYMBOL(mod_data_blob_handler_new);
#endif

/**
 * Make a module associated shared SRAM copy of DRAM read-only data.
 * @param mod	Pointer to module this copy is allocated for.
 * @return Pointer to the SRAM copy.
 *
 * Like fast_get() but the handler is automatically freed.
 */
#if CONFIG_FAST_GET
const void *mod_fast_get(struct processing_module *mod, const void * const dram_ptr, size_t size)
{
	struct module_resources *res = &mod->priv.resources;
	struct module_resource *container;
	const void *ptr;

	MEM_API_CHECK_THREAD(res);

	container = container_get(mod);
	if (!container)
		return NULL;

	ptr = fast_get(dram_ptr, size);
	if (!ptr) {
		container_put(mod, container);
		return NULL;
	}

	container->sram_ptr = ptr;
	container->size = 0;
	container->type = MOD_RES_FAST_GET;
	list_item_prepend(&container->list, &res->res_list);

	return ptr;
}
EXPORT_SYMBOL(mod_fast_get);
#endif

static int free_contents(struct processing_module *mod, struct module_resource *container)
{
	struct module_resources *res = &mod->priv.resources;

	switch (container->type) {
	case MOD_RES_HEAP:
#if CONFIG_SOF_PACOVR
	/* do we need to use the scratch heap or the batch heap? */
	if (mod->priv.state != MODULE_INITIALIZED) {
		/* static allocator */
		pacovr_static_free(mod->dev->pipeline->pacovr, container->ptr);
	} else {
		/* dynamic allocator */
		pacovr_dynamic_free(mod->dev->pipeline->pacovr, container->ptr);
	}
#else
		rfree(container->ptr);
#endif
		res->heap_usage -= container->size;
		return 0;
#if CONFIG_COMP_BLOB
	case MOD_RES_BLOB_HANDLER:
		comp_data_blob_handler_free(container->bhp);
		return 0;
#endif
#if CONFIG_FAST_GET
	case MOD_RES_FAST_GET:
		fast_put(container->sram_ptr);
		return 0;
#endif
	default:
		comp_err(mod->dev, "Unknown resource type: %d", container->type);
	}
	return -EINVAL;
}

/**
 * Frees the memory block removes it from module's book keeping.
 * @param mod	Pointer to module this memory block was allocated for.
 * @param ptr	Pointer to the memory block.
 */
int mod_free(struct processing_module *mod, const void *ptr)
{
	struct module_resources *res = &mod->priv.resources;
	struct module_resource *container;
	struct list_item *res_list;

	MEM_API_CHECK_THREAD(res);
	if (!ptr)
		return 0;

	/* Find which container keeps this memory */
	list_for_item(res_list, &res->res_list) {
		container = container_of(res_list, struct module_resource, list);
		if (container->ptr == ptr) {
			int ret = free_contents(mod, container);

			list_item_del(&container->list);
			container_put(mod, container);
			return ret;
		}
	}

	comp_err(mod->dev, "error: could not find memory pointed by %p", ptr);

	return -EINVAL;
}
EXPORT_SYMBOL(mod_free);

#if CONFIG_COMP_BLOB
void mod_data_blob_handler_free(struct processing_module *mod, struct comp_data_blob_handler *dbh)
{
	mod_free(mod, (void *)dbh);
}
EXPORT_SYMBOL(mod_data_blob_handler_free);
#endif

#if CONFIG_FAST_GET
void mod_fast_put(struct processing_module *mod, const void *sram_ptr)
{
	mod_free(mod, sram_ptr);
}
EXPORT_SYMBOL(mod_fast_put);
#endif

int module_prepare(struct processing_module *mod,
		   struct sof_source **sources, int num_of_sources,
		   struct sof_sink **sinks, int num_of_sinks)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	const struct module_interface *const ops = dev->drv->adapter_ops;

	comp_dbg(dev, "entry");

#if CONFIG_IPC_MAJOR_3
	if (mod->priv.state == MODULE_IDLE)
		return 0;
	if (mod->priv.state < MODULE_INITIALIZED)
		return -EPERM;
#endif
	if (ops->prepare) {
		int ret = ops->prepare(mod, sources, num_of_sources, sinks, num_of_sinks);

		if (ret) {
			comp_err(dev, "error %d: module specific prepare failed", ret);
			return ret;
		}
	}

	/* After prepare is done we no longer need runtime configuration
	 * as it has been applied during the procedure - it is safe to
	 * free it.
	 */
	rfree(md->cfg.data);

	md->cfg.avail = false;
	md->cfg.data = NULL;

#if CONFIG_IPC_MAJOR_3
	md->state = MODULE_IDLE;
#endif
	comp_dbg(dev, "done");

	return 0;
}

int module_process_legacy(struct processing_module *mod,
			  struct input_stream_buffer *input_buffers, int num_input_buffers,
			  struct output_stream_buffer *output_buffers,
			  int num_output_buffers)
{
	struct comp_dev *dev = mod->dev;
	const struct module_interface *const ops = dev->drv->adapter_ops;
	int ret;

	comp_dbg(dev, "entry");

#if CONFIG_IPC_MAJOR_3
	struct module_data *md = &mod->priv;

	if (md->state != MODULE_IDLE) {
		comp_err(dev, "wrong state %d", md->state);
		return -EPERM;
	}

	/* set state to processing */
	md->state = MODULE_PROCESSING;
#endif
	if (IS_PROCESSING_MODE_AUDIO_STREAM(mod))
		ret = ops->process_audio_stream(mod, input_buffers, num_input_buffers,
							     output_buffers, num_output_buffers);
	else if (IS_PROCESSING_MODE_RAW_DATA(mod))
		ret = ops->process_raw_data(mod, input_buffers, num_input_buffers,
							 output_buffers, num_output_buffers);
	else
		ret = -EOPNOTSUPP;

	if (ret && ret != -ENOSPC && ret != -ENODATA) {
		comp_err(dev, "error %d", ret);
		return ret;
	}

	comp_dbg(dev, "done");

#if CONFIG_IPC_MAJOR_3
	/* reset state to idle */
	md->state = MODULE_IDLE;
#endif
	return 0;
}

int module_process_sink_src(struct processing_module *mod,
			    struct sof_source **sources, int num_of_sources,
			    struct sof_sink **sinks, int num_of_sinks)

{
	struct comp_dev *dev = mod->dev;
	const struct module_interface *const ops = dev->drv->adapter_ops;
	int ret;

	comp_dbg(dev, "entry");

#if CONFIG_IPC_MAJOR_3
	struct module_data *md = &mod->priv;
	if (md->state != MODULE_IDLE) {
		comp_err(dev, "wrong state %d", md->state);
		return -EPERM;
	}

	/* set state to processing */
	md->state = MODULE_PROCESSING;
#endif
	assert(ops->process);
	ret = ops->process(mod, sources, num_of_sources, sinks, num_of_sinks);

	if (ret && ret != -ENOSPC && ret != -ENODATA) {
		comp_err(dev, "error %d", ret);
		return ret;
	}

	comp_dbg(dev, "done");

#if CONFIG_IPC_MAJOR_3
	/* reset state to idle */
	md->state = MODULE_IDLE;
#endif
	return 0;
}

int module_reset(struct processing_module *mod)
{
	int ret;
	const struct module_interface *const ops = mod->dev->drv->adapter_ops;
	struct module_data *md = &mod->priv;

#if CONFIG_IPC_MAJOR_3
	/* if the module was never prepared, no need to reset */
	if (md->state < MODULE_IDLE)
		return 0;
#endif
	/* cancel task if DP task*/
	if (mod->dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP && mod->dev->task)
		schedule_task_cancel(mod->dev->task);
	if (ops->reset) {
		ret = ops->reset(mod);
		if (ret) {
			if (ret != PPL_STATUS_PATH_STOP)
				comp_err(mod->dev,
					 "error %d: module specific reset() failed", ret);
			return ret;
		}
	}

	md->cfg.avail = false;
	md->cfg.size = 0;
	rfree(md->cfg.data);
	md->cfg.data = NULL;

#if CONFIG_IPC_MAJOR_3
	/*
	 * reset the state to allow the module's prepare callback to be invoked again for the
	 * subsequent triggers
	 */
	md->state = MODULE_INITIALIZED;
#endif
	return 0;
}

/**
 * Frees all the resources registered for this module
 * @param mod	Pointer to module that should have its resource freed.
 *
 * This function is called automatically when the module is unloaded.
 */
void mod_free_all(struct processing_module *mod)
{
	struct module_resources *res = &mod->priv.resources;
	struct list_item *list;
	struct list_item *_list;

	MEM_API_CHECK_THREAD(res);
	/* Find which container keeps this memory */
	list_for_item_safe(list, _list, &res->res_list) {
		struct module_resource *container =
			container_of(list, struct module_resource, list);

		free_contents(mod, container);
		list_item_del(&container->list);
	}

	list_for_item_safe(list, _list, &res->cont_chunk_list) {
		struct container_chunk *chunk =
			container_of(list, struct container_chunk, chunk_list);

		list_item_del(&chunk->chunk_list);
		rfree(chunk);
	}
}
EXPORT_SYMBOL(mod_free_all);

int module_free(struct processing_module *mod)
{
	const struct module_interface *const ops = mod->dev->drv->adapter_ops;
	struct module_data *md = &mod->priv;
	int ret = 0;

	if (ops->free) {
		ret = ops->free(mod);
		if (ret)
			comp_warn(mod->dev, "error: %d", ret);
	}

	/* Free all memory shared by module_adapter & module */
	md->cfg.avail = false;
	md->cfg.size = 0;
	rfree(md->cfg.data);
	md->cfg.data = NULL;
	if (md->runtime_params) {
		rfree(md->runtime_params);
		md->runtime_params = NULL;
	}
#if CONFIG_IPC_MAJOR_3
	md->state = MODULE_DISABLED;
#endif
	return ret;
}

/*
 * \brief Set module configuration - Common method to assemble large configuration message
 * \param[in] mod - struct processing_module pointer
 * \param[in] config_id - Configuration ID
 * \param[in] pos - position of the fragment in the large message
 * \param[in] data_offset_size: size of the whole configuration if it is the first fragment or the
 *				 only fragment. Otherwise, it is the offset of the fragment in the
 *				 whole configuration.
 * \param[in] fragment: configuration fragment buffer
 * \param[in] fragment_size: size of @fragment
 * \params[in] response: optional response buffer to fill
 * \params[in] response_size: size of @response
 *
 * \return: 0 upon success or error upon failure
 */
int module_set_configuration(struct processing_module *mod,
			     uint32_t config_id,
			     enum module_cfg_fragment_position pos, size_t data_offset_size,
			     const uint8_t *fragment_in, size_t fragment_size, uint8_t *response,
			     size_t response_size)
{
#if CONFIG_IPC_MAJOR_3
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment_in;
	const uint8_t *fragment = (const uint8_t *)cdata->data[0].data;
#elif CONFIG_IPC_MAJOR_4
	const uint8_t *fragment = fragment_in;
#endif
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	size_t offset = 0;
	uint8_t *dst;
	int ret;

	switch (pos) {
	case MODULE_CFG_FRAGMENT_FIRST:
	case MODULE_CFG_FRAGMENT_SINGLE:
		/*
		 * verify input params & allocate memory for the config blob when the first
		 * fragment arrives
		 */
		md->new_cfg_size = data_offset_size;

		/* Check that there is no previous request in progress */
		if (md->runtime_params) {
			comp_err(dev, "error: busy with previous request");
			return -EBUSY;
		}

		if (!md->new_cfg_size)
			return 0;

		if (md->new_cfg_size > CONFIG_MODULE_MAX_BLOB_SIZE) {
			comp_err(dev, "error: blob size is too big cfg size %zu, allowed %d",
				 md->new_cfg_size, CONFIG_MODULE_MAX_BLOB_SIZE);
			return -EINVAL;
		}

		/* Allocate buffer for new params */
		md->runtime_params = rballoc(SOF_MEM_FLAG_USER, md->new_cfg_size);
		if (!md->runtime_params) {
			comp_err(dev, "space allocation for new params failed");
			return -ENOMEM;
		}

		memset(md->runtime_params, 0, md->new_cfg_size);
		break;
	default:
		if (!md->runtime_params) {
			comp_err(dev, "error: no memory available for runtime params in consecutive load");
			return -EIO;
		}

		/* set offset for intermediate and last fragments */
		offset = data_offset_size;
		break;
	}

	dst = (uint8_t *)md->runtime_params + offset;

	ret = memcpy_s(dst, md->new_cfg_size - offset, fragment, fragment_size);
	if (ret < 0) {
		comp_err(dev, "error: %d failed to copy fragment", ret);
		return ret;
	}

	/* return as more fragments of config data expected */
	if (pos == MODULE_CFG_FRAGMENT_MIDDLE || pos == MODULE_CFG_FRAGMENT_FIRST)
		return 0;

	/* config fully copied, now load it */
	ret = module_load_config(dev, md->runtime_params, md->new_cfg_size);
	if (ret)
		comp_err(dev, "error %d: config failed", ret);
	else
		comp_dbg(dev, "config load successful");

	md->new_cfg_size = 0;

	if (md->runtime_params)
		rfree(md->runtime_params);
	md->runtime_params = NULL;

	return ret;
}
EXPORT_SYMBOL(module_set_configuration);

int module_bind(struct processing_module *mod, struct bind_info *bind_data)
{
	int ret;
	const struct module_interface *const ops = mod->dev->drv->adapter_ops;

	switch (bind_data->bind_type) {
	case COMP_BIND_TYPE_SINK:
		ret = sink_bind(bind_data->sink, mod);
		break;
	case COMP_BIND_TYPE_SOURCE:
		ret = source_bind(bind_data->source, mod);
		break;
	default:
		ret = -EINVAL;
	}
	if (ret)
		return ret;

	if (ops->bind)
		ret = ops->bind(mod, bind_data);

	return ret;
}

int module_unbind(struct processing_module *mod, struct bind_info *unbind_data)
{
	int ret;
	const struct module_interface *const ops = mod->dev->drv->adapter_ops;

	switch (unbind_data->bind_type) {
	case COMP_BIND_TYPE_SINK:
		ret = sink_unbind(unbind_data->sink);
		break;
	case COMP_BIND_TYPE_SOURCE:
		ret = source_unbind(unbind_data->source);
		break;
	default:
		ret = -EINVAL;
	}
	if (ret)
		return ret;

	if (ops->unbind)
		ret = ops->unbind(mod, unbind_data);

	return ret;
}

void module_update_buffer_position(struct input_stream_buffer *input_buffers,
				   struct output_stream_buffer *output_buffers,
				   uint32_t frames)
{
	struct audio_stream *source = input_buffers->data;
	struct audio_stream *sink = output_buffers->data;

	input_buffers->consumed += audio_stream_frame_bytes(source) * frames;
	output_buffers->size += audio_stream_frame_bytes(sink) * frames;
}
EXPORT_SYMBOL(module_update_buffer_position);
