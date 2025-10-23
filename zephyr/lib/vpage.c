// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2025 Intel Corporation.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/check.h>
#include <sof/lib/regions_mm.h>
#include <sof/lib/vpage.h>
#include <zephyr/drivers/mm/mm_drv_intel_adsp_mtl_tlb.h>

LOG_MODULE_REGISTER(vpage, CONFIG_SOF_LOG_LEVEL);

/* Simple Page Allocator.
 *
 * This allocator manages the allocation and deallocation of virtual memory
 * pages from a predefined virtual memory region which is larger than the
 * physical memory region.
 *
 * Both memory regions are divided into pages (usually 4kB) that are represented
 * as blocks in a bitmap using the zephyr sys_mem_blocks API. The virtual block
 * map tracks the allocation of virtual memory pages while the physical block
 * map in the Zephyr MM driver tracks the allocation of physical memory pages.
 */

/* max number of allocations */
#define VPAGE_MAX_ALLOCS	CONFIG_SOF_VPAGE_MAX_ALLOCS

/*
 * Virtual memory allocation element - tracks allocated virtual page id and size
 */
struct valloc_elem {
	unsigned short pages;	/* number of pages allocated in contiguous block */
	unsigned short vpage;	/* virtual page number from start of region */
};

/*
 * Virtual page allocator context
 *
 * This structure holds all information about virtual memory pages including the
 * number of free and total pages, the virtual memory region, the block
 * allocator for virtual pages and the allocations.
 */
struct vpage_context {
	struct k_mutex lock;
	unsigned int free_pages;		/* number of free pages */
	unsigned int total_pages;		/* total number of pages */

	/* Virtual memory region information */
	const struct sys_mm_drv_region *virtual_region;
	struct sys_mem_blocks vpage_blocks;
	sys_bitarray_t bitmap;

	/* allocation elements to track page id to allocation size */
	unsigned int num_elems_in_use;		/* number of allocated elements in use*/
	struct valloc_elem velems[VPAGE_MAX_ALLOCS];
};

/* uncache persistent across all cores */
static struct vpage_context vpage_ctx;

/**
 * @brief Allocate and map virtual memory pages
 *
 * Allocates memory pages from the virtual page allocator.
 * Maps physical memory pages to the virtual region as needed.
 *
 * @param pages Number of pages to allocate.
 * @param ptr Pointer to store the address of allocated pages.
 * @retval 0 if successful.
 */
static int vpages_alloc_and_map(unsigned int pages, void **ptr)
{
	void *vaddr;
	int ret;

	/* check for valid pages and ptr */
	if (!ptr)
		return -EINVAL;

	if (!pages) {
		*ptr = NULL;
		return 0;
	}

	/* quick check for enough free pages */
	if (vpage_ctx.free_pages < pages) {
		LOG_ERR("error: not enough free pages %u for requested pages %u",
			vpage_ctx.free_pages, pages);
		return -ENOMEM;
	}

	/* check for allocation elements */
	if (vpage_ctx.num_elems_in_use >= VPAGE_MAX_ALLOCS) {
		LOG_ERR("error: max allocation elements reached");
		return -ENOMEM;
	}

	/* allocate virtual continuous blocks */
	ret = sys_mem_blocks_alloc_contiguous(&vpage_ctx.vpage_blocks, pages, &vaddr);
	if (ret < 0) {
		LOG_ERR("error: failed to allocate %u continuous virtual pages, free %u",
			pages, vpage_ctx.free_pages);
		return ret;
	}

	/* map the virtual blocks in virtual region to free physical blocks */
	ret = sys_mm_drv_map_region_safe(vpage_ctx.virtual_region, vaddr,
					 0, pages * CONFIG_MM_DRV_PAGE_SIZE, SYS_MM_MEM_PERM_RW);
	if (ret < 0) {
		LOG_ERR("error: failed to map virtual region %p to physical region %p, error %d",
			vaddr, vpage_ctx.virtual_region->addr, ret);
		sys_mem_blocks_free_contiguous(&vpage_ctx.vpage_blocks, vaddr, pages);
		return ret;
	}

	/* success update the free pages */
	vpage_ctx.free_pages -= pages;

	/* Elements are acquired densely, just use the next one */
	vpage_ctx.velems[vpage_ctx.num_elems_in_use].pages = pages;
	vpage_ctx.velems[vpage_ctx.num_elems_in_use].vpage =
		(POINTER_TO_UINT(vaddr) -
		 POINTER_TO_UINT(vpage_ctx.virtual_region->addr)) /
		CONFIG_MM_DRV_PAGE_SIZE;
	vpage_ctx.num_elems_in_use++;

	/* return the virtual address */
	*ptr = vaddr;

	return 0;
}

/**
 * @brief Allocate virtual memory pages
 *
 * Allocates virtual memory pages from the virtual page allocator.
 *
 * @param pages Number of pages (usually 4kB large) to allocate.
 * @retval NULL on allocation failure.
 */
void *vpage_alloc(unsigned int pages)
{
	void *ptr = NULL;
	int ret;

	k_mutex_lock(&vpage_ctx.lock, K_FOREVER);
	ret = vpages_alloc_and_map(pages, &ptr);
	k_mutex_unlock(&vpage_ctx.lock);
	if (ret < 0)
		LOG_ERR("vpage_alloc failed %d for %d pages, total %d free %d",
			ret, pages, vpage_ctx.total_pages, vpage_ctx.free_pages);
	else
		LOG_INF("vpage_alloc ptr %p pages %u free %u/%u", ptr, pages, vpage_ctx.free_pages,
			vpage_ctx.total_pages);
	return ptr;
}

/**
 * @brief Free and unmap virtual memory pages
 *
 * Frees previously allocated virtual memory pages and unmaps them.
 *
 * @param ptr Pointer to the memory pages to free.
 * @retval 0 if successful.
 * @retval -EINVAL if ptr is invalid.
 */
static int vpages_free_and_unmap(uintptr_t *ptr)
{
	unsigned int alloc_idx, elem_idx;
	unsigned int pages = 0;
	int ret;

	/* check for valid ptr which must be page aligned */
	CHECKIF(!IS_ALIGNED(ptr, CONFIG_MM_DRV_PAGE_SIZE)) {
		LOG_ERR("error: invalid non aligned page pointer %p", ptr);
		return -EINVAL;
	}

	alloc_idx = (POINTER_TO_UINT(ptr) - POINTER_TO_UINT(vpage_ctx.virtual_region->addr)) /
		CONFIG_MM_DRV_PAGE_SIZE;

	/* find the allocation element */
	for (elem_idx = 0; elem_idx < VPAGE_MAX_ALLOCS; elem_idx++) {
		if (vpage_ctx.velems[elem_idx].pages > 0 &&
		    vpage_ctx.velems[elem_idx].vpage == alloc_idx) {
			pages = vpage_ctx.velems[elem_idx].pages;

			LOG_DBG("found allocation element %d pages %u vpage %u for ptr %p",
				elem_idx, vpage_ctx.velems[elem_idx].pages,
				vpage_ctx.velems[elem_idx].vpage, ptr);
			break;
		}
	}

	/* check we found allocation element */
	CHECKIF(!pages) {
		LOG_ERR("error: invalid page pointer %p not found", ptr);
		return -EINVAL;
	}

	/* unmap the pages from virtual region */
	ret = sys_mm_drv_unmap_region((void *)ptr, pages * CONFIG_MM_DRV_PAGE_SIZE);
	if (ret < 0) {
		LOG_ERR("error: failed to unmap virtual region %p pages %u, error %d",
			ptr, pages, ret);
		return ret;
	}

	/* free physical blocks */
	ret = sys_mem_blocks_free_contiguous(&vpage_ctx.vpage_blocks, ptr, pages);
	if (ret < 0) {
		LOG_ERR("error: failed to free %u continuous virtual page blocks at %p, error %d",
			pages, ptr, ret);
		return ret;
	}

	/* move the last element over the released one, clear the last element */
	if (vpage_ctx.num_elems_in_use != elem_idx)
		vpage_ctx.velems[elem_idx] = vpage_ctx.velems[vpage_ctx.num_elems_in_use];
	vpage_ctx.velems[vpage_ctx.num_elems_in_use].pages = 0;
	vpage_ctx.velems[vpage_ctx.num_elems_in_use].vpage = 0;
	vpage_ctx.num_elems_in_use--;

	/* success update the free pages */
	vpage_ctx.free_pages += pages;

	return ret;
}

/**
 * @brief Free virtual pages
 * Frees previously allocated virtual memory pages and unmaps them.
 *
 * @param ptr
 */
void vpage_free(void *ptr)
{
	int ret;

	k_mutex_lock(&vpage_ctx.lock, K_FOREVER);
	ret = vpages_free_and_unmap((uintptr_t *)ptr);
	k_mutex_unlock(&vpage_ctx.lock);

	if (!ret)
		LOG_INF("vptr %p free/total pages %d/%d", ptr, vpage_ctx.free_pages,
			vpage_ctx.total_pages);
}

/**
 * @brief Initialize virtual page allocator
 *
 * Initializes a virtual page allocator that manages a virtual memory region
 * using a page table and block structures.
 *
 * @retval 0 if successful.
 * @retval -ENOMEM on creation failure.
 */
static int vpage_init(void)
{
	const struct sys_mm_drv_region *virtual_memory_regions;
	const struct sys_mm_drv_region *region;
	uint32_t *bundles = NULL;
	unsigned int block_count, bitmap_num_bundles;
	int ret;

	/* create the virtual memory region and add it to the system */
	size_t remaining_ram = L2_SRAM_BASE + L2_SRAM_SIZE -
		(adsp_mm_get_unused_l2_start_aligned() +
		 CONFIG_SOF_ZEPHYR_VIRTUAL_HEAP_REGION_SIZE +
		 CONFIG_LIBRARY_REGION_SIZE);

	ret = adsp_add_virtual_memory_region(adsp_mm_get_unused_l2_start_aligned() +
					     CONFIG_SOF_ZEPHYR_VIRTUAL_HEAP_REGION_SIZE,
					     remaining_ram, VIRTUAL_REGION_VPAGES_ATTR);
	if (ret)
		return ret;

	k_mutex_init(&vpage_ctx.lock);

	/* now find the virtual region in all memory regions */
	virtual_memory_regions = sys_mm_drv_query_memory_regions();
	SYS_MM_DRV_MEMORY_REGION_FOREACH(virtual_memory_regions, region) {
		if (region->attr == VIRTUAL_REGION_VPAGES_ATTR) {
			vpage_ctx.virtual_region = region;
			break;
		}
	}
	sys_mm_drv_query_memory_regions_free(virtual_memory_regions);

	/* check for a valid region */
	if (!vpage_ctx.virtual_region) {
		LOG_ERR("error: no valid virtual region found");
		k_panic();
	}

	block_count = region->size / CONFIG_MM_DRV_PAGE_SIZE;
	if (block_count == 0) {
		LOG_ERR("error: virtual region too small %zu", region->size);
		k_panic();
	}

	vpage_ctx.total_pages = block_count;
	vpage_ctx.free_pages = block_count;
	vpage_ctx.num_elems_in_use = 0;

	/* bundles are uint32_t of bits */
	bitmap_num_bundles = SOF_DIV_ROUND_UP(block_count, sizeof(uint32_t) * 8);

	/* allocate memory for bitmap bundles */
	bundles = rzalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT,
			  bitmap_num_bundles * sizeof(uint32_t));
	if (!bundles) {
		LOG_ERR("error: virtual region bitmap alloc failed");
		k_panic();
	}

	/* Fill allocators data based on config and virtual region data */
	vpage_ctx.vpage_blocks.info.num_blocks = block_count;
	vpage_ctx.vpage_blocks.info.blk_sz_shift = ilog2(CONFIG_MM_DRV_PAGE_SIZE);
	/* buffer is the start of the virtual memory region */
	vpage_ctx.vpage_blocks.buffer = (uint8_t *)vpage_ctx.virtual_region->addr;

	/* initialize bitmap */
	vpage_ctx.bitmap.num_bits = block_count;
	vpage_ctx.bitmap.num_bundles = bitmap_num_bundles;
	vpage_ctx.bitmap.bundles = bundles;
	vpage_ctx.vpage_blocks.bitmap = &vpage_ctx.bitmap;

	LOG_INF("region %p size %#zx pages %u",
		(void *)vpage_ctx.virtual_region->addr,
		vpage_ctx.virtual_region->size, block_count);

	return 0;
}

SYS_INIT(vpage_init, POST_KERNEL, 1);
