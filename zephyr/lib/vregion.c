// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2025 Intel Corporation.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <sof/lib/vpages.h>
#include <sof/lib/vregion.h>
#include <rtos/alloc.h>
#include <sof/common.h>
#include <zephyr/arch/cache.h>

LOG_MODULE_REGISTER(vregion, CONFIG_SOF_LOG_LEVEL);

/*
 * Pre Allocated Contiguous Virtual Memory Region Allocator
 *
 * This allocator manages a pre-allocated virtual memory region that uses the
 * virtual page allocator to allocate and free memory pages.
 *
 * It is designed for use cases where a contiguous virtual memory region
 * is required, such as for batched allocation of audio pipelines and modules.
 *
 * New pipelines will create a new virtual region that will contain a dynamic heap at
 * the start of the region and above the dynamic heap will be a simple static
 * linear incrementing allocator for audio pipeline modules.
 *
 * The dynamic heap is used for temporary allocations during audio processing whilst
 * the static allocator is used for long term allocations that are freed when the
 * pipeline is destroyed.
 *
 * The shared static allocator is used for short term audio buffers that can be
 * shared between multiple pipelines and is reset when the pipeline is stopped.
 *
 * An optional read-only text region can be created at the start of the region
 * that can be used to hold read-only data or executable code.
 *
 * TODO: Pipeline/module reset() could reset the dynamic heap.
 */

/**
 * @brief virtual region memory region structure.
 *
 * This structure represents a virtual region memory region, which includes
 * information about the base address, size, and allocation status
 * of the region.
 *
 * The virtual region memory region can be partitioned into four main areas
 * (two are optional), listed here from base to top:
 *
 * 1. Text Region (optional): A read-only and executable region that can be used
 *    to store code or constant data. This region is optional and only present
 *    if the virtual region is created with a text size. It is page aligned and located
 *    at the start of the virtual region region. Main use case would be DP modules.
 *
 * 2. Dynamic Heap: A dynamic memory area used for multiple temporary allocations
 *    and frees over the lifetime of the audio processing pipeline.
 *
 * 3. Static Allocator: A simple incrementing allocator used for long-term static
 *    allocations that persist for the lifetime of the audio processing pipeline.
 *
 * 4. Shared Static Allocator (optional): A simple incrementing allocator used for long
 *    term static allocations that can be shared between multiple audio processing pipelines. This
 *    area is optional and only present if the virtual region is created with shared static size.
 *    It is page aligned and located after the dynamic heap and before the static allocator.
 */

struct vlinear_heap {
	uint8_t *base;		/* base address of linear allocator */
	uint8_t *ptr;		/* current alloc pointer */
	size_t size;		/* size of linear allocator in bytes */
	size_t used;		/* used bytes in linear allocator */
	int free_count;		/* number of frees - tuning only */
};

struct zephyr_heap {
	struct sys_heap heap;
	uint8_t *base;		/* base address of linear allocator */
	size_t size;		/* size of heap in bytes */
};

struct vregion {
	/* region context */
	uint8_t *base;			/* base address of entire region */
	size_t size;			/* size of whole region in bytes */
	size_t pages;			/* size of whole region in pages */

	/* optional text region - RO and Executable */
	struct vlinear_heap text;	/* text linear heap */

	/* interim heap */
	struct zephyr_heap interim;	/* interim heap */

	/* interim shared */
	struct zephyr_heap interim_shared;	/* shared interim heap */

	/* lifetime heap */
	struct vlinear_heap lifetime;	/* lifetime linear heap */

	/* optional shared static buffer heap */
	struct vlinear_heap lifetime_shared;	/* shared lifetime linear heap */
};

/**
 * @brief Create a new virtual region instance with shared pages.
 *
 * Create a new VIRTUAL REGION instance with specified static, dynamic, and shared static sizes.
 * Total size is the sum of static, dynamic, and shared static sizes.
 *
 * @param[in] lifetime_size Size of the virtual region lifetime partition.
 * @param[in] interim_size Size of the virtual region interim partition.
 * @param[in] lifetime_shared_size Size of the virtual region shared lifetime partition.
 * @param[in] interim_shared_size Size of the virtual region shared interim partition.
 * @param[in] text_size Size of the optional read-only text partition.
 * @return struct vregion* Pointer to the new virtual region instance, or NULL on failure.
 */
struct vregion *vregion_create(size_t lifetime_size, size_t interim_size,
			       size_t lifetime_shared_size, size_t interim_shared_size,
			       size_t text_size)
{
	struct vregion *vr;
	uint32_t pages;
	size_t total_size;

	if (!lifetime_size || !interim_size) {
		LOG_ERR("error: invalid vregion lifetime size %d or interim size %d",
			lifetime_size, interim_size);
		return NULL;
	}

	/* align up lifetime size and interim size to nearest page */
	lifetime_size = ALIGN_UP(lifetime_size, CONFIG_MM_DRV_PAGE_SIZE);
	interim_size = ALIGN_UP(interim_size, CONFIG_MM_DRV_PAGE_SIZE);
	if (lifetime_shared_size)
		lifetime_shared_size = ALIGN_UP(lifetime_shared_size, CONFIG_MM_DRV_PAGE_SIZE);
	if (interim_shared_size)
		interim_shared_size = ALIGN_UP(interim_shared_size, CONFIG_MM_DRV_PAGE_SIZE);
	if (text_size)
		text_size = ALIGN_UP(text_size, CONFIG_MM_DRV_PAGE_SIZE);
	total_size = lifetime_size + interim_size +
		lifetime_shared_size + interim_shared_size + text_size;

	/* allocate vregion structure in userspace */
	vr = rzalloc(SOF_MEM_FLAG_USER, sizeof(*vr));
	if (!vr)
		return NULL;

	/* allocate pages for vregion */
	pages = total_size / CONFIG_MM_DRV_PAGE_SIZE;
	vr->base = alloc_vpages(pages);
	if (!vr->base) {
		rfree(vr);
		return NULL;
	}

	/* init vregion */
	vr->size = total_size;
	vr->pages = pages;

	/* set partition sizes */
	vr->interim.size = interim_size;
	vr->interim_shared.size = interim_shared_size;
	vr->lifetime.size = lifetime_size;
	vr->lifetime_shared.size = lifetime_shared_size;
	vr->text.size = text_size;

	/* set base addresses for partitions */
	vr->text.base = vr->base;
	vr->interim.base = vr->text.base + text_size;
	vr->lifetime.base = vr->interim.base + interim_size;
	vr->lifetime_shared.base = vr->lifetime.base + lifetime_size;
	vr->interim_shared.base = vr->lifetime_shared.base + lifetime_shared_size;

	/* set alloc ptr addresses for lifetime linear partitions */
	vr->text.ptr = vr->text.base;
	vr->lifetime.ptr = vr->lifetime.base;
	vr->lifetime_shared.ptr = vr->lifetime_shared.base;

	/* init interim heaps */
	sys_heap_init(&vr->interim.heap, vr->interim.base, interim_size);
	if (interim_shared_size) {
		sys_heap_init(&vr->interim_shared.heap, vr->interim_shared.base,
			      interim_shared_size);
	}

	LOG_INF("new at %p size 0x%x pages %d",
		 (void *)vr->base, total_size, pages);
	LOG_INF(" interim size 0x%x at %p", interim_size, (void *)vr->interim.base);
	LOG_INF(" lifetime size 0x%x at %p", lifetime_size, (void *)vr->lifetime.base);
	if (interim_shared_size)
		LOG_INF(" interim shared size 0x%x at %p", interim_shared_size,
			(void *)vr->interim_shared.base);
	if (lifetime_shared_size)
		LOG_INF(" lifetime shared size 0x%x at %p", lifetime_shared_size,
			(void *)vr->lifetime_shared.base);
	if (text_size)
		LOG_INF(" text size 0x%x at %p", text_size, (void *)vr->text.base);

	return vr;
}

/**
 * @brief Destroy a virtual region instance.
 *
 * @param[in] vr Pointer to the virtual region instance to destroy.
 */
void vregion_destroy(struct vregion *vr)
{
	if (!vr)
		return;

	LOG_INF("destroy %p size 0x%x pages %d",
		(void *)vr->base, vr->size, vr->pages);
	LOG_INF(" lifetime used %d free count %d",
		vr->lifetime.used, vr->lifetime.free_count);
	if (vr->lifetime_shared.size)
		LOG_INF(" lifetime shared used %d free count %d",
			vr->lifetime_shared.used, vr->lifetime_shared.free_count);
	free_vpages(vr->base);
	rfree(vr);
}


/**
 * @brief Allocate memory with alignment from the virtual region dynamic heap.
 *
 * @param[in] vr Pointer to the virtual region instance.
 * @param[in] heap Pointer to the heap to use.
 * @param[in] size Size of the allocation.
 * @param[in] align Alignment of the allocation.
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
static void *interim_alloc(struct vregion *vr, struct zephyr_heap *heap,
			   size_t size, size_t align)
{
	void *ptr;

	/* align up size to 4 bytes - force aligned loads and stores */
	if (!align)
		align = sizeof(uint32_t);

	ptr = sys_heap_aligned_alloc(&heap->heap, size, align);
	if (!ptr) {
		LOG_ERR("error: interim alloc failed for %d bytes align %d",
			size, align);
		return NULL;
	}

	return ptr;
}

/**
 * @brief Free memory from the virtual region interim heap.
 *
 * @param[in] vr Pointer to the virtual region instance.
 * @param[in] heap Pointer to the heap to use.
 * @param[in] ptr Pointer to the memory to free.
 */
static void interim_free(struct vregion *vr, struct zephyr_heap *heap, void *ptr)
{
	sys_heap_free(&heap->heap, ptr);
}

/**
 * @brief Allocate memory from the virtual region lifetime allocator.
 *
 * @param[in] vr Pointer to the virtual region instance.
 * @param[in] heap Pointer to the linear heap to use.
 * @param[in] size Size of the allocation.
 * @param[in] align Alignment of the allocation.
 * @param[in] align_size If non-zero also align up size to D$ line size.
 *
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
static void *lifetime_alloc(struct vregion *vr, struct vlinear_heap *heap,
			    size_t size, size_t align, size_t align_size)
{
	void *ptr;
	uint8_t *aligned_ptr;
	size_t heap_obj_size;

	/* align up size to 4 bytes - force aligned loads and stores */
	if (!align)
		align = sizeof(uint32_t);

	/* align heap pointer to alignment */
	aligned_ptr = UINT_TO_POINTER(ALIGN_UP(POINTER_TO_UINT(heap->ptr), align));

	/* also align up size to D$ bytes if asked - allocation head and tail aligned */
	if (align_size && align_size < CONFIG_DCACHE_LINE_SIZE)
		size = ALIGN_UP(size, CONFIG_DCACHE_LINE_SIZE);

	/* calculate new heap object size for object and alignments */
	heap_obj_size = aligned_ptr - heap->ptr + size;

	/* check we have enough shared static space left */
	if (heap_obj_size + heap->used > heap->size) {
		LOG_ERR("error: shared alloc failed for object %d heap %d bytes free %d",
			size, heap_obj_size, heap->size - heap->used);
		return NULL;
	}

	/* allocate memory */
	ptr = aligned_ptr;
	heap->ptr += heap_obj_size;
	heap->used += heap_obj_size;
	return ptr;
}

/**
 * @brief Free memory from the virtual region lifetime allocator.
 *
 * @param[in] vr Pointer to the virtual region instance.
 * @param[in] ptr Pointer to the memory to free.
 */
void lifetime_free(struct vregion *vr, struct vlinear_heap *heap, void *ptr)
{
	/* simple free, just increment free count, this is for tuning only */
	heap->free_count++;

	LOG_DBG("lifetime free %p count %d", ptr, heap->free_count);
}

/**
 * @brief Free memory from the virtual region.
 *
 * @param vr Pointer to the virtual region instance.
 * @param ptr Pointer to the memory to free.
 */
void vregion_free(struct vregion *vr, void *ptr)
{
	if (!vr || !ptr)
		return;

	/* check if pointer is in interim heap */
	if (ptr >= (void *)vr->interim.base &&
	    ptr < (void *)(vr->interim.base + vr->interim.size)) {
		interim_free(vr, &vr->interim, ptr);
		return;
	}

	/* check if pointer is in interim shared heap */
	if (vr->interim_shared.size &&
	    ptr >= (void *)vr->interim_shared.base &&
	    ptr < (void *)(vr->interim_shared.base + vr->interim_shared.size)) {
		interim_free(vr, &vr->interim_shared, ptr);
		return;
	}

	/* check if pointer is in lifetime heap */
	if (ptr >= (void *)vr->lifetime.base &&
	    ptr < (void *)(vr->lifetime.base + vr->lifetime.size)) {
		lifetime_free(vr, &vr->lifetime, ptr);
		return;
	}

	/* check if pointer is in lifetime shared heap */
	if (vr->lifetime_shared.size &&
	    ptr >= (void *)vr->lifetime_shared.base &&
	    ptr < (void *)(vr->lifetime_shared.base + vr->lifetime_shared.size)) {
		lifetime_free(vr, &vr->lifetime_shared, ptr);
		return;
	}

	LOG_ERR("error: vregion free invalid pointer %p", ptr);
}

/**
 * @brief Allocate memory type from the virtual region.
 *
 * @param[in] vr Pointer to the virtual region instance.
 * @param[in] type Memory type to allocate.
 * @param[in] size Size of the allocation.
 * @param[in] alignment Alignment of the allocation.
 *
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
void *vregion_alloc_align(struct vregion *vr, enum vregion_mem_type type,
			  size_t size, size_t alignment)
{
	if (!vr || !size)
		return NULL;

	switch (type) {
	case VREGION_MEM_TYPE_INTERIM:
		return interim_alloc(vr, &vr->interim, size, alignment);
	case VREGION_MEM_TYPE_LIFETIME:
		return lifetime_alloc(vr, &vr->lifetime, size, alignment, 0);
	case VREGION_MEM_TYPE_INTERIM_SHARED:
		return interim_alloc(vr, &vr->interim_shared, size, alignment);
	case VREGION_MEM_TYPE_LIFETIME_SHARED:
		return lifetime_alloc(vr, &vr->lifetime_shared, size,
			alignment < CONFIG_DCACHE_LINE_SIZE ? CONFIG_DCACHE_LINE_SIZE : alignment,
			CONFIG_DCACHE_LINE_SIZE);
	default:
		LOG_ERR("error: invalid memory type %d", type);
		return NULL;
	}
}

/**
 * @brief Allocate memory from the virtual region.
 * @param[in] vr Pointer to the virtual region instance.
 * @param[in] type Memory type to allocate.
 * @param[in] size Size of the allocation.
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
void *vregion_alloc(struct vregion *vr, enum vregion_mem_type type, size_t size)
{
	return vregion_alloc_align(vr, type, size, 0);
}

/**
 * @brief Log virtual region memory usage.
 *
 * @param[in] vr Pointer to the virtual region instance.
 */
void vregion_info(struct vregion *vr)
{
	if (!vr)
		return;

	LOG_INF("base %p size 0x%x pages %d",
		(void *)vr->base, vr->size, vr->pages);
	LOG_INF("lifetime used 0x%x free count %d",
		vr->lifetime.used, vr->lifetime.free_count);
	LOG_INF("lifetime shared used 0x%x free count %d",
		vr->lifetime_shared.used, vr->lifetime_shared.free_count);
}
EXPORT_SYMBOL(vregion_info);
