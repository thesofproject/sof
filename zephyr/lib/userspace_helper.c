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
#include <sof/lib/mailbox.h>

#define MODULE_DRIVER_HEAP_CACHED		CONFIG_SOF_ZEPHYR_HEAP_CACHED

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/app_memory/app_memdomain.h>

#if CONFIG_USERSPACE

K_APPMEM_PARTITION_DEFINE(common_partition);

struct k_heap *module_driver_heap_init(void)
{
	struct k_heap *mod_drv_heap = rballoc(SOF_MEM_FLAG_USER, sizeof(*mod_drv_heap));

	if (!mod_drv_heap)
		return NULL;

	void *mem = rballoc_align(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT, USER_MOD_HEAP_SIZE,
				  CONFIG_MM_DRV_PAGE_SIZE);
	if (!mem) {
		rfree(mod_drv_heap);
		return NULL;
	}

	k_heap_init(mod_drv_heap, mem, USER_MOD_HEAP_SIZE);
	mod_drv_heap->heap.init_mem = mem;
	mod_drv_heap->heap.init_bytes = USER_MOD_HEAP_SIZE;

	return mod_drv_heap;
}

void module_driver_heap_remove(struct k_heap *mod_drv_heap)
{
	if (mod_drv_heap) {
		rfree(mod_drv_heap->heap.init_mem);
		rfree(mod_drv_heap);
	}
}

void *user_stack_allocate(size_t stack_size, uint32_t options)
{
	return k_thread_stack_alloc(stack_size, options & K_USER);
}

int user_stack_free(void *p_stack)
{
	if (!p_stack)
		return 0;
	return k_thread_stack_free(p_stack);
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

int user_memory_attach_common_partition(struct k_mem_domain *dom)
{
	return k_mem_domain_add_partition(dom, &common_partition);
}

int user_access_to_mailbox(struct k_mem_domain *domain, k_tid_t thread_id)
{
	struct k_mem_partition mem_partition;
	int ret;

	/*
	 * Start with mailbox_swregs. This is aligned with mailbox.h
	 * implementation with uncached addressed used for register I/O.
	 */
	mem_partition.start =
		(uintptr_t)sys_cache_uncached_ptr_get((void __sparse_cache *)MAILBOX_SW_REG_BASE);

	BUILD_ASSERT(MAILBOX_SW_REG_SIZE == CONFIG_MMU_PAGE_SIZE);
	mem_partition.size = CONFIG_MMU_PAGE_SIZE;
	mem_partition.attr = K_MEM_PARTITION_P_RW_U_RW;

	ret = k_mem_domain_add_partition(domain, &mem_partition);
	if (ret < 0)
		return ret;

#ifndef CONFIG_IPC_MAJOR_4
	/*
	 * Next mailbox_stream (not available in IPC4). Stream access is cached,
	 * so different mapping this time.
	 */
	mem_partition.start =
		(uintptr_t)sys_cache_cached_ptr_get((void *)SRAM_STREAM_BASE);
	BUILD_ASSERT(MAILBOX_STREAM_SIZE == CONFIG_MMU_PAGE_SIZE);
	/* size and attr the same as for mailbox_swregs */

	ret = k_mem_domain_add_partition(domain, &mem_partition);
	if (ret < 0)
		return ret;
#endif

	k_mem_domain_add_thread(domain, thread_id);

	return 0;
}

#else /* CONFIG_USERSPACE */

void *user_stack_allocate(size_t stack_size, uint32_t options)
{
	/* allocate stack - must be aligned and cached so a separate alloc */
	stack_size = K_KERNEL_STACK_LEN(stack_size);
	void *p_stack = rballoc_align(SOF_MEM_FLAG_USER, stack_size, Z_KERNEL_STACK_OBJ_ALIGN);

	return p_stack;
}

int user_stack_free(void *p_stack)
{
	rfree(p_stack);
	return 0;
}

void module_driver_heap_remove(struct k_heap *mod_drv_heap)
{ }

#endif /* CONFIG_USERSPACE */
