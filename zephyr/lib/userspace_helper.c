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
#include <../../../../../zephyr/zephyr/arch/xtensa/include/xtensa_mmu_priv.h>

#if CONFIG_USERSPACE

K_APPMEM_PARTITION_DEFINE(common_partition);

struct sys_heap *module_driver_heap_init(void)
{
	struct sys_heap *mod_drv_heap = rballoc(SOF_MEM_FLAG_USER, sizeof(struct sys_heap));

	if (!mod_drv_heap)
		return NULL;

	void *mem = rballoc_align(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT, USER_MOD_HEAP_SIZE,
				  CONFIG_MM_DRV_PAGE_SIZE);
	if (!mem) {
		rfree(mod_drv_heap);
		return NULL;
	}

	sys_heap_init(mod_drv_heap, mem, USER_MOD_HEAP_SIZE);
	mod_drv_heap->init_mem = mem;
	mod_drv_heap->init_bytes = USER_MOD_HEAP_SIZE;

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

int user_add_memory(struct k_mem_domain *domain, uintptr_t addr, size_t size, uint32_t attr)
{
	struct k_mem_partition part;
	int ret;

	k_mem_region_align(&part.start, &part.size, addr, size, HOST_PAGE_SIZE);
	/* Define parameters for partition */
	part.attr = attr;
	ret = k_mem_domain_add_partition(domain, &part);
	/* -EINVAL means that given page is already in the domain */
	/* Not an error case for us. */
	if (ret == -EINVAL)
		return 0;

	return ret;
}

int user_remove_memory(struct k_mem_domain *domain, uintptr_t addr, size_t size)
{
	struct k_mem_partition part;
	int ret;

	/* Define parameters for user_partition */
	k_mem_region_align(&part.start, &part.size, addr, size, HOST_PAGE_SIZE);
	ret = k_mem_domain_remove_partition(domain, &part);
	/* -ENOENT means that given partition is already removed */
	/* Not an error case for us. */
	if (ret == -ENOENT)
		return 0;

	return ret;
}

extern struct tr_ctx userspace_proxy_tr;

LOG_MODULE_DECLARE(userspace_proxy, CONFIG_SOF_LOG_LEVEL);

static void dump_pte_attr(char *attr_s, uint32_t attr)
{
	attr_s[0] = attr & XTENSA_MMU_CACHED_WT ? 'T' : '-';
	attr_s[1] = attr & XTENSA_MMU_CACHED_WB ? 'B' : '-';
	attr_s[2] = attr & XTENSA_MMU_PERM_W ? 'W' : '-';
	attr_s[3] = attr & XTENSA_MMU_PERM_X ? 'X' : '-';
	attr_s[4] = '\0';
}

static uint32_t *dump_pte(uint32_t pte)
{
	uint32_t ppn = pte & XTENSA_MMU_PTE_PPN_MASK;
	uint32_t ring = XTENSA_MMU_PTE_RING_GET(pte);
	uint32_t sw = XTENSA_MMU_PTE_SW_GET(pte);
	uint32_t sw_ring = XTENSA_MMU_PTE_SW_RING_GET(sw);
	uint32_t sw_attr = XTENSA_MMU_PTE_SW_ATTR_GET(sw);
	uint32_t attr = XTENSA_MMU_PTE_ATTR_GET(pte);

	char attr_s[5];
	dump_pte_attr(attr_s, attr);

	char sw_attr_s[5];
	dump_pte_attr(sw_attr_s, sw_attr);

	tr_err(&userspace_proxy_tr, "PPN %#x, sw %#x (ring: %u, %s), ring %u %s",
	       ppn, sw, sw_ring, sw_attr_s, ring, attr_s);

	if ((attr & XTENSA_MMU_PTE_ATTR_ILLEGAL) == XTENSA_MMU_PTE_ATTR_ILLEGAL) {
		tr_err(&userspace_proxy_tr, "ILLEGAL PTE");
		return NULL;
	}

	return (uint32_t *)ppn;
}

void dump_page_table(uint32_t *ptables, void *test)
{
	const uint32_t l1_index = XTENSA_MMU_L1_POS(POINTER_TO_UINT(test));
	const uint32_t l2_index = XTENSA_MMU_L2_POS(POINTER_TO_UINT(test));
	const uint32_t test_aligned = POINTER_TO_UINT(test) & ~(CONFIG_MMU_PAGE_SIZE - 1);

	tr_err(&userspace_proxy_tr, "test %p, ptables = %p, L1 = %#x, L2 = %#x", test,
	       (void *)ptables, l1_index, l2_index);

	uint32_t *const l1_entry = ptables + l1_index;
	tr_err(&userspace_proxy_tr, "l1 @ %p = %p", (void *)l1_entry, *l1_entry);

	uint32_t* l1_ppn = dump_pte(*l1_entry);
	if (!l1_ppn) {
		tr_err(&userspace_proxy_tr, "INVALID L1 PTE!");
		return;
	}

	uint32_t *const l2_entry = l1_ppn + l2_index;
	tr_err(&userspace_proxy_tr, "l2 @ %p = %p", (void *)l2_entry, *l2_entry);
	uint32_t *l2_ppn = dump_pte(*l2_entry);

	if (test_aligned != POINTER_TO_UINT(l2_ppn)) {
		tr_err(&userspace_proxy_tr, "INVALID L2 PTE!");
		return;
	}
}

extern uint32_t *xtensa_kernel_ptables;

void dump_page_tables(uint32_t *ptables, void *test, bool kernel)
{
	if (ptables) {
		tr_err(&userspace_proxy_tr, "Dump for %p in user table", test);
		dump_page_table(ptables, sys_cache_cached_ptr_get(test));
		dump_page_table(ptables, sys_cache_uncached_ptr_get(test));
	}

	if (kernel) {
		tr_err(&userspace_proxy_tr, "Kernel table", test);
		dump_page_table(xtensa_kernel_ptables, sys_cache_cached_ptr_get(test));
		dump_page_table(xtensa_kernel_ptables, sys_cache_uncached_ptr_get(test));
	}
}

static void dump_domain_attr(char *attr_s, uint32_t attr)
{
	attr_s[0] = attr & XTENSA_MMU_MAP_SHARED ? 'S' : '-';
	attr_s[1] = K_MEM_PARTITION_IS_USER(attr) ? 'U' : '-';
	dump_pte_attr(attr_s + 2, attr);
}

void dump_memory_domain(struct k_mem_domain *domain)
{
	int i;
	char attrs[7];

	for (i = 0; i < domain->num_partitions; i++) {
		dump_domain_attr(attrs, domain->partitions[i].attr);

		tr_err(&userspace_proxy_tr, "partitions[%d]: %p + %#zx %s", i,
		       UINT_TO_POINTER(domain->partitions[i].start),
		       domain->partitions[i].size, attrs);
	}

	tr_err(&userspace_proxy_tr, "ptables = %p, asid = %u, dirty = %u",
	       (void *)domain->arch.ptables, domain->arch.asid, domain->arch.dirty);
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
