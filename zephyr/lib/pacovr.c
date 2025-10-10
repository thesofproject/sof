// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2025 Intel Corporation.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <sof/lib/vpage.h>
#include <sof/lib/pacovr.h>
#include <rtos/alloc.h>
#include <sof/common.h>

LOG_MODULE_REGISTER(pacovr, CONFIG_SOF_LOG_LEVEL);

/*
 * Pre Allocated COntiguous Virtual memory Region - PACOVR.
 *
 * This allocator manages a pre-allocated virtual memory region that uses the
 * virtual page allocator to allocate and free memory pages.
 *
 * It is designed for use cases where a contiguous virtual memory region
 * is required, such as for batched allocation of audio pipelines and modules.
 *
 * New pipelines will create a new PACOVR that will contain a dynamic heap at
 * the start of the region and above the dynamic heap will be a simple static
 * linear incrementing allocator for audio pipeline modules.
 *
 * The dynamic heap is used for temporary allocations during audio processing whilst
 * the static allocator is used for long term allocations that are freed when the
 * pipeline is destroyed.
 *
 * TODO: Pipeline/module reset() could reset the dynamic heap.
 */

/**
 * @brief PACOVR memory region structure.
 *
 * This structure represents a PACOVR memory region, which includes
 * information about the base address, size, and allocation status
 * of the region.
 * The PACOVR memory region is divided into two main areas:
 * 1. Dynamic Heap: A dynamic memory area used for multiple temporary allocations
 *    and frees over the lifetime of the audio processing pipeline.
 * 2. Static Allocator: A simple incrementing allocator used for long-term static
 *    allocations that persist for the lifetime of the audio processing pipeline.
 */
struct pacovr {
	uint8_t *base;			/* base address of region */
	size_t size;			/* size of whole region in bytes */
	size_t static_used;		/* used bytes in static heap */
	size_t dynamic_size;		/* size of dynamic heap */
	size_t static_size;		/* size of static heap */
	size_t pages;			/* size of region in pages */
	struct k_heap dynamic;		/* dynamic heap */
	uint8_t *static_ptr;		/* current static alloc pointer */
	int static_free_count;		/* number of static frees - tuning only */
};

/**
 * @brief Create a new PACOVR instance.
 * @param[in] static_size Size of the PACOVR static region.
 * @param[in] dynamic_size Size of the dynamic heap.
 * @return struct pacovr* Pointer to the new PACOVR instance, or NULL on failure.
 */
struct pacovr *pacovr_create(size_t static_size, size_t dynamic_size)
{
	struct pacovr *p;
	uint32_t pages;
	size_t total_size;

	if (!static_size || !dynamic_size) {
		LOG_ERR("error: invalid pacovr static size %d or dynamic size %d",
			static_size, dynamic_size);
		return NULL;
	}

	/* align up static size and dynamic size to nearest page */
	static_size = ALIGN_UP(static_size, CONFIG_MM_DRV_PAGE_SIZE);
	dynamic_size = ALIGN_UP(dynamic_size, CONFIG_MM_DRV_PAGE_SIZE);
	total_size = static_size + dynamic_size;

	/* allocate pacovr structure in userspace */
	p = rzalloc(SOF_MEM_FLAG_USER, sizeof(*p));
	if (!p)
		return NULL;

	/* allocate pages for pacovr */
	pages = (static_size + dynamic_size) / CONFIG_MM_DRV_PAGE_SIZE;
	p->base = vpage_alloc(pages);
	if (!p->base) {
		rfree(p);
		return NULL;
	}

	/* init pacovr */
	p->size = total_size;
	p->dynamic_size = dynamic_size;
	p->static_size = static_size;
	p->pages = pages;
	p->static_ptr = p->base + dynamic_size;

	/* init dynamic heap */
	sys_heap_init(&p->dynamic.heap, p->base, dynamic_size);

	LOG_INF("pacovr created at base %p total size 0x%x pages %d dynamic 0x%x static 0x%x",
		(void *)p->base, total_size, pages, dynamic_size, static_size);

	return p;
}

/**
 * @brief Destroy a PACOVR instance.
 *
 * @param[in] p Pointer to the PACOVR instance to destroy.
 */
void pacovr_destroy(struct pacovr *p)
{
	if (!p)
		return;
	LOG_INF("pacovr destroy base %p size 0x%x pages %d static used 0x%x free count %d",
		(void *)p->base, p->size, p->pages, p->static_used, p->static_free_count);
	vpage_free(p->base);
	rfree(p);
}

/**
 * @brief Allocate memory from the PACOVR dynamic heap.
 *
 * @param[in] p Pointer to the PACOVR instance.
 * @param[in] size Size of the allocation.
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
void *pacovr_dynamic_alloc(struct pacovr *p, size_t size)
{
	void *ptr;

	if (!p || !size)
		return NULL;

	ptr = sys_heap_alloc(&p->dynamic.heap, size);
	if (!ptr) {
		LOG_ERR("error: pacovr dynamic alloc failed");
		return NULL;
	}

	return ptr;
}

/**
 * @brief Allocate memory with alignment from the PACOVR dynamic heap.
 *
 * @param[in] p Pointer to the PACOVR instance.
 * @param[in] size Size of the allocation.
 * @param[in] align Alignment of the allocation.
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
void *pacovr_dynamic_alloc_align(struct pacovr *p, size_t size, size_t align)
{
	void *ptr;

	if (!p || !size)
		return NULL;

	/* align up size to 4 bytes - force aligned loads and stores */
	if (!align)
		align = sizeof(uint32_t);

	ptr = sys_heap_aligned_alloc(&p->dynamic.heap, size, align);
	if (!ptr) {
		LOG_ERR("error: pacovr dynamic alloc failed");
		return NULL;
	}

	return ptr;
}

/**
 * @brief Free memory from the PACOVR dynamic heap.
 *
 * @param[in] p Pointer to the PACOVR instance.
 * @param[in] ptr Pointer to the memory to free.
 */
void pacovr_dynamic_free(struct pacovr *p, void *ptr)
{
	if (!p || !ptr)
		return;

	sys_heap_free(&p->dynamic.heap, ptr);
}

/**
 * @brief Allocate memory from the PACOVR static allocator.
 *
 * @param[in] p Pointer to the PACOVR instance.
 * @param[in] size Size of the allocation.
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
void *pacovr_static_alloc(struct pacovr *p, size_t size)
{
	void *ptr;

	if (!p || !size)
		return NULL;

	/* align up size to 4 bytes - force aligned loads and stores */
	size = ALIGN_UP(size, 4);

	/* check we have enough static space left */
	if (p->static_used + size > p->static_size) {
		LOG_ERR("error: pacovr static alloc failed for %d bytes, only %d bytes free",
			size, p->static_size - p->static_used);
		return NULL;
	}

	/* allocate memory */
	ptr = p->static_ptr;
	p->static_ptr += size;
	p->static_used += size;
	return ptr;
}

/**
 * @brief Free memory from the PACOVR static allocator.
 *
 * @param[in] p Pointer to the PACOVR instance.
 * @param[in] ptr Pointer to the memory to free.
 */
void pacovr_static_free(struct pacovr *p, void *ptr)
{
	if (!p || !ptr)
		return;

	/* simple free, just increment free count, this is for tuning only */
	p->static_free_count++;

	LOG_DBG("pacovr static free %p count %d", ptr, p->static_free_count);
}

/**
 * @brief Log PACOVR memory usage.
 *
 * @param[in] p Pointer to the PACOVR instance.
 */
void pacovr_info(struct pacovr *p)
{
	if (!p)
		return;

	LOG_INF("pacovr info base %p size 0x%x pages %d static used 0x%x free count %d",
		(void *)p->base, p->size, p->pages, p->static_used, p->static_free_count);
}
EXPORT_SYMBOL(pacovr_info);
