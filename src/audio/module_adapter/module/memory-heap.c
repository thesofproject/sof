// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>

/*
 * \file memory-heap.c
 * \brief Generic Codec Memory API heap functions
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

struct container_chunk {
	struct list_item chunk_list;
	struct module_resource containers[CONFIG_MODULE_MEMORY_API_CONTAINER_CHUNK_SIZE];
};

void mod_resource_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	/* Init memory list */
	list_init(&md->resources.res_list);
	list_init(&md->resources.free_cont_list);
	list_init(&md->resources.cont_chunk_list);
	md->resources.heap_usage = 0;
	md->resources.heap_high_water_mark = 0;
}

static struct module_resource *container_get(struct processing_module *mod)
{
	struct module_resources *res = &mod->priv.resources;
	struct k_heap *mod_heap = res->heap;
	struct module_resource *container;

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
 * @param mod		Pointer to the module this memory block is allocated for.
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

	MEM_API_CHECK_THREAD(res);

	container = container_get(mod);
	if (!container)
		return NULL;

	if (!size) {
		comp_err(mod->dev, "requested allocation of 0 bytes.");
		container_put(mod, container);
		return NULL;
	}

	/* Allocate buffer memory for module */
	void *ptr = sof_heap_alloc(res->heap, SOF_MEM_FLAG_USER | SOF_MEM_FLAG_LARGE_BUFFER,
				   size, alignment);

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
 * Allocates aligned memory block with flags for module.
 * @param mod		Pointer to the module this memory block is allocated for.
 * @param flags		Allocator flags.
 * @param bytes		Size in bytes.
 * @param alignment	Alignment in bytes.
 * @return Pointer to the allocated memory or NULL if failed.
 *
 * The allocated memory is automatically freed when the module is unloaded.
 */
void *mod_alloc_ext(struct processing_module *mod, uint32_t flags, size_t size, size_t alignment)
{
	struct module_resources *res = &mod->priv.resources;
	struct module_resource *container;

	MEM_API_CHECK_THREAD(res);

	container = container_get(mod);
	if (!container)
		return NULL;

	if (!size) {
		comp_err(mod->dev, "requested allocation of 0 bytes.");
		container_put(mod, container);
		return NULL;
	}

	/* Allocate memory for module */
	void *ptr = sof_heap_alloc(res->heap, flags, size, alignment);

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
EXPORT_SYMBOL(mod_alloc_ext);

/**
 * Creates a blob handler and releases it when the module is unloaded
 * @param mod	Pointer to module this memory block is allocated for.
 * @return Pointer to the created data blob handler
 *
 * Like comp_data_blob_handler_new() but the handler is automatically freed.
 */
#if CONFIG_COMP_BLOB
struct comp_data_blob_handler *mod_data_blob_handler_new(struct processing_module *mod)
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

	ptr = fast_get(res->heap, dram_ptr, size);
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
		sof_heap_free(res->heap, container->ptr);
		res->heap_usage -= container->size;
		return 0;
#if CONFIG_COMP_BLOB
	case MOD_RES_BLOB_HANDLER:
		comp_data_blob_handler_free(container->bhp);
		return 0;
#endif
#if CONFIG_FAST_GET
	case MOD_RES_FAST_GET:
		fast_put(res->heap, container->sram_ptr);
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

/**
 * Frees all the resources registered for this module
 * @param mod	Pointer to module that should have its resource freed.
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
	/* Free all contents found in used containers */
	list_for_item(list, &res->res_list) {
		struct module_resource *container =
			container_of(list, struct module_resource, list);

		free_contents(mod, container);
	}

	/*
	 * We do not need to remove the containers from res_list in
	 * the loop above or go through free_cont_list as all the
	 * containers are anyway freed in the loop below, and the list
	 * heads are reinitialized when mod_resource_init() is called.
	 */
	list_for_item_safe(list, _list, &res->cont_chunk_list) {
		struct container_chunk *chunk =
			container_of(list, struct container_chunk, chunk_list);

		list_item_del(&chunk->chunk_list);
		sof_heap_free(mod_heap, chunk);
	}

	/* Make sure resource lists and accounting are reset */
	mod_resource_init(mod);
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

#if CONFIG_MM_DRV
#define PAGE_SZ CONFIG_MM_DRV_PAGE_SIZE
#else
#include <platform/platform.h>
#define PAGE_SZ HOST_PAGE_SIZE
#endif

static struct k_heap *module_adapter_dp_heap_new(const struct comp_ipc_config *config)
{
	/* src-lite with 8 channels has been seen allocating 14k in one go */
	/* FIXME: the size will be derived from configuration */
	const size_t heap_size = 20 * 1024;

	/* Keep uncached to match the default SOF heap! */
	uint8_t *mod_heap_mem = rballoc_align(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT,
					      heap_size, PAGE_SZ);

	if (!mod_heap_mem)
		return NULL;

	struct k_heap *mod_heap = (struct k_heap *)mod_heap_mem;
	const size_t heap_prefix_size = ALIGN_UP(sizeof(*mod_heap), 8);
	void *mod_heap_buf = mod_heap_mem + heap_prefix_size;

	k_heap_init(mod_heap, mod_heap_buf, heap_size - heap_prefix_size);

#ifdef __ZEPHYR__
	mod_heap->heap.init_mem = mod_heap_buf;
	mod_heap->heap.init_bytes = heap_size - heap_prefix_size;
#endif
	return mod_heap;
}

struct processing_module *module_adapter_mem_alloc(const struct comp_driver *drv,
						   const struct comp_ipc_config *config)
{
	struct k_heap *mod_heap;
	/*
	 * For DP shared modules the struct processing_module object must be
	 * accessible from all cores. Unfortunately at this point there's no
	 * information of components the module will be bound to. So we need to
	 * allocate shared memory for each DP module.
	 * To be removed when pipeline 2.0 is ready.
	 */
	uint32_t flags = config->proc_domain == COMP_PROCESSING_DOMAIN_DP ?
		SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT : SOF_MEM_FLAG_USER;

	if (config->proc_domain == COMP_PROCESSING_DOMAIN_DP && IS_ENABLED(CONFIG_USERSPACE) &&
	    !IS_ENABLED(CONFIG_SOF_USERSPACE_USE_DRIVER_HEAP)) {
		mod_heap = module_adapter_dp_heap_new(config);
		if (!mod_heap) {
			comp_cl_err(drv, "Failed to allocate DP module heap");
			return NULL;
		}
	} else {
		mod_heap = drv->user_heap;
	}

	struct processing_module *mod = sof_heap_alloc(mod_heap, flags, sizeof(*mod), 0);

	if (!mod) {
		comp_cl_err(drv, "failed to allocate memory for module");
		goto emod;
	}

	memset(mod, 0, sizeof(*mod));
	mod->priv.resources.heap = mod_heap;

	/*
	 * Would be difficult to optimize the allocation to use cache. Only if
	 * the whole currently active topology is running on the primary core,
	 * then it can be cached. Effectively it can be only cached in
	 * single-core configurations.
	 */
	struct comp_dev *dev = sof_heap_alloc(mod_heap, SOF_MEM_FLAG_COHERENT, sizeof(*dev), 0);

	if (!dev) {
		comp_cl_err(drv, "failed to allocate memory for comp_dev");
		goto err;
	}

	memset(dev, 0, sizeof(*dev));
	comp_init(drv, dev, sizeof(*dev));
	dev->ipc_config = *config;
	mod->dev = dev;
	dev->mod = mod;

	return mod;

err:
	sof_heap_free(mod_heap, mod);
emod:
	if (mod_heap != drv->user_heap)
		rfree(mod_heap);

	return NULL;
}

void module_adapter_mem_free(struct processing_module *mod)
{
	struct k_heap *mod_heap = mod->priv.resources.heap;

#if CONFIG_IPC_MAJOR_4
	sof_heap_free(mod_heap, mod->priv.cfg.input_pins);
#endif
	sof_heap_free(mod_heap, mod->dev);
	sof_heap_free(mod_heap, mod);
}

