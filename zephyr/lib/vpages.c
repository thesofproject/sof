// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2025 Intel Corporation.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <sof/lib/regions_mm.h>
#include <zephyr/drivers/mm/mm_drv_intel_adsp_mtl_tlb.h>

LOG_MODULE_REGISTER(vpage, CONFIG_SOF_LOG_LEVEL);

/* Simple Page Allocator.
 *
 * This allocator manages the allocation and deallocation of virtual memory pages from
 * a predefined virtual memory region roughly twice the size of the physical memory
 * region.
 *
 * Both memory regions are divided into 4kB pages that are represented as blocks in a
 * bitmap using the zephyr sys_mem_blocks API.
 *
 * The blocks APIs track the allocation of physical memory pages and virtual memory pages
 * separately. The physical memory pages are mapped to the virtual memory pages as needed to
 * provide a contiguous virtual memory space to the user.
 */

#define VPAGE_ALLOC_ELEMS	CONFIG_SOF_VPAGE_ELEMS	/* max number of allocation elements */

/*
 * Virtual memory allocation element - tracks allocated virtual page id and size
 */
struct valloc_elem {
	uint16_t pages;		/* number of 4kB pages allocated in contiguous block */
	uint16_t vpage;		/* virtual page number from start of region */
};

/*
 * Virtual page table structure
 *
 * This structure holds all information about virtual memory pages
 * including the number of free and total pages, the virtual memory
 * region, the block allocator for virtual pages and the allocation
 * elements.
 */
struct vpage_context {
	struct k_mutex lock;
	uint32_t free_pages;	/* number of free 4kB pages */
	uint32_t total_pages;	/* total number of 4kB pages */

	/* Virtual memory region information */
	const struct sys_mm_drv_region *virtual_region;
	struct sys_mem_blocks vpage_blocks;

	/* allocation elements to track page id to allocation size */
	uint32_t num_elems;	/* number of allocated elements in use*/
	struct valloc_elem velems[VPAGE_ALLOC_ELEMS];
};

/* uncache persistent across all cores */
static struct vpage_context page_context;
static sys_bitarray_t bitmap;

/* singleton across all cores */
static int vpage_init_done;

/**
 * @brief Allocate and map virtual memory pages
 *
 * Allocates memory pages from the virtual page allocator.
 * Maps physical memory pages to the virtual region as needed.
 *
 * @param pages Number of 4kB pages to allocate.
 * @param ptr Pointer to store the address of allocated pages.
 * @retval 0 if successful.
 */
static int vpages_alloc_and_map(uint32_t pages, void **ptr)
{
	void *vaddr;
	int ret;

	/* check for valid pages and ptr */
	if (!pages || !ptr)
		return -EINVAL;

	*ptr = NULL;

	/* quick check for enough free pages */
	if (page_context.free_pages < pages) {
		LOG_ERR("error: not enough free pages %d for requested pages %d",
			page_context.free_pages, pages);
		return -ENOMEM;
	}

	/* check for allocation elements */
	if (page_context.num_elems >= VPAGE_ALLOC_ELEMS) {
		LOG_ERR("error: max allocation elements reached");
		return -ENOMEM;
	}

	/* allocate virtual continuous blocks */
	ret = sys_mem_blocks_alloc_contiguous(&page_context.vpage_blocks, pages, &vaddr);
	if (ret < 0) {
		LOG_ERR("error: failed to allocate %d continuous virtual pages, free %d",
			pages, page_context.free_pages);
		return ret;
	}

	/* map the virtual blocks in virtual region to free physical blocks */
	ret = sys_mm_drv_map_region_safe(page_context.virtual_region, vaddr,
					 0, pages * CONFIG_MM_DRV_PAGE_SIZE, SYS_MM_MEM_PERM_RW);
	if (ret < 0) {
		LOG_ERR("error: failed to map virtual region %p to physical region %p, error %d",
			vaddr, page_context.virtual_region->addr, ret);
		sys_mem_blocks_free(&page_context.vpage_blocks, pages, &vaddr);
		return ret;
	}

	/* success update the free pages */
	page_context.free_pages -= pages;

	/* store the size and virtual page number in first free alloc element,
	 * we have already checked for a free element before the mapping.
	 */
	for (int i = 0; i < VPAGE_ALLOC_ELEMS; i++) {
		if (page_context.velems[i].pages == 0) {
			page_context.velems[i].pages = pages;
			page_context.velems[i].vpage =
				(POINTER_TO_UINT(vaddr) -
				POINTER_TO_UINT(page_context.vpage_blocks.buffer)) /
				CONFIG_MM_DRV_PAGE_SIZE;
			page_context.num_elems++;
			break;
		}
	}

	/* return the virtual address */
	*ptr = vaddr;
	return ret;
}

/**
 * @brief Allocate virtual memory pages
 *
 * Allocates virtual memory pages from the virtual page allocator.
 *
 * @param pages Number of 4kB pages to allocate.
 * @retval ptr to allocated pages if successful.
 * @retval NULL on allocation failure.
 */
void *vpage_alloc(uint32_t pages)
{
	void *ptr;
	int err;

	k_mutex_lock(&page_context.lock, K_FOREVER);
	err = vpages_alloc_and_map(pages, &ptr);
	k_mutex_unlock(&page_context.lock);
	if (err < 0) {
		LOG_ERR("vpage_alloc failed %d for %d pages, total %d free %d",
			err, pages, page_context.total_pages, page_context.free_pages);
	}
	LOG_INF("vpage_alloc ptr %p pages %d free %d/%d", ptr, pages, page_context.free_pages,
		page_context.total_pages);
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
	int pages = 0;
	int ret;

	/* check for valid ptr which must be page aligned */
	if (!ptr || ((uintptr_t)ptr % CONFIG_MM_DRV_PAGE_SIZE) != 0) {
		LOG_ERR("error: invalid page pointer %p", ptr);
		return -EINVAL;
	}

	/* find the allocation element */
	for (int i = 0; i < VPAGE_ALLOC_ELEMS; i++) {
		if (page_context.velems[i].pages > 0 &&
		    page_context.velems[i].vpage ==
		    (POINTER_TO_UINT(ptr) - POINTER_TO_UINT(page_context.vpage_blocks.buffer)) /
		    CONFIG_MM_DRV_PAGE_SIZE) {

			pages = page_context.velems[i].pages;

			LOG_DBG("found allocation element %d pages %d vpage %d for ptr %p",
				i, page_context.velems[i].pages,
				page_context.velems[i].vpage, ptr);

			/* clear the element */
			page_context.velems[i].pages = 0;
			page_context.velems[i].vpage = 0;
			page_context.num_elems--;
			break;
		}
	}

	/* check we found allocation element */
	if (pages == 0) {
		LOG_ERR("error: invalid page pointer %p not found", ptr);
		return -EINVAL;
	}

	/* unmap the pages from virtual region */
	ret = sys_mm_drv_unmap_region((void *)ptr, pages * CONFIG_MM_DRV_PAGE_SIZE);
	if (ret < 0) {
		LOG_ERR("error: failed to unmap virtual region %p pages %d, error %d",
			ptr, pages, ret);
		return ret;
	}

	/* free physical blocks */
	ret = sys_mem_blocks_free_contiguous(&page_context.vpage_blocks, ptr, pages);
	if (ret < 0) {
		LOG_ERR("error: failed to free %d continuous virtual page blocks at %p, error %d",
			pages, ptr, ret);
		return ret;
	}

	/* success update the free pages */
	page_context.free_pages += pages;
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
	k_mutex_lock(&page_context.lock, K_FOREVER);
	vpages_free_and_unmap((uintptr_t *)ptr);
	assert(!ret); /* should never fail */
	k_mutex_unlock(&page_context.lock);
	LOG_INF("vpage_free done ptr %p free pages %d/%d", ptr, page_context.free_pages,
		page_context.total_pages);
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
	size_t block_count, bitmap_num_bundles;
	int ret;

	/* Check if already initialized */
	if (vpage_init_done)
		return 0;

	/* create the virtual memory region and add it to the system */
	ret = adsp_add_virtual_memory_region(adsp_mm_get_unused_l2_start_aligned(),
					     CONFIG_SOF_ZEPHYR_VIRTUAL_HEAP_REGION_SIZE,
					     VIRTUAL_REGION_SHARED_HEAP_ATTR);
	if (ret)
		return ret;

	memset(&page_context, 0, sizeof(page_context));
	k_mutex_init(&page_context.lock);

	/* now find the virtual region in all memory regions */
	virtual_memory_regions = sys_mm_drv_query_memory_regions();
	SYS_MM_DRV_MEMORY_REGION_FOREACH(virtual_memory_regions, region) {
		if (region->attr == VIRTUAL_REGION_SHARED_HEAP_ATTR) {
			page_context.virtual_region = region;
			break;
		}
	}

	/* check for a valid region */
	if (!page_context.virtual_region) {
		LOG_ERR("error: no valid virtual region found");
		return -EINVAL;
	}

	block_count = region->size / CONFIG_MM_DRV_PAGE_SIZE;
	if (block_count == 0) {
		LOG_ERR("error: virtual region too small %d", region->size);
		return -ENOMEM;
	}
	page_context.total_pages = block_count;
	page_context.free_pages = block_count;
	page_context.num_elems = 0;

	/* bundles are uint32_t of bits */
	bitmap_num_bundles = SOF_DIV_ROUND_UP(block_count, 32);

	/* allocate memory for bitmap bundles */
	bundles = rzalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT,
			  bitmap_num_bundles * sizeof(uint32_t));
	if (!bundles) {
		LOG_ERR("error: virtual region bitmap alloc failed");
		return -ENOMEM;
	}

	/* Fill allocators data based on config and virtual region data */
	page_context.vpage_blocks.info.num_blocks = block_count;
	page_context.vpage_blocks.info.blk_sz_shift = 12; /* 4kB blocks */
	/* buffer is the start of the physical memory region */
	page_context.vpage_blocks.buffer = (uint8_t *)page_context.virtual_region->addr;

	/* initialize bitmap */
	bitmap.num_bits = block_count;
	bitmap.num_bundles = bitmap_num_bundles;
	bitmap.bundles = bundles;
	page_context.vpage_blocks.bitmap = &bitmap;

	LOG_INF("vpage_init region %p size 0x%x pages %d",
		(void *)page_context.virtual_region->addr,
		(int)page_context.virtual_region->size, block_count);

	vpage_init_done = 1;
	return 0;
}

SYS_INIT(vpage_init, POST_KERNEL, 1);

