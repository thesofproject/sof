// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.
//
// Author: Jaroslaw Stelter <jaroslaw.stelter@intel.com>
//	   Adrian Warecki <adrian.warecki@intel.com>

/**
 * \file
 * \brief Zephyr userspace helper functions
 * \authors Jaroslaw Stelter <jaroslaw.stelter@intel.com>
 * \authors Adrian Warecki <adrian.warecki@intel.com>
 */

#include <stdint.h>

#include <rtos/alloc.h>
#include <rtos/userspace_helper.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/module_adapter/library/userspace_proxy.h>

#define MODULE_DRIVER_HEAP_CACHED		CONFIG_SOF_ZEPHYR_HEAP_CACHED

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/app_memory/app_memdomain.h>

#if CONFIG_USERSPACE

K_APPMEM_PARTITION_DEFINE(common_partition);

struct sys_heap *module_driver_heap_init(void)
{
	struct sys_heap *mod_drv_heap = rballoc(SOF_MEM_FLAG_USER, sizeof(struct sys_heap));

	if (!mod_drv_heap)
		return NULL;

	void *mem = rballoc_align(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT, DRV_HEAP_SIZE,
				  CONFIG_MM_DRV_PAGE_SIZE);
	if (!mem) {
		rfree(mod_drv_heap);
		return NULL;
	}

	sys_heap_init(mod_drv_heap, mem, DRV_HEAP_SIZE);
	mod_drv_heap->init_mem = mem;
	mod_drv_heap->init_bytes = DRV_HEAP_SIZE;

	return mod_drv_heap;
}

void *module_driver_heap_aligned_alloc(struct sys_heap *mod_drv_heap, uint32_t flags, size_t bytes,
				       uint32_t align)
{
#ifdef MODULE_DRIVER_HEAP_CACHED
	const bool cached = (flags & SOF_MEM_FLAG_COHERENT) == 0;
#endif /* MODULE_DRIVER_HEAP_CACHED */

	if (mod_drv_heap) {
#ifdef MODULE_DRIVER_HEAP_CACHED
		if (cached) {
			/*
			 * Zephyr sys_heap stores metadata at start of each
			 * heap allocation. To ensure no allocated cached buffer
			 * overlaps the same cacheline with the metadata chunk,
			 * align both allocation start and size of allocation
			 * to cacheline. As cached and non-cached allocations are
			 * mixed, same rules need to be followed for both type of
			 * allocations.
			 */
			align = MAX(PLATFORM_DCACHE_ALIGN, align);
			bytes = ALIGN_UP(bytes, align);
		}
#endif /* MODULE_DRIVER_HEAP_CACHED */
		void *mem = sys_heap_aligned_alloc(mod_drv_heap, align, bytes);
#ifdef MODULE_DRIVER_HEAP_CACHED
		if (cached)
			return sys_cache_cached_ptr_get(mem);
#endif	/* MODULE_DRIVER_HEAP_CACHED */
		return mem;
	} else {
		return rballoc_align(flags, bytes, align);
	}
}

void *module_driver_heap_rmalloc(struct sys_heap *mod_drv_heap, uint32_t flags, size_t bytes)
{
	if (mod_drv_heap)
		return module_driver_heap_aligned_alloc(mod_drv_heap, flags, bytes, 0);
	else
		return rmalloc(flags, bytes);
}

void *module_driver_heap_rzalloc(struct sys_heap *mod_drv_heap, uint32_t flags, size_t bytes)
{
	void *ptr;

	ptr = module_driver_heap_rmalloc(mod_drv_heap, flags, bytes);
	if (ptr)
		memset(ptr, 0, bytes);

	return ptr;
}

void module_driver_heap_free(struct sys_heap *mod_drv_heap, void *mem)
{
	if (mod_drv_heap) {
#ifdef MODULE_DRIVER_HEAP_CACHED
		if (is_cached(mem)) {
			void *mem_uncached = sys_cache_uncached_ptr_get(
				(__sparse_force void __sparse_cache *)mem);

			sys_cache_data_invd_range(mem,
						  sys_heap_usable_size(mod_drv_heap, mem_uncached));

			mem = mem_uncached;
		}
#endif
		sys_heap_free(mod_drv_heap, mem);
	} else {
		rfree(mem);
	}
}

void module_driver_heap_remove(struct sys_heap *mod_drv_heap)
{
	if (mod_drv_heap) {
		rfree(mod_drv_heap->init_mem);
		rfree(mod_drv_heap);
	}
}

void *user_stack_allocate(size_t stack_size, uint32_t options)
{
	return (__sparse_force void __sparse_cache *)
		k_thread_stack_alloc(stack_size, options & K_USER);
}

int user_stack_free(void *p_stack)
{
	if (!p_stack)
		return 0;
	return k_thread_stack_free((__sparse_force void *)p_stack);
}

int user_memory_init_shared(k_tid_t thread_id, struct processing_module *mod)
{
	struct k_mem_domain *comp_dom = mod->user_ctx->comp_dom;
	int ret;

	ret = k_mem_domain_add_partition(comp_dom, &common_partition);
	if (ret < 0)
		return ret;

	return k_mem_domain_add_thread(comp_dom, thread_id);
}

#else /* CONFIG_USERSPACE */

void *user_stack_allocate(size_t stack_size, uint32_t options)
{
	/* allocate stack - must be aligned and cached so a separate alloc */
	stack_size = K_KERNEL_STACK_LEN(stack_size);
	void *p_stack = (__sparse_force void __sparse_cache *)
		rballoc_align(SOF_MEM_FLAG_USER, stack_size, Z_KERNEL_STACK_OBJ_ALIGN);

	return p_stack;
}

int user_stack_free(void *p_stack)
{
	rfree((__sparse_force void *)p_stack);
	return 0;
}

void *module_driver_heap_rmalloc(struct sys_heap *mod_drv_heap, uint32_t flags, size_t bytes)
{
	return rmalloc(flags, bytes);
}

void *module_driver_heap_aligned_alloc(struct sys_heap *mod_drv_heap, uint32_t flags, size_t bytes,
				       uint32_t align)
{
	return rballoc_align(flags, bytes, align);
}

void *module_driver_heap_rzalloc(struct sys_heap *mod_drv_heap, uint32_t flags, size_t bytes)
{
	return rzalloc(flags, bytes);
}

void module_driver_heap_free(struct sys_heap *mod_drv_heap, void *mem)
{
	rfree(mem);
}

void module_driver_heap_remove(struct sys_heap *mod_drv_heap)
{ }

#endif /* CONFIG_USERSPACE */
