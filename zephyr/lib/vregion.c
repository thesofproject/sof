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
 * New pipelines will create a new virtual region and will specify the size of the region
 * which can be divided into multiple areas for different allocation lifetimes, permissions
 * and sharing requirements.
 *
 * Advantages:
 *
 * 1) Contiguous virtual memory region for easier management and tracking of
 *    pipeline & DP module memory. i.e. we just need to track the vregion pointer.
 * 2) Easier management of memory protection and sharing between different cores
 *    and domains by partitioning the virtual region into different areas with
 *    specific permissions and sharing requirements.
 * 3) Reduced fragmentation and better cache utilization by using a simple linear
 *    allocator for lifetime objects.
 *
 * Note: Software must pass in the size of the region areas at pipeline creation time.
 */

/**
 * @brief virtual region memory region structure.
 *
 * This structure represents a virtual memory region, which includes
 * information about the base address, size, and allocation status
 * of the region.
 *
 * The virtual region memory region can be partitioned into five main areas on
 * page-aligned boundaries (some are optional), listed here from base to top:
 *
 * 1. Text Region (optional): A read-only and executable region that can be used
 *    to store code or constant data. This region is optional and only present
 *    if the virtual region is created with a text size. It is page aligned and located
 *    at the start of the virtual region. Main use case would be DP module text.
 *
 * 2. Interim Heap: A interim memory area used for multiple temporary allocations
 *    and frees over the lifetime of the audio processing pipeline. e.g. for
 *    module kcontrol derived allocations/frees.
 *
 * 3. Shared Interim Heap (optional): A interim memory area used for multiple temporary
 *    allocations/frees that can be shared between multiple cores or memory domains. e.g
 *    shared buffers between different cores or domains.
 *
 * 4. Lifetime Allocator: A simple incrementing allocator used for long-term static
 *    allocations that persist for the lifetime of the audio processing pipeline. This
 *    allocator compresses allocations for better cache utilization.
 *
 * 5. Shared Lifetime Allocator (optional): A simple incrementing allocator used for long
 *    term static allocations that can be shared between multiple cores or domains. This
 *    allocator aligns allocations to cache line boundaries to ensure cache coherency.
 *
 *  * TODO: Pipeline/module reset() could reset the dynamic heap.
 */

 /* linear heap used for lifetime allocations */
struct vlinear_heap {
	uint8_t *base;		/* base address of linear allocator */
	uint8_t *ptr;		/* current alloc pointer */
	size_t size;		/* size of linear allocator in bytes */
	size_t used;		/* used bytes in linear allocator */
	int free_count;		/* number of frees - tuning only */
};

/* zephyr k_heap for interim allocations. TODO: make lockless for improved performance */
struct zephyr_heap {
	struct k_heap heap;
	uint8_t *base;		/* base address of zephyr heap allocator */
	size_t size;		/* size of heap in bytes */
};

/* Main vregion context, see above intro for more details.
 * TODO: Add support to flag which heaps should have their contexts saved and restored.
 */
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
	uint8_t *vregion_base;

	if (!lifetime_size || !interim_size) {
		LOG_ERR("error: invalid vregion lifetime size %d or interim size %d",
			lifetime_size, interim_size);
		return NULL;
	}

	/*
	 * Align up lifetime sizes and interim sizes to nearest page, the
	 * vregion structure is stored in lifetime area so account for its size too.
	 */
	lifetime_size += sizeof(*vr);
	lifetime_size = ALIGN_UP(lifetime_size, CONFIG_MM_DRV_PAGE_SIZE);
	interim_size = ALIGN_UP(interim_size, CONFIG_MM_DRV_PAGE_SIZE);
	lifetime_shared_size = ALIGN_UP(lifetime_shared_size, CONFIG_MM_DRV_PAGE_SIZE);
	interim_shared_size = ALIGN_UP(interim_shared_size, CONFIG_MM_DRV_PAGE_SIZE);
	text_size = ALIGN_UP(text_size, CONFIG_MM_DRV_PAGE_SIZE);
	total_size = lifetime_size + interim_size +
		lifetime_shared_size + interim_shared_size + text_size;

	/* allocate pages for vregion */
	pages = total_size / CONFIG_MM_DRV_PAGE_SIZE;
	vregion_base = alloc_vpages(pages);
	if (!vregion_base)
		return NULL;

	/* init vregion - place it at the start of the lifetime region */
	vr = (struct vregion *)(vregion_base + text_size + interim_size);
	vr->base = vregion_base;
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
	vr->lifetime.ptr = vr->lifetime.base + sizeof(*vr); /* skip vregion struct */
	vr->lifetime.used = sizeof(*vr);
	vr->lifetime_shared.ptr = vr->lifetime_shared.base;

	/* init interim heaps */
	k_heap_init(&vr->interim.heap, vr->interim.base, interim_size);
	if (interim_shared_size) {
		k_heap_init(&vr->interim_shared.heap, vr->interim_shared.base,
			    interim_shared_size);
	}

	/* log the new vregion */
	LOG_INF("new at base %p size 0x%x pages %d struct embedded at %p",
		(void *)vr->base, total_size, pages, (void *)vr);
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

	/* log the vregion being destroyed */
	LOG_INF("destroy %p size 0x%x pages %d",
		(void *)vr->base, vr->size, vr->pages);
	LOG_INF(" lifetime used %d free count %d",
		vr->lifetime.used, vr->lifetime.free_count);
	if (vr->lifetime_shared.size)
		LOG_INF(" lifetime shared used %d free count %d",
			vr->lifetime_shared.used, vr->lifetime_shared.free_count);
	free_vpages(vr->base);
}

/**
 * @brief Allocate memory with alignment from the virtual region dynamic heap.
 *
 * @param[in] heap Pointer to the heap to use.
 * @param[in] size Size of the allocation.
 * @param[in] align Alignment of the allocation.
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
static void *interim_alloc(struct zephyr_heap *heap,
			   size_t size, size_t align)
{
	void *ptr;

	ptr = k_heap_aligned_alloc(&heap->heap, size, align, K_FOREVER);
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
 * @param[in] heap Pointer to the heap to use.
 * @param[in] ptr Pointer to the memory to free.
 */
static void interim_free(struct zephyr_heap *heap, void *ptr)
{
	k_heap_free(&heap->heap, ptr);
}

/**
 * @brief Allocate memory from the virtual region lifetime allocator.
 *
 * @param[in] heap Pointer to the linear heap to use.
 * @param[in] size Size of the allocation.
 * @param[in] align Alignment of the allocation.
 *
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
static void *lifetime_alloc(struct vlinear_heap *heap,
			    size_t size, size_t align)
{
	void *ptr;
	uint8_t *aligned_ptr;
	size_t heap_obj_size;

	/* align heap pointer to alignment requested */
	aligned_ptr = UINT_TO_POINTER(ALIGN_UP(POINTER_TO_UINT(heap->ptr), align));

	/* also align up size to D$ bytes if asked - allocation head and tail aligned */
	if (align == CONFIG_DCACHE_LINE_SIZE)
		size = ALIGN_UP(size, CONFIG_DCACHE_LINE_SIZE);

	/* calculate new heap object size for object and alignments */
	heap_obj_size = aligned_ptr - heap->ptr + size;

	/* check we have enough lifetime space left */
	if (heap_obj_size + heap->used > heap->size) {
		LOG_ERR("error: lifetime alloc failed for object %d heap %d bytes free %d",
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
 * @param[in] heap Pointer to the linear heap to use.
 * @param[in] ptr Pointer to the memory to free.
 */
static void lifetime_free(struct vlinear_heap *heap, void *ptr)
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
		interim_free(&vr->interim, ptr);
		return;
	}

	/* check if pointer is in interim shared heap */
	if (vr->interim_shared.size &&
	    ptr >= (void *)vr->interim_shared.base &&
	    ptr < (void *)(vr->interim_shared.base + vr->interim_shared.size)) {
		interim_free(&vr->interim_shared, ptr);
		return;
	}

	/* check if pointer is in lifetime heap */
	if (ptr >= (void *)vr->lifetime.base &&
	    ptr < (void *)(vr->lifetime.base + vr->lifetime.size)) {
		lifetime_free(&vr->lifetime, ptr);
		return;
	}

	/* check if pointer is in lifetime shared heap */
	if (vr->lifetime_shared.size &&
	    ptr >= (void *)vr->lifetime_shared.base &&
	    ptr < (void *)(vr->lifetime_shared.base + vr->lifetime_shared.size)) {
		lifetime_free(&vr->lifetime_shared, ptr);
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

	if (!alignment)
		alignment = 4; /* default align 4 bytes */

	switch (type) {
	case VREGION_MEM_TYPE_INTERIM:
		return interim_alloc(&vr->interim, size, alignment);
	case VREGION_MEM_TYPE_LIFETIME:
		return lifetime_alloc(&vr->lifetime, size, alignment);
	case VREGION_MEM_TYPE_INTERIM_SHARED:
		return interim_alloc(&vr->interim_shared, size, alignment);
	case VREGION_MEM_TYPE_LIFETIME_SHARED:
		return lifetime_alloc(&vr->lifetime_shared, size,
			MAX(alignment, CONFIG_DCACHE_LINE_SIZE));
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
