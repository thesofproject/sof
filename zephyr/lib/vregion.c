// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2025 Intel Corporation.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <sys/types.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <sof/lib/vpage.h>
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
 * 2) Reduced fragmentation and better cache utilization by using a simple linear
 *    allocator for lifetime objects.
 *
 * Note: Software must pass in the size of the region areas at pipeline creation time.
 */

/**
 * @brief virtual region memory structure.
 *
 * This structure represents a virtual memory region, which includes
 * information about the base address, size, and allocation status
 * of the region.
 *
 * Currently the virtual region memory can be partitioned into two areas on
 * page-aligned boundaries:
 *
 * 1. Interim Heap: An interim memory area used for multiple temporary
 *    allocations and frees over the lifetime of the audio processing pipeline.
 *    E.g. for module kcontrol derived allocations.
 *
 * 2. Lifetime Allocator: A simple incrementing allocator used for long-term
 *    static allocations that persist for the lifetime of the audio processing
 *    pipeline. This allocator compresses allocations for better cache
 *    utilization.
 *
 * More types can be added in the future.
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
struct interim_heap {
	struct k_heap heap;
};

/* Main vregion context, see above intro for more details.
 * TODO: Add support to flag which heaps should have their contexts saved and restored.
 */
struct vregion {
	/* region context */
	uint8_t *base;			/* base address of entire region */
	size_t size;			/* size of whole region in bytes */
	unsigned int pages;		/* size of whole region in pages */
	struct k_mutex lock;		/* protect vregion heaps and use-count */
	unsigned int use_count;

	/* current allocation mode */
	enum vregion_mem_type type;	/* LIFETIME at creation, switch to INTERIM */

	/* interim heap - created lazily on first interim allocation */
	struct interim_heap interim;	/* interim heap */

	/* lifetime heap */
	struct vlinear_heap lifetime;	/* lifetime linear heap */
};

/**
 * @brief Create a new virtual region instance.
 *
 * Create a new VIRTUAL REGION instance with specified memory size.
 * Allocations start in LIFETIME mode.
 *
 * @param[in] memsize Total size of the virtual region memory.
 * @return struct vregion* Pointer to the new virtual region instance, or NULL on failure.
 */
struct vregion *vregion_create(size_t memsize)
{
	struct vregion *vr;
	unsigned int pages;
	size_t total_size;
	uint8_t *vregion_base;

	if (!memsize) {
		LOG_ERR("error: invalid vregion memsize %zu", memsize);
		return NULL;
	}

	/*
	 * Align up the memory size to nearest page. Lifetime
	 * allocations growing upward from the base. The interim
	 * k_heap is created lazily from the remaining space when
	 * initialization phase is ready.
	 */
	total_size = ALIGN_UP(memsize, CONFIG_MM_DRV_PAGE_SIZE);

	/* allocate vregion metadata separately to keep it inaccessible to the user */
	vr = rmalloc(0, sizeof(*vr));
	if (!vr)
		return NULL;

	/* allocate pages for vregion */
	pages = total_size / CONFIG_MM_DRV_PAGE_SIZE;
	vregion_base = vpage_alloc(pages);
	if (!vregion_base) {
		rfree(vr);
		return NULL;
	}

	/* init vregion */
	vr->base = vregion_base;
	vr->size = total_size;
	vr->pages = pages;

	/* lifetime linear allocator starts at the beginning of the vregion memory */
	vr->lifetime.base = vregion_base;
	vr->lifetime.size = total_size;
	vr->lifetime.ptr = vregion_base;
	vr->lifetime.used = 0;
	vr->lifetime.free_count = 0;

	/* start in lifetime allocation mode */
	vr->type = VREGION_MEM_TYPE_LIFETIME;

	k_mutex_init(&vr->lock);
	/* The creator is the first user */
	vr->use_count = 1;

	/* log the new vregion */
	LOG_INF("new at base %p size %#zx pages %u metadata at %p",
		(void *)vr->base, total_size, pages, (void *)vr);

	return vr;
}

struct vregion *vregion_get(struct vregion *vr)
{
	if (!vr)
		return NULL;

	k_mutex_lock(&vr->lock, K_FOREVER);
	vr->use_count++;
	k_mutex_unlock(&vr->lock);

	return vr;
}

/**
 * @brief Decrement virtual region's user count or destroy it.
 *
 * @param[in] vr Pointer to the virtual region instance to release.
 * @return struct vregion* Pointer to the virtual region instance or NULL if it has been destroyed.
 */
struct vregion *vregion_put(struct vregion *vr)
{
	unsigned int use_count;

	if (!vr)
		return NULL;

	k_mutex_lock(&vr->lock, K_FOREVER);
	use_count = --vr->use_count;
	k_mutex_unlock(&vr->lock);

	if (use_count)
		return vr;

	/* Last user: nobody else can access the instance. */

	/* log the vregion being destroyed */
	LOG_DBG("destroy %p size %#zx pages %u", (void *)vr->base, vr->size, vr->pages);
	LOG_DBG(" lifetime used %zu free count %d", vr->lifetime.used, vr->lifetime.free_count);
	vpage_free(vr->base);
	rfree(vr);

	return NULL;
}

/**
 * @brief Initialize the interim heap from remaining vregion space.
 *
 * Called on first interim allocation. Creates the k_heap from all buffer
 * space not yet consumed by lifetime allocations.
 *
 * @param[in] vr Pointer to the virtual region instance.
 */
static void interim_heap_init(struct vregion *vr)
{
	uint8_t *interim_base;
	ssize_t interim_size;

	if (vr->type == VREGION_MEM_TYPE_INTERIM) {
		LOG_WRN("vregion %p already in interim mode", (void *)vr);
		return;
	}

	/* interim heap starts right after current lifetime pointer, page-aligned */
	interim_base = UINT_TO_POINTER(ALIGN_UP(POINTER_TO_UINT(vr->lifetime.ptr),
						CONFIG_DCACHE_LINE_SIZE));
	interim_size = (vr->base + vr->size) - interim_base;
	/*
	 * Calling k_heap_init() with too small size causes an assert
	 * failure. Exact limit is hard to deduce from sys_heap_init()
	 * code, but 1024 seems to be enough.
	 */
	if (interim_size < 1024) {
		LOG_WRN("Too little memory, %zd bytes, left for interim heap", interim_size);
		vr->type = VREGION_MEM_TYPE_INVALID;
		return;
	}

	LOG_INF("creating interim heap: lifetime used %zu, interim available %zu at %p",
		vr->lifetime.used, interim_size, (void *)interim_base);

	/* cap lifetime so no more lifetime allocs can grow into interim */
	vr->lifetime.size = interim_base - vr->base;

	vr->interim.heap.heap.init_mem = interim_base;
	vr->interim.heap.heap.init_bytes = interim_size;

	k_heap_init(&vr->interim.heap, interim_base, interim_size);
	vr->type = VREGION_MEM_TYPE_INTERIM;
}

void vregion_set_interim(struct vregion *vr)
{
	if (!vr)
		return;

	k_mutex_lock(&vr->lock, K_FOREVER);

	interim_heap_init(vr);

	k_mutex_unlock(&vr->lock);
}

/**
 * @brief Allocate memory with alignment from the virtual region dynamic heap.
 *
 * @param[in] heap Pointer to the heap to use.
 * @param[in] size Size of the allocation.
 * @param[in] align Alignment of the allocation.
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
static void *interim_alloc(struct interim_heap *heap, size_t size, size_t align)
{
	void *ptr;

	ptr = k_heap_aligned_alloc(&heap->heap, align, size, K_NO_WAIT);
	if (!ptr)
		LOG_WRN("interim alloc failed for %d bytes align %d",
			size, align);

	return ptr;
}

/**
 * @brief Free memory from the virtual region interim heap.
 *
 * @param[in] heap Pointer to the heap to use.
 * @param[in] ptr Pointer to the memory to free.
 */
static void interim_free(struct interim_heap *heap, void *ptr)
{
	k_heap_free(&heap->heap, ptr);
}

/**
 * @brief Allocate memory from the virtual region lifetime allocator.
 *
 * If the interim heap has already been created (i.e., an interim allocation
 * was made), log a warning and fall back to interim allocation.
 *
 * @param[in] heap Pointer to the linear heap to use.
 * @param[in] size Size of the allocation.
 * @param[in] align Alignment of the allocation.
 *
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
static void *lifetime_alloc(struct vlinear_heap *heap, size_t size, size_t align)
{
	void *ptr;
	uint8_t *aligned_ptr;
	size_t heap_obj_size;

	/* align heap pointer to alignment requested */
	aligned_ptr = UINT_TO_POINTER(ALIGN_UP(POINTER_TO_UINT(heap->ptr), align));

	/* also align up size to D$ bytes if asked - allocation head and tail aligned */
	if (align == CONFIG_DCACHE_LINE_SIZE)
		size = ALIGN_UP(size, CONFIG_DCACHE_LINE_SIZE);

	/* calculate new heap object size for object and alignment */
	heap_obj_size = aligned_ptr - heap->ptr + size;

	/* check we have enough lifetime space left */
	if (heap_obj_size + heap->used > heap->size) {
		LOG_WRN("lifetime alloc failed for object %d heap %d bytes free %d",
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

	k_mutex_lock(&vr->lock, K_FOREVER);

	if (sys_cache_is_ptr_uncached(ptr))
		ptr = sys_cache_cached_ptr_get(ptr);

	/* Check if pointer is in interim heap (if initialized) */
	if (vr->type == VREGION_MEM_TYPE_INTERIM &&
	    ptr >= (void *)vr->interim.heap.heap.init_mem &&
	    ptr < (void *)((uint8_t *)vr->interim.heap.heap.init_mem +
			   vr->interim.heap.heap.init_bytes))
		interim_free(&vr->interim, ptr);
	else if (ptr >= (void *)vr->lifetime.base &&
		   ptr < (void *)(vr->lifetime.base + vr->lifetime.size))
		/* pointer is in lifetime area - no-op free */
		lifetime_free(&vr->lifetime, ptr);
	else
		LOG_ERR("error: vregion free invalid pointer %p", ptr);

	k_mutex_unlock(&vr->lock);
}
EXPORT_SYMBOL(vregion_free);

/**
 * @brief Allocate memory from the virtual region.
 *
 * @param[in] vr Pointer to the virtual region instance.
 * @param[in] size Size of the allocation.
 * @param[in] alignment Alignment of the allocation.
 *
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
void *vregion_alloc_align(struct vregion *vr, size_t size, size_t alignment)
{
	void *p;

	if (!vr || !size)
		return NULL;

	if (alignment < PLATFORM_DCACHE_ALIGN)
		alignment = PLATFORM_DCACHE_ALIGN;

	k_mutex_lock(&vr->lock, K_FOREVER);

	switch (vr->type) {
	case VREGION_MEM_TYPE_INTERIM:
		p = interim_alloc(&vr->interim, size, alignment);
		break;
	case VREGION_MEM_TYPE_LIFETIME:
		p = lifetime_alloc(&vr->lifetime, size, alignment);
		break;
	default:
		LOG_ERR("error: invalid memory type %d", vr->type);
		p = NULL;
	}

	k_mutex_unlock(&vr->lock);

	return p;
}
EXPORT_SYMBOL(vregion_alloc_align);

/**
 * @brief Allocate memory from the virtual region.
 * @param[in] vr Pointer to the virtual region instance.
 * @param[in] size Size of the allocation.
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
void *vregion_alloc(struct vregion *vr, size_t size)
{
	return vregion_alloc_align(vr, size, 0);
}
EXPORT_SYMBOL(vregion_alloc);

void *vregion_alloc_coherent(struct vregion *vr, size_t size)
{
	size = ALIGN_UP(size, CONFIG_DCACHE_LINE_SIZE);

	void *p = vregion_alloc_align(vr, size, CONFIG_DCACHE_LINE_SIZE);

	if (!p)
		return NULL;

	sys_cache_data_invd_range(p, size);

	return sys_cache_uncached_ptr_get(p);
}

void *vregion_alloc_coherent_align(struct vregion *vr, size_t size, size_t alignment)
{
	if (alignment < CONFIG_DCACHE_LINE_SIZE)
		alignment = CONFIG_DCACHE_LINE_SIZE;
	size = ALIGN_UP(size, CONFIG_DCACHE_LINE_SIZE);

	void *p = vregion_alloc_align(vr, size, alignment);

	if (!p)
		return NULL;

	sys_cache_data_invd_range(p, size);

	return sys_cache_uncached_ptr_get(p);
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

	LOG_INF("base %p size %#zx pages %u",
		(void *)vr->base, vr->size, vr->pages);
	LOG_INF("lifetime used %#zx free count %d",
		vr->lifetime.used, vr->lifetime.free_count);
}
EXPORT_SYMBOL(vregion_info);

void vregion_mem_info(struct vregion *vr, size_t *size, uintptr_t *start)
{
	if (size)
		*size = vr->size;

	if (start)
		*start = (uintptr_t)vr->base;
}
