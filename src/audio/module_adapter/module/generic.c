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
#include <sof/schedule/dp_schedule.h>
#if CONFIG_IPC_MAJOR_4
#include <ipc4/header.h>
#include <ipc4/module.h>
#include <ipc4/pipeline.h>
#endif

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

	comp_dbg(dev, "module_load_config() start");

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

	comp_dbg(dev, "module_load_config() done");
	return ret;
}

static void mod_resource_init(struct processing_module *mod)
{
	struct module_resources *rsrc = &mod->priv.resources;

	/* Init memory list */
	list_init(&rsrc->mem_list);
	list_init(&rsrc->free_cont_list);
	list_init(&rsrc->cont_chunk_list);
	rsrc->heap_usage = 0;
	rsrc->heap_high_water_mark = 0;
#if CONFIG_MODULE_MEMORY_API_DEBUG && defined(__ZEPHYR__)
	rsrc->rsrc_mngr = k_current_get();
#endif
}

int module_init(struct processing_module *mod)
{
	int ret;
	struct comp_dev *dev = mod->dev;
	const struct module_interface *const interface = dev->drv->adapter_ops;

	comp_dbg(dev, "module_init() start");

#if CONFIG_IPC_MAJOR_3
	if (mod->priv.state == MODULE_INITIALIZED)
		return 0;
	if (mod->priv.state > MODULE_INITIALIZED)
		return -EPERM;
#endif
	if (!interface) {
		comp_err(dev, "module interface not defined for comp id %d",
			 dev_comp_id(dev));
		return -EIO;
	}

	/* check interface, there must be one and only one of processing procedure */
	if (!interface->init ||
	    (!!interface->process + !!interface->process_audio_stream +
	     !!interface->process_raw_data < 1)) {
		comp_err(dev, "comp %d is missing mandatory interfaces",
			 dev_comp_id(dev));
		return -EIO;
	}

	mod_resource_init(mod);

	/* Now we can proceed with module specific initialization */
#if CONFIG_IPC_MAJOR_4
	if (mod->dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP)
		ret = scheduler_dp_thread_ipc(mod, SOF_IPC4_MOD_INIT_INSTANCE, NULL);
	else
#endif
		ret = interface->init(mod);

	if (ret) {
		comp_err(dev, "module_init() error %d: module specific init failed, comp id %d",
			 ret, dev_comp_id(dev));
		return ret;
	}

	comp_dbg(dev, "module_init() done");
#if CONFIG_IPC_MAJOR_3
	mod->priv.state = MODULE_INITIALIZED;
#endif

	return 0;
}

struct container_chunk {
	struct list_item chunk_list;
	struct module_memory containers[CONFIG_MODULE_MEMORY_API_CONTAINER_CHUNK_SIZE];
};

static struct module_memory *container_get(struct processing_module *mod)
{
	struct module_resources *res = &mod->priv.resources;
	struct k_heap *mod_heap = res->heap;
	struct module_memory *container;

	if (list_is_empty(&res->free_cont_list)) {
		struct container_chunk *chunk = sof_heap_alloc(mod_heap, 0, sizeof(*chunk), 0);
		int i;

		if (!chunk) {
			comp_err(mod->dev, "allocating more containers failed");
			return NULL;
		}

		memset(chunk, 0, sizeof(*chunk));

		list_item_append(&chunk->chunk_list, &res->cont_chunk_list);
		for (i = 0; i < ARRAY_SIZE(chunk->containers); i++)
			list_item_append(&chunk->containers[i].mem_list, &res->free_cont_list);
	}

	container = list_first_item(&res->free_cont_list, struct module_memory, mem_list);
	list_item_del(&container->mem_list);
	return container;
}

static void container_put(struct processing_module *mod, struct module_memory *container)
{
	struct module_resources *res = &mod->priv.resources;

	list_item_append(&container->mem_list, &res->free_cont_list);
}

/**
 * Allocates aligned memory block with flags for module.
 * @param mod		Pointer to the module this memory block is allocatd for.
 * @param flags		Allocator flags.
 * @param bytes		Size in bytes.
 * @param alignment	Alignment in bytes.
 * @return Pointer to the allocated memory or NULL if failed.
 *
 * The allocated memory is automatically freed when the module is unloaded.
 */
void *mod_alloc_ext(struct processing_module *mod, uint32_t flags, size_t size, size_t alignment)
{
	struct module_memory *container = container_get(mod);
	struct module_resources *res = &mod->priv.resources;
	struct k_heap *mod_heap = res->heap;
	void *ptr;

	MEM_API_CHECK_THREAD(res);
	if (!container)
		return NULL;

	if (!size) {
		comp_err(mod->dev, "mod_alloc: requested allocation of 0 bytes.");
		container_put(mod, container);
		return NULL;
	}

	/* Allocate memory for module */
	ptr = sof_heap_alloc(mod_heap, flags, size, alignment);
	if (!ptr) {
		comp_err(mod->dev, "mod_alloc: failed to allocate memory for comp %x.",
			 dev_comp_id(mod->dev));
		container_put(mod, container);
		return NULL;
	}
	/* Store reference to allocated memory */
	container->ptr = ptr;
	container->size = size;
	container->free = NULL;
	list_item_prepend(&container->mem_list, &res->mem_list);

	res->heap_usage += size;
	if (res->heap_usage > res->heap_high_water_mark)
		res->heap_high_water_mark = res->heap_usage;

	return ptr;
}
EXPORT_SYMBOL(mod_alloc_ext);

/**
 * Creates a blob handler and releases it when the module is unloaded
 * @param mod	Pointer to module this memory block is allocated for.
 * @return Pointer to the created data blob handler
 *
 * Like comp_data_blob_handler_new() but the handler is automatically freed.
 */
#if CONFIG_COMP_BLOB
static void blob_free_wrapper(struct k_heap *heap, struct comp_data_blob_handler *blob_handler)
{
	ARG_UNUSED(heap);
	comp_data_blob_handler_free(blob_handler);
}

struct comp_data_blob_handler *
mod_data_blob_handler_new(struct processing_module *mod)
{
	struct module_resources *res = &mod->priv.resources;
	struct module_memory *container = container_get(mod);
	struct comp_data_blob_handler *dbh;

	MEM_API_CHECK_THREAD(res);
	if (!container)
		return NULL;

	dbh = comp_data_blob_handler_new_ext(mod->dev, false, NULL, NULL);
	if (!dbh) {
		container_put(mod, container);
		return NULL;
	}

	container->ptr = dbh;
	container->size = 0;
	container->free = (void (*)(struct k_heap *, void *))blob_free_wrapper;
	list_item_prepend(&container->mem_list, &res->mem_list);

	return dbh;
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
	struct module_memory *container = container_get(mod);
	const void *ptr;

	MEM_API_CHECK_THREAD(res);
	if (!container)
		return NULL;

	ptr = fast_get(res->heap, dram_ptr, size);
	if (!ptr) {
		container_put(mod, container);
		return NULL;
	}

	container->ptr = (void *)ptr;
	container->size = 0;
	container->free = (void (*)(struct k_heap *, void *))fast_put;
	list_item_prepend(&container->mem_list, &res->mem_list);

	return ptr;
}
EXPORT_SYMBOL(mod_fast_get);
#endif

/**
 * Frees the memory block removes it from module's book keeping.
 * @param mod	Pointer to module this memory block was allocated for.
 * @param ptr	Pointer to the memory block.
 */
int mod_free(struct processing_module *mod, const void *ptr)
{
	struct module_resources *res = &mod->priv.resources;
	struct k_heap *mod_heap = res->heap;
	struct module_memory *mem;
	struct list_item *mem_list;
	struct list_item *_mem_list;

	MEM_API_CHECK_THREAD(res);
	if (!ptr)
		return 0;

	/* Find which container keeps this memory */
	list_for_item_safe(mem_list, _mem_list, &res->mem_list) {
		mem = container_of(mem_list, struct module_memory, mem_list);
		if (mem->ptr == ptr) {
			if (mem->free) {
				mem->free(mod_heap, mem->ptr);
			} else {
				sof_heap_free(mod_heap, mem->ptr);
				res->heap_usage -= mem->size;
			}
			list_item_del(&mem->mem_list);
			container_put(mod, mem);
			return 0;
		}
	}

	comp_err(mod->dev, "mod_free: error: could not find memory pointed by %p",
		 ptr);

	return -EINVAL;
}
EXPORT_SYMBOL(mod_free);

int module_prepare(struct processing_module *mod,
		   struct sof_source **sources, int num_of_sources,
		   struct sof_sink **sinks, int num_of_sinks)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	const struct module_interface *const ops = dev->drv->adapter_ops;

	comp_dbg(dev, "module_prepare() start");

#if CONFIG_IPC_MAJOR_3
	if (mod->priv.state == MODULE_IDLE)
		return 0;
	if (mod->priv.state < MODULE_INITIALIZED)
		return -EPERM;
#endif
	if (ops->prepare) {
		int ret;

		if (mod->dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP) {
#if CONFIG_IPC_MAJOR_4
			union scheduler_dp_thread_ipc_param param = {
				.pipeline_state = {
					.trigger_cmd = COMP_TRIGGER_PREPARE,
					.state = SOF_IPC4_PIPELINE_STATE_RUNNING,
					.n_sources = num_of_sources,
					.sources = sources,
					.n_sinks = num_of_sinks,
					.sinks = sinks,
				},
			};
			ret = scheduler_dp_thread_ipc(mod, SOF_IPC4_GLB_SET_PIPELINE_STATE, &param);
#else
			ret = 0;
#endif
		} else {
			ret = ops->prepare(mod, sources, num_of_sources, sinks, num_of_sinks);
		}

		if (ret) {
			comp_err(dev, "module_prepare() error %d: module specific prepare failed, comp_id %d",
				 ret, dev_comp_id(dev));
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
	comp_dbg(dev, "module_prepare() done");

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

	comp_dbg(dev, "module_process_legacy() start");

#if CONFIG_IPC_MAJOR_3
	struct module_data *md = &mod->priv;

	if (md->state != MODULE_IDLE) {
		comp_err(dev, "wrong state of comp_id %x, state %d",
			 dev_comp_id(dev), md->state);
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
		comp_err(dev, "module_process() error %d: for comp %d",
			 ret, dev_comp_id(dev));
		return ret;
	}

	comp_dbg(dev, "module_process_legacy() done");

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

	comp_dbg(dev, "module_process sink src() start");

#if CONFIG_IPC_MAJOR_3
	struct module_data *md = &mod->priv;
	if (md->state != MODULE_IDLE) {
		comp_err(dev, "wrong state of comp_id %x, state %d",
			 dev_comp_id(dev), md->state);
		return -EPERM;
	}

	/* set state to processing */
	md->state = MODULE_PROCESSING;
#endif
	assert(ops->process);
	ret = ops->process(mod, sources, num_of_sources, sinks, num_of_sinks);

	if (ret && ret != -ENOSPC && ret != -ENODATA) {
		comp_err(dev, "module_process() error %d: for comp %d",
			 ret, dev_comp_id(dev));
		return ret;
	}

	comp_dbg(dev, "module_process sink src() done");

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

	if (ops->reset) {
		if (mod->dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP) {
#if CONFIG_IPC_MAJOR_4
			union scheduler_dp_thread_ipc_param param = {
				.pipeline_state.trigger_cmd = COMP_TRIGGER_STOP,
			};
			ret = scheduler_dp_thread_ipc(mod, SOF_IPC4_GLB_SET_PIPELINE_STATE, &param);
#else
			ret = 0;
#endif
		} else {
			ret = ops->reset(mod);
		}

		if (ret) {
			if (ret != PPL_STATUS_PATH_STOP)
				comp_err(mod->dev,
					 "module_reset() error %d: module specific reset() failed for comp %d",
					 ret, dev_comp_id(mod->dev));
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
 * Frees all the memory allocated for this module
 * @param mod	Pointer to module this memory block was allocated for.
 *
 * This function is called automatically when the module is unloaded.
 */
void mod_free_all(struct processing_module *mod)
{
	struct module_resources *res = &mod->priv.resources;
	struct k_heap *mod_heap = res->heap;
	struct list_item *list;
	struct list_item *_list;

	MEM_API_CHECK_THREAD(res);
	/* Find which container keeps this memory */
	list_for_item_safe(list, _list, &res->mem_list) {
		struct module_memory *mem = container_of(list, struct module_memory, mem_list);

		if (mem->free)
			mem->free(res->heap, mem->ptr);
		else
			sof_heap_free(mod_heap, mem->ptr);
		list_item_del(&mem->mem_list);
	}

	list_for_item_safe(list, _list, &res->cont_chunk_list) {
		struct container_chunk *chunk =
			container_of(list, struct container_chunk, chunk_list);

		list_item_del(&chunk->chunk_list);
		sof_heap_free(mod_heap, chunk);
	}
}
EXPORT_SYMBOL(mod_free_all);

int module_free(struct processing_module *mod)
{
	const struct module_interface *const ops = mod->dev->drv->adapter_ops;
	struct module_data *md = &mod->priv;
	int ret = 0;

	if (ops->free && mod->dev->ipc_config.proc_domain != COMP_PROCESSING_DOMAIN_DP) {
		ret = ops->free(mod);
		if (ret)
			comp_warn(mod->dev, "error: %d for %d",
				  ret, dev_comp_id(mod->dev));
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
		comp_err(dev, "error: %d failed to copy fragment",
			 ret);
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

	if (ops->bind) {
		if (mod->dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP) {
#if CONFIG_IPC_MAJOR_4
			union scheduler_dp_thread_ipc_param param = {
				.bind_data = bind_data,
			};
			ret = scheduler_dp_thread_ipc(mod, SOF_IPC4_MOD_BIND, &param);
#endif
		} else {
			ret = ops->bind(mod, bind_data);
		}
	}

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
