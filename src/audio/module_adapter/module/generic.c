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
#include <rtos/mutex.h>
#include <rtos/userspace_data.h>
#include <sof/compiler_attributes.h>
#include <sof/objpool.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/data_blob.h>
#include <sof/debug/telemetry/performance_monitor.h>
#include <sof/lib/cold.h>
#include <sof/lib/fast-get.h>
#include <sof/lib/vregion.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/schedule/schedule.h>
#if CONFIG_IPC_MAJOR_4
#include <ipc4/header.h>
#include <ipc4/module.h>
#include <ipc4/pipeline.h>
#endif

LOG_MODULE_DECLARE(module_adapter, CONFIG_SOF_LOG_LEVEL);

/*
 * A single lock shared by the module resource pools that are used in
 * supervisor mode, by kernel code and the mod_dp_*() system calls, so it does
 * not need to live in the per-module, user-writable struct - a static kernel
 * object serialises all pools. Contention is negligible because resource
 * bookkeeping only happens at module setup/teardown, not on the processing path.
 */
static K_MUTEX_DEFINE(mod_res_klock);

#if CONFIG_SOF_USERSPACE_LL
/*
 * The pools of LL modules are only used in user mode, by the LL and IPC user
 * threads, see mod_res_user(), and have their own lock in LL memory.
 */
static APP_SYSUSER_BSS SYS_MUTEX_DEFINE(mod_res_ulock);
#endif

static void mod_res_lock(void)
{
#if CONFIG_SOF_USERSPACE_LL
	if (k_is_user_context()) {
		sys_mutex_lock(&mod_res_ulock, K_FOREVER);
		return;
	}
#endif
	k_mutex_lock(&mod_res_klock, K_FOREVER);
}

static void mod_res_unlock(void)
{
#if CONFIG_SOF_USERSPACE_LL
	if (k_is_user_context()) {
		sys_mutex_unlock(&mod_res_ulock);
		return;
	}
#endif
	k_mutex_unlock(&mod_res_klock);
}

/*
 * Configuration buffers of DP modules are in the module heap: DP modules
 * set their configuration in the DP thread, which may not use the LL user
 * heap. Until the pipeline is complete, the vregion of a DP module only has
 * its lifetime allocator, which does not reclaim freed memory, so DP modules
 * keep their configuration buffers and reuse them for any request that
 * fits. Other modules use the LL user heap, which reclaims freed memory at
 * any time.
 */
static bool module_cfg_in_mod_heap(const struct processing_module *mod)
{
	return mod->dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP;
}

static void *module_cfg_alloc(struct processing_module *mod, size_t size)
{
	if (module_cfg_in_mod_heap(mod))
		return mod_alloc_ext(mod, SOF_MEM_FLAG_USER | SOF_MEM_FLAG_LARGE_BUFFER, size, 0);

	return sof_heap_alloc(sof_sys_user_heap_get(),
			      SOF_MEM_FLAG_USER | SOF_MEM_FLAG_LARGE_BUFFER, size, 0);
}

static void module_cfg_free(struct processing_module *mod, void *ptr)
{
	if (!ptr)
		return;

	if (module_cfg_in_mod_heap(mod))
		mod_free(mod, ptr);
	else
		sof_heap_free(sof_sys_user_heap_get(), ptr);
}

/*
 * Get a buffer for @p size bytes in @p buf, whose allocated size is in
 * @p buf_size. DP modules reuse a buffer that is big enough, other modules
 * one of the same size.
 */
static void *module_cfg_buf_get(struct processing_module *mod, void **buf,
				size_t *buf_size, size_t size)
{
	bool fits = module_cfg_in_mod_heap(mod) ? *buf_size >= size : *buf_size == size;

	if (!*buf || !fits) {
		module_cfg_free(mod, *buf);
		*buf = module_cfg_alloc(mod, size);
		*buf_size = *buf ? size : 0;
	}

	return *buf;
}

static void module_cfg_buf_free(struct processing_module *mod, void **buf, size_t *buf_size)
{
	module_cfg_free(mod, *buf);
	*buf = NULL;
	*buf_size = 0;
}

/* Get a staging buffer for a configuration of @p size bytes */
static void *module_cfg_staging_get(struct processing_module *mod, size_t size)
{
	if (!module_cfg_in_mod_heap(mod))
		return module_cfg_alloc(mod, size);

	return module_cfg_buf_get(mod, &mod->cfg_staging, &mod->cfg_staging_size, size);
}

/* Release a staging buffer after a request, DP modules keep it */
static void module_cfg_staging_put(struct processing_module *mod, void *buf)
{
	if (!module_cfg_in_mod_heap(mod))
		module_cfg_free(mod, buf);
}

__cold int module_load_config(struct comp_dev *dev, const void *cfg, size_t size)
{
	int ret;
	struct module_config *dst;
	/* loadable module must use module adapter */
	struct processing_module *mod = comp_mod(dev);
	struct module_data *md = &mod->priv;

	assert_can_be_cold();

	comp_dbg(dev, "entry");

	if (!cfg || !size) {
		comp_err(dev, "wrong input params! dev %zx, cfg %zx size %zu",
			 (size_t)dev, (size_t)cfg, size);
		return -EINVAL;
	}

	dst = &md->cfg;

	/* allocate space for the config, unless the previous one can be reused */
	if (!module_cfg_buf_get(mod, &dst->data, &mod->cfg_data_size, size)) {
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

void mod_resource_init(struct processing_module *mod)
{
	struct module_resources *res = &mod->priv.resources;

	/* Init memory list */
	list_init(&res->objpool.list);
	res->objpool.heap = res->alloc->heap;
	res->objpool.vreg = res->alloc->vreg;
	res->heap_usage = 0;
	res->heap_high_water_mark = 0;
}

__cold int module_init(struct processing_module *mod)
{
	int ret;
	struct comp_dev *dev = mod->dev;
	const struct module_interface *const interface = dev->drv->adapter_ops;

	assert_can_be_cold();

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

	/* check interface, there must be a processing procedure */
	if (!interface->init || !interface->process) {
		comp_err(dev, "comp is missing mandatory interfaces");
		return -EIO;
	}

	/* Now we can proceed with module specific initialization */
#if CONFIG_SOF_USERSPACE_APPLICATION
	if (mod->dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP) {
		union scheduler_dp_thread_ipc_param param = {
			.ext_data = mod->priv.cfg.ext_data,
		};

		ret = scheduler_dp_thread_ipc(mod, SOF_IPC4_MOD_INIT_INSTANCE, &param);
	} else
#endif
		ret = interface->init(mod);

	if (ret) {
		comp_err(dev, "error %d: module specific init failed", ret);
		mod_free_all(mod);
		return ret;
	}

	comp_dbg(dev, "done");
#if CONFIG_IPC_MAJOR_3
	mod->priv.state = MODULE_INITIALIZED;
#endif

	return 0;
}

static struct module_resource *container_get(struct processing_module *mod)
{
	return objpool_alloc(&mod->priv.resources.objpool, sizeof(struct module_resource), 0);
}

static void container_put(struct processing_module *mod, struct module_resource *container)
{
	objpool_free(&mod->priv.resources.objpool, container);
}

#if CONFIG_USERSPACE
void mod_heap_info(struct processing_module *mod, size_t *size, uintptr_t *start)
{
	struct module_resources *res = &mod->priv.resources;

	if (res->alloc->vreg) {
		vregion_mem_info(res->alloc->vreg, size, start);
	} else if (res->alloc->heap) {
		if (size)
			*size = res->alloc->heap->heap.init_bytes;

		if (start)
			*start = (uintptr_t)res->alloc->heap->heap.init_mem;
	}
}
#endif

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
void *z_impl_mod_balloc_align(struct processing_module *mod, size_t size, size_t alignment)
{
	struct module_resources *res = &mod->priv.resources;
	struct module_resource *container;

	mod_res_lock();

	container = container_get(mod);
	if (!container) {
		mod_res_unlock();
		return NULL;
	}

	if (!size) {
		comp_err(mod->dev, "requested allocation of 0 bytes.");
		container_put(mod, container);
		mod_res_unlock();
		return NULL;
	}

	/* Allocate buffer memory for module, same as mod_alloc_ext() */
	void *ptr = sof_ctx_alloc(res->alloc, SOF_MEM_FLAG_USER | SOF_MEM_FLAG_LARGE_BUFFER,
				  size, alignment);

	if (!ptr) {
		comp_err(mod->dev, "Failed to alloc %zu bytes %zu alignment for comp %#x.",
			 size, alignment, dev_comp_id(mod->dev));
		container_put(mod, container);
		mod_res_unlock();
		return NULL;
	}
	/* Store reference to allocated memory */
	container->ptr = ptr;
	container->size = size;
	container->type = MOD_RES_HEAP;

	res->heap_usage += size;
	if (res->heap_usage > res->heap_high_water_mark)
		res->heap_high_water_mark = res->heap_usage;

	mod_res_unlock();
	perf_data_update_module_resources(mod->dev);
	return ptr;
}
EXPORT_SYMBOL(z_impl_mod_balloc_align);

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
void *z_impl_mod_alloc_ext(struct processing_module *mod, uint32_t flags, size_t size,
			   size_t alignment)
{
	struct module_resources *res = &mod->priv.resources;
	struct module_resource *container;

	if (!size) {
		comp_err(mod->dev, "requested allocation of 0 bytes.");
		return NULL;
	}

	mod_res_lock();

	container = container_get(mod);
	if (!container) {
		mod_res_unlock();
		return NULL;
	}

	/* Allocate memory for module */
	void *ptr = sof_ctx_alloc(res->alloc, flags, size, alignment);

	if (!ptr) {
		comp_err(mod->dev, "Failed to alloc %zu bytes %zu alignment for comp %#x.",
			 size, alignment, dev_comp_id(mod->dev));
		container_put(mod, container);
		mod_res_unlock();
		return NULL;
	}
	/* Store reference to allocated memory */
	container->ptr = ptr;
	container->size = size;
	container->type = MOD_RES_HEAP;

	res->heap_usage += size;
	if (res->heap_usage > res->heap_high_water_mark)
		res->heap_high_water_mark = res->heap_usage;

	mod_res_unlock();
	perf_data_update_module_resources(mod->dev);
	return ptr;
}
EXPORT_SYMBOL(z_impl_mod_alloc_ext);

/**
 * Creates a blob handler and releases it when the module is unloaded
 * @param mod	Pointer to module this memory block is allocated for.
 * @return Pointer to the created data blob handler
 *
 * Like comp_data_blob_handler_new() but the handler is automatically freed.
 */
#if CONFIG_COMP_BLOB
static bool mod_data_blob_find(void *data, void *arg)
{
	struct comp_data_blob_handler **bhp = arg;
	struct module_resource *container = data;

	if (container->type != MOD_RES_BLOB_HANDLER)
		return false;

	*bhp = container->bhp;
	return true;
}

struct comp_data_blob_handler *z_impl_mod_data_blob_handler_new(struct processing_module *mod)
{
	struct module_resources *res = &mod->priv.resources;
	struct comp_data_blob_handler *bhp;
	struct module_resource *container;

	mod_res_lock();
	int ret = objpool_iterate(&res->objpool, mod_data_blob_find, &bhp);

	if (!ret) {
		mod_res_unlock();
		/* data blob already exists */
		comp_warn(mod->dev, "Returning existing data blob");
		return bhp;
	}

	container = container_get(mod);
	if (!container) {
		mod_res_unlock();
		return NULL;
	}

	bhp = comp_data_blob_handler_new_ext(mod->dev, false, NULL, NULL);
	if (!bhp) {
		container_put(mod, container);
		mod_res_unlock();
		return NULL;
	}

	container->bhp = bhp;
	container->size = 0;
	container->type = MOD_RES_BLOB_HANDLER;

	mod_res_unlock();
	return bhp;
}
EXPORT_SYMBOL(z_impl_mod_data_blob_handler_new);

int z_impl_mod_data_blob_set(struct processing_module *mod,
			     enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			     const uint8_t *fragment_in, size_t fragment_size)
{
	struct module_resources *res = &mod->priv.resources;
	struct comp_data_blob_handler *bhp = NULL;

	mod_res_lock();
	int ret = objpool_iterate(&res->objpool, mod_data_blob_find, &bhp);
	mod_res_unlock();

	if (ret < 0)
		return ret;

	if (!bhp)
		return -EFAULT;

	return comp_data_blob_set(bhp, pos, data_offset_size, fragment_in, fragment_size);
}
#endif

/**
 * Make a module associated shared SRAM copy of DRAM read-only data.
 * @param mod	Pointer to module this copy is allocated for.
 * @return Pointer to the SRAM copy.
 *
 * Like fast_get() but the handler is automatically freed.
 */
#if CONFIG_FAST_GET
const void *z_impl_mod_fast_get(struct processing_module *mod, const void * const dram_ptr,
				size_t size)
{
	struct module_resources *res = &mod->priv.resources;
	struct module_resource *container;
	const void *ptr;

	mod_res_lock();

	container = container_get(mod);
	if (!container) {
		mod_res_unlock();
		return NULL;
	}

#if CONFIG_SOF_USERSPACE_LL
	/* only LL modules use their resources in user mode, see mod_res_user() */
	if (k_is_user_context())
		ptr = fast_get_ll(dram_ptr, size);
	else
#endif
		ptr = fast_get(res->alloc, dram_ptr, size);
	if (!ptr) {
		container_put(mod, container);
		mod_res_unlock();
		return NULL;
	}

	container->sram_ptr = ptr;
	container->size = 0;
	container->type = MOD_RES_FAST_GET;

	mod_res_unlock();
	return ptr;
}
EXPORT_SYMBOL(z_impl_mod_fast_get);
#endif

static int free_contents(struct processing_module *mod, struct module_resource *container)
{
	struct module_resources *res = &mod->priv.resources;
#if CONFIG_FAST_GET
	struct k_mem_domain *mdom;
#endif

	switch (container->type) {
	case MOD_RES_HEAP:
		sof_ctx_free(res->alloc, container->ptr);
		res->heap_usage -= container->size;
		return 0;
#if CONFIG_COMP_BLOB
	case MOD_RES_BLOB_HANDLER:
		comp_data_blob_handler_free(container->bhp);
		return 0;
#endif
#if CONFIG_FAST_GET
	case MOD_RES_FAST_GET:
#if CONFIG_SOF_USERSPACE_LL
		if (k_is_user_context()) {
			fast_put_ll(container->sram_ptr);
			return 0;
		}
#endif
#if CONFIG_USERSPACE
		mdom = mod->mdom;
#else
		mdom = NULL;
#endif
		fast_put(res->alloc, mdom, container->sram_ptr);
		return 0;
#endif
	default:
		comp_err(mod->dev, "Unknown resource type: %d", container->type);
	}
	return -EINVAL;
}

struct mod_res_cb_arg {
	struct processing_module *mod;
	const void *ptr;
};

static bool mod_res_free(void *data, void *arg)
{
	struct mod_res_cb_arg *cb_arg = arg;
	struct module_resource *container = data;

	if (cb_arg->ptr && container->ptr != cb_arg->ptr)
		return false;

	int ret = free_contents(cb_arg->mod, container);

	if (ret < 0)
		comp_err(cb_arg->mod->dev, "Cannot free allocation %p", cb_arg->ptr);

	container_put(cb_arg->mod, container);

	/* cb_arg->ptr == NULL means, that we're freeing all. Continue iterating */
	return !!cb_arg->ptr;
}

/**
 * Frees the memory block removes it from module's book keeping.
 * @param mod	Pointer to module this memory block was allocated for.
 * @param ptr	Pointer to the memory block.
 */
int z_impl_mod_free(struct processing_module *mod, const void *ptr)
{
	struct module_resources *res = &mod->priv.resources;

	if (!ptr)
		return 0;

	/* Find which container holds this memory */
	struct mod_res_cb_arg cb_arg = {mod, ptr};

	mod_res_lock();
	int ret = objpool_iterate(&res->objpool, mod_res_free, &cb_arg);

	mod_res_unlock();

	if (ret < 0)
		comp_err(mod->dev, "error: could not find memory pointed by %p", ptr);
	else
		perf_data_update_module_resources(mod->dev);

	return ret;
}
EXPORT_SYMBOL(z_impl_mod_free);

#if defined(__ZEPHYR__) && defined(CONFIG_SOF_FULL_ZEPHYR_APPLICATION)
/*
 * With CONFIG_SOF_USERSPACE_LL, the LL and IPC user threads use the resources
 * of LL modules in user mode: the module memory, its resource pool and the
 * LL user heap are all in LL memory, and the kernel must not run the heap
 * allocator on metadata that user threads can write. DP module resources are
 * used through the mod_dp_*() system calls, as DP threads cannot access the
 * kernel structures behind their vregions. In kernel mode the system call
 * wrappers call the implementation directly.
 *
 * The processing domain is user-writable, so a user thread can only pick the
 * wrong path for itself: the system calls reject LL modules.
 */
static bool mod_res_user(struct processing_module *mod)
{
#if CONFIG_SOF_USERSPACE_LL
	return k_is_user_context() &&
	       mod->dev->ipc_config.proc_domain != COMP_PROCESSING_DOMAIN_DP;
#else
	return false;
#endif
}

void *mod_balloc_align(struct processing_module *mod, size_t size, size_t alignment)
{
	if (mod_res_user(mod))
		return z_impl_mod_balloc_align(mod, size, alignment);

	return mod_dp_balloc_align(mod, size, alignment);
}
EXPORT_SYMBOL(mod_balloc_align);

void *mod_alloc_ext(struct processing_module *mod, uint32_t flags, size_t size,
		    size_t alignment)
{
	if (mod_res_user(mod))
		return z_impl_mod_alloc_ext(mod, flags, size, alignment);

	return mod_dp_alloc_ext(mod, flags, size, alignment);
}
EXPORT_SYMBOL(mod_alloc_ext);

int mod_free(struct processing_module *mod, const void *ptr)
{
	if (mod_res_user(mod))
		return z_impl_mod_free(mod, ptr);

	return mod_dp_free(mod, ptr);
}
EXPORT_SYMBOL(mod_free);

void mod_free_all(struct processing_module *mod)
{
	if (mod_res_user(mod))
		z_impl_mod_free_all(mod);
	else
		mod_dp_free_all(mod);
}
EXPORT_SYMBOL(mod_free_all);

#if CONFIG_COMP_BLOB
struct comp_data_blob_handler *mod_data_blob_handler_new(struct processing_module *mod)
{
	if (mod_res_user(mod))
		return z_impl_mod_data_blob_handler_new(mod);

	return mod_dp_data_blob_handler_new(mod);
}
EXPORT_SYMBOL(mod_data_blob_handler_new);

int mod_data_blob_set(struct processing_module *mod,
		      enum module_cfg_fragment_position pos, uint32_t data_offset_size,
		      const uint8_t *fragment, size_t fragment_size)
{
	if (mod_res_user(mod))
		return z_impl_mod_data_blob_set(mod, pos, data_offset_size, fragment,
						fragment_size);

	return mod_dp_data_blob_set(mod, pos, data_offset_size, fragment, fragment_size);
}
EXPORT_SYMBOL(mod_data_blob_set);
#endif

#if CONFIG_FAST_GET
#if CONFIG_SOF_USERSPACE_LL
/*
 * When the fast_get area of the LL memory region is full, LL modules get a
 * private copy in the module memory instead of a shared one. The copy is a
 * heap resource, so mod_fast_put() frees it with mod_free().
 */
static const void *mod_fast_get_copy(struct processing_module *mod,
				     const void * const dram_ptr, size_t size)
{
	void *ptr = z_impl_mod_alloc_ext(mod, SOF_MEM_FLAG_USER, size, 0);

	if (!ptr)
		return NULL;

	memcpy_s(ptr, size, dram_ptr, size);
	dcache_writeback_region((__sparse_force void __sparse_cache *)ptr, size);

	return ptr;
}
#endif

const void *mod_fast_get(struct processing_module *mod, const void * const dram_ptr,
			 size_t size)
{
#if CONFIG_SOF_USERSPACE_LL
	if (mod_res_user(mod)) {
		const void *ptr = z_impl_mod_fast_get(mod, dram_ptr, size);

		return ptr ? ptr : mod_fast_get_copy(mod, dram_ptr, size);
	}
#endif

	return mod_dp_fast_get(mod, dram_ptr, size);
}
EXPORT_SYMBOL(mod_fast_get);
#endif

void *z_impl_mod_dp_balloc_align(struct processing_module *mod, size_t size, size_t alignment)
{
	return z_impl_mod_balloc_align(mod, size, alignment);
}

void *z_impl_mod_dp_alloc_ext(struct processing_module *mod, uint32_t flags, size_t size,
			      size_t alignment)
{
	return z_impl_mod_alloc_ext(mod, flags, size, alignment);
}

int z_impl_mod_dp_free(struct processing_module *mod, const void *ptr)
{
	return z_impl_mod_free(mod, ptr);
}

void z_impl_mod_dp_free_all(struct processing_module *mod)
{
	z_impl_mod_free_all(mod);
}

#if CONFIG_COMP_BLOB
struct comp_data_blob_handler *z_impl_mod_dp_data_blob_handler_new(struct processing_module *mod)
{
	return z_impl_mod_data_blob_handler_new(mod);
}

int z_impl_mod_dp_data_blob_set(struct processing_module *mod,
				enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				const uint8_t *fragment, size_t fragment_size)
{
	return z_impl_mod_data_blob_set(mod, pos, data_offset_size, fragment, fragment_size);
}
#endif

#if CONFIG_FAST_GET
const void *z_impl_mod_dp_fast_get(struct processing_module *mod, const void * const dram_ptr,
				   size_t size)
{
	return z_impl_mod_fast_get(mod, dram_ptr, size);
}
#endif
#endif /* __ZEPHYR__ && CONFIG_SOF_FULL_ZEPHYR_APPLICATION */

#ifdef CONFIG_USERSPACE
#include <zephyr/internal/syscall_handler.h>

/*
 * Checks shared by the mod_dp_*() validators. Returns a pinned vregion of the
 * module, or NULL, to release with mod_dp_vrfy_done() after the call.
 */
static struct vregion *mod_dp_vrfy(struct processing_module *mod)
{
	struct vregion *vreg = NULL;
	size_t h_size = 0;
	uintptr_t h_start;

	K_OOPS(K_SYSCALL_MEMORY_WRITE(mod, sizeof(*mod)));
#if CONFIG_SOF_USERSPACE_LL
	struct mod_alloc_ctx *alloc;

	/*
	 * LL modules use their resources in user mode. Only DP modules,
	 * whose allocation context uses a vregion, may get here, so the
	 * kernel never allocates from the LL user heap or from a heap that a
	 * user thread made up.
	 */
	K_OOPS(K_SYSCALL_MEMORY_READ(mod->dev, sizeof(*mod->dev)));
	K_OOPS(mod->dev->ipc_config.proc_domain != COMP_PROCESSING_DOMAIN_DP);
	alloc = mod->priv.resources.alloc;
	K_OOPS(K_SYSCALL_MEMORY_READ(alloc, sizeof(*alloc)));
	vreg = alloc->vreg;
	K_OOPS(!vregion_verify_pin(vreg));
#endif
	mod_heap_info(mod, &h_size, &h_start);
	if (h_size && K_SYSCALL_MEMORY_WRITE(h_start, h_size)) {
		if (vreg)
			vregion_unpin(vreg);
		K_OOPS(true);
	}

	return vreg;
}

static void mod_dp_vrfy_done(struct vregion *vreg)
{
	if (vreg)
		vregion_unpin(vreg);
}

#if CONFIG_FAST_GET
const void *z_vrfy_mod_dp_fast_get(struct processing_module *mod, const void * const dram_ptr,
				   size_t size)
{
	struct vregion *vreg;
	const void *ptr;

	K_OOPS(K_SYSCALL_MEMORY_READ(dram_ptr, size));
	vreg = mod_dp_vrfy(mod);
	ptr = z_impl_mod_dp_fast_get(mod, dram_ptr, size);
	mod_dp_vrfy_done(vreg);

	return ptr;
}
#include <zephyr/syscalls/mod_dp_fast_get_mrsh.c>
#endif

void *z_vrfy_mod_dp_alloc_ext(struct processing_module *mod, uint32_t flags, size_t size,
			      size_t alignment)
{
	struct vregion *vreg = mod_dp_vrfy(mod);
	void *ptr = z_impl_mod_dp_alloc_ext(mod, flags, size, alignment);

	mod_dp_vrfy_done(vreg);

	return ptr;
}
#include <zephyr/syscalls/mod_dp_alloc_ext_mrsh.c>

void *z_vrfy_mod_dp_balloc_align(struct processing_module *mod, size_t size, size_t alignment)
{
	struct vregion *vreg = mod_dp_vrfy(mod);
	void *ptr = z_impl_mod_dp_balloc_align(mod, size, alignment);

	mod_dp_vrfy_done(vreg);

	return ptr;
}
#include <zephyr/syscalls/mod_dp_balloc_align_mrsh.c>

int z_vrfy_mod_dp_free(struct processing_module *mod, const void *ptr)
{
	struct vregion *vreg = mod_dp_vrfy(mod);
	int ret = z_impl_mod_dp_free(mod, ptr);

	mod_dp_vrfy_done(vreg);

	return ret;
}
#include <zephyr/syscalls/mod_dp_free_mrsh.c>

void z_vrfy_mod_dp_free_all(struct processing_module *mod)
{
	struct vregion *vreg = mod_dp_vrfy(mod);

	z_impl_mod_dp_free_all(mod);
	mod_dp_vrfy_done(vreg);
}
#include <zephyr/syscalls/mod_dp_free_all_mrsh.c>

#if CONFIG_COMP_BLOB
struct comp_data_blob_handler *z_vrfy_mod_dp_data_blob_handler_new(struct processing_module *mod)
{
	struct vregion *vreg = mod_dp_vrfy(mod);
	struct comp_data_blob_handler *bhp = z_impl_mod_dp_data_blob_handler_new(mod);

	mod_dp_vrfy_done(vreg);

	return bhp;
}
#include <zephyr/syscalls/mod_dp_data_blob_handler_new_mrsh.c>

int z_vrfy_mod_dp_data_blob_set(struct processing_module *mod,
				enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				const uint8_t *fragment_in, size_t fragment_size)
{
	struct vregion *vreg;
	int ret;

	if (fragment_size)
		K_OOPS(K_SYSCALL_MEMORY_READ(fragment_in, fragment_size));
	vreg = mod_dp_vrfy(mod);
	ret = z_impl_mod_dp_data_blob_set(mod, pos, data_offset_size, fragment_in,
					  fragment_size);
	mod_dp_vrfy_done(vreg);

	return ret;
}
#include <zephyr/syscalls/mod_dp_data_blob_set_mrsh.c>
#endif
#endif

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
		int ret;

#if CONFIG_SOF_USERSPACE_APPLICATION
		if (dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP) {
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
		} else
#endif
			ret = ops->prepare(mod, sources, num_of_sources, sinks, num_of_sinks);

		if (ret) {
			comp_err(dev, "error %d: module specific prepare failed", ret);
			return ret;
		}
	}

	/* After prepare is done we no longer need runtime configuration
	 * as it has been applied during the procedure - it is safe to
	 * free it.
	 */
	module_cfg_buf_free(mod, &md->cfg.data, &mod->cfg_data_size);

	md->cfg.avail = false;

#if CONFIG_IPC_MAJOR_3
	md->state = MODULE_IDLE;
#endif
	comp_dbg(dev, "done");

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
	if (mod->dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP && mod->dev->task &&
	    !IS_ENABLED(CONFIG_SOF_USERSPACE_APPLICATION) &&
	    !IS_ENABLED(CONFIG_SOF_USERSPACE_MOD_IPC_BY_DP_THREAD))
		schedule_task_cancel(mod->dev->task);

	if (ops->reset) {
#if CONFIG_SOF_USERSPACE_APPLICATION
		if (mod->dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP) {
			union scheduler_dp_thread_ipc_param param = {
				.pipeline_state.trigger_cmd = COMP_TRIGGER_STOP,
			};
			ret = scheduler_dp_thread_ipc(mod, SOF_IPC4_GLB_SET_PIPELINE_STATE, &param);
		} else
#endif
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
	module_cfg_buf_free(mod, &md->cfg.data, &mod->cfg_data_size);

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
__cold void z_impl_mod_free_all(struct processing_module *mod)
{
	struct module_resources *res = &mod->priv.resources;

	/* Free all contents found in used containers */
	struct mod_res_cb_arg cb_arg = {mod, NULL};

	assert_can_be_cold();

	mod_res_lock();
	objpool_iterate(&res->objpool, mod_res_free, &cb_arg);
	objpool_prune(&res->objpool);
	mod_res_unlock();

	/* Make sure resource lists and accounting are reset */
	mod_resource_init(mod);
	perf_data_update_module_resources(mod->dev);
}

__cold int module_free(struct processing_module *mod)
{
	const struct module_interface *const ops = mod->dev->drv->adapter_ops;
	struct module_data *md = &mod->priv;
	int ret = 0;

	assert_can_be_cold();

	if (ops->free) {
		if (mod->dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_LL ||
		    !IS_ENABLED(CONFIG_SOF_USERSPACE_APPLICATION)) {
			ret = ops->free(mod);
			if (ret)
				comp_warn(mod->dev, "error: %d", ret);
#if CONFIG_SOF_USERSPACE_APPLICATION
		} else {
			/*
			 * Run DP module's .free() method in its thread context.
			 * Unlike with other IPCs we first run module's .free()
			 * in thread context, then cancel the thread, and then
			 * execute final clean up
			 */
			scheduler_dp_thread_ipc(mod, SOF_IPC4_MOD_DELETE_INSTANCE, NULL);
#endif
		}
	}

	/* Free all memory shared by module_adapter & module */
	md->cfg.avail = false;
	md->cfg.size = 0;
	module_cfg_buf_free(mod, &md->cfg.data, &mod->cfg_data_size);
	if (md->runtime_params) {
		module_cfg_staging_put(mod, md->runtime_params);
		md->runtime_params = NULL;
	}
	module_cfg_buf_free(mod, &mod->cfg_staging, &mod->cfg_staging_size);
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
__cold int module_set_configuration(struct processing_module *mod,
				    uint32_t config_id,
				    enum module_cfg_fragment_position pos, size_t data_offset_size,
				    const uint8_t *fragment_in, size_t fragment_size,
				    uint8_t *response, size_t response_size)
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

	assert_can_be_cold();

	switch (pos) {
	case MODULE_CFG_FRAGMENT_FIRST:
	case MODULE_CFG_FRAGMENT_SINGLE:
		/*
		 * verify input params & allocate memory for the config blob when the first
		 * fragment arrives
		 */

		/* Check that there is no previous request in progress */
		if (md->runtime_params) {
			comp_err(dev, "busy with previous request");
			return -EBUSY;
		}

		if (!data_offset_size)
			return 0;

		if (data_offset_size > CONFIG_MODULE_MAX_BLOB_SIZE) {
			comp_err(dev, "blob size is too big cfg size %zu, allowed %d",
				 data_offset_size, CONFIG_MODULE_MAX_BLOB_SIZE);
			return -EINVAL;
		}

		/* Allocate buffer for new params */
		md->runtime_params = module_cfg_staging_get(mod, data_offset_size);
		if (!md->runtime_params) {
			comp_err(dev, "space allocation for new params failed");
			return -ENOMEM;
		}

		md->new_cfg_size = data_offset_size;

		memset(md->runtime_params, 0, md->new_cfg_size);
		break;
	default:
		if (!md->runtime_params) {
			comp_err(dev, "error: no memory available for runtime params in consecutive load");
			return -EIO;
		}

		/* set offset for intermediate and last fragments */
		offset = data_offset_size;
		if (offset > md->new_cfg_size ||
			fragment_size > md->new_cfg_size - offset) {
				comp_err(dev, "fragment (offset %zu, size %zu) exceeds config buffer %zu",
					 offset, fragment_size, md->new_cfg_size);
					return -EINVAL;
		}
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

	module_cfg_staging_put(mod, md->runtime_params);
	md->runtime_params = NULL;

	return ret;
}
EXPORT_SYMBOL(module_set_configuration);

__cold int module_bind(struct processing_module *mod, const struct bind_info *bind_data)
{
	int ret;
	const struct module_interface *const ops = mod->dev->drv->adapter_ops;

	assert_can_be_cold();

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
#if CONFIG_SOF_USERSPACE_APPLICATION
		if (mod->dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP) {
			union scheduler_dp_thread_ipc_param param = {
				.bind_data = bind_data,
			};
			ret = scheduler_dp_thread_ipc(mod, SOF_IPC4_MOD_BIND, &param);
		} else
#endif
			ret = ops->bind(mod, bind_data);
	}

	return ret;
}

__cold int module_unbind(struct processing_module *mod, const struct bind_info *unbind_data)
{
	int ret;
	const struct module_interface *const ops = mod->dev->drv->adapter_ops;

	assert_can_be_cold();

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

	if (ops->unbind) {
#if CONFIG_SOF_USERSPACE_APPLICATION
		if (mod->dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_DP) {
			union scheduler_dp_thread_ipc_param param = {
				.bind_data = unbind_data,
			};
			ret = scheduler_dp_thread_ipc(mod, SOF_IPC4_MOD_UNBIND, &param);
		} else
#endif
			ret = ops->unbind(mod, unbind_data);
	}

	return ret;
}

uint32_t module_get_deadline(struct processing_module *mod)
{
	/* a DP module can write mod: read the count once and bound it */
	size_t num_of_sinks = mod->num_of_sinks;
	uint32_t deadline;

	num_of_sinks = MIN(num_of_sinks, ARRAY_SIZE(mod->sinks));

	/* LL modules have no deadline - it is always "now" */
	if (mod->dev->ipc_config.proc_domain == COMP_PROCESSING_DOMAIN_LL)
		return 0;

	/* startup condition - set deadline to "unknown" */
	if (mod->dp_startup_delay)
		return UINT32_MAX / 2;

	deadline = UINT32_MAX;
	/* calculate the shortest LFT for all sinks */
	for (size_t i = 0; i < num_of_sinks; i++) {
		struct sof_sink *sink = mod->sinks[i];
		uint32_t sink_lft;

		if (!sink)
			continue;

		sink_lft = sink_get_last_feeding_time(sink);
		deadline = MIN(deadline, sink_lft);
	}

	return deadline;
}
