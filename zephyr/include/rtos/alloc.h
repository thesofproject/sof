/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

/**
 * @file rtos/alloc.h
 * @brief Memory allocation API, zones, heaps, and alignment utilities.
 * @ingroup memory_api
 */

#ifndef __ZEPHYR_RTOS_ALLOC_H__
#define __ZEPHYR_RTOS_ALLOC_H__

#include <rtos/bit.h>
#include <rtos/string.h>
#include <rtos/cache.h> /* PLATFORM_DCACHE_ALIGN */
#include <sof/trace/trace.h>
#include <user/trace.h>

#include <stddef.h>
#include <stdint.h>

/** \addtogroup memory_api
 *  @{
 */

/*
 * for compatibility with the initial `flags` meaning
 * SOF_MEM_FLAG_ should start at BIT(2)
 * the first two positions are reserved for SOF_BUF_ flags
 */

/** \name Allocation flags
 *  @{
 */

/** \brief Allocate memory accessible by DMA controllers (e.g. Host or DSP DMA). */
#define SOF_MEM_FLAG_DMA		BIT(2)
/**
 * \brief realloc() skips copying the original content.
 * @deprecated Legacy realloc optimization flag. rbrealloc() and rbrealloc_align()
 *             have been removed from the allocation API.
 */
#define SOF_MEM_FLAG_NO_COPY		BIT(3)
/**
 * \brief Allocate uncached / coherent memory address.
 *
 * Directs the allocator to return an uncached pointer (e.g. bypassing cache alias
 * or mapping uncached). Mandatory for DMA descriptors, mailboxes, and cross-core
 * shared variables. Not supported for L3 memory.
 */
#define SOF_MEM_FLAG_COHERENT		BIT(4)
/**
 * \brief Allocate from L3 address space (IMR / external DRAM heap).
 *
 * Allocates from the L3 heap (l3_heap). Only cached allocations are supported;
 * requesting coherent L3 memory will fail and return NULL.
 */
#define SOF_MEM_FLAG_L3			BIT(5)
/**
 * \brief Allocate Low power memory address.
 * @deprecated Legacy flag from XTOS memory partitions; no-op in modern Zephyr heap manager.
 */
#define SOF_MEM_FLAG_LOW_POWER		BIT(6)
/** \brief Base allocation flag denoting memory allocated for kernel-space data structures. */
#define SOF_MEM_FLAG_KERNEL		BIT(7)
/** \brief Base allocation flag denoting memory allocated for user-space threads and module execution. */
#define SOF_MEM_FLAG_USER		BIT(8)
/** \brief Allocate from shared user memory heap (shared_buffer_heap) accessible across user/kernel domains. */
#define SOF_MEM_FLAG_USER_SHARED_BUFFER	BIT(9)
/** \brief Route allocation to the Virtual Memory Heap (VMH / virtual_buffers_heap) for large stream buffers. */
#define SOF_MEM_FLAG_LARGE_BUFFER	BIT(10)

/** @} */

/**
 * @brief Allocate memory block with explicit alignment and heap selection flags.
 *
 * Core RTOS memory allocator. Evaluates @p flags to select the appropriate heap:
 * - #SOF_MEM_FLAG_L3: Allocates from the L3 IMR / external DRAM heap (cached only).
 * - #SOF_MEM_FLAG_USER_SHARED_BUFFER: Allocates from shared_buffer_heap.
 * - Default: Allocates from system heap (sof_heap).
 *
 * If #SOF_MEM_FLAG_COHERENT is specified, returns an uncached (coherent) pointer.
 * Otherwise, returns a cached pointer. When CONFIG_VIRTUAL_HEAP is enabled and
 * system heap allocation fails, transparently attempts fallback allocation from
 * the Virtual Memory Heap (VMH).
 *
 * @param[in] flags Memory allocation flags, see SOF_MEM_FLAG_...
 * @param[in] bytes Size in bytes to allocate.
 * @param[in] alignment Required alignment in bytes (0 for default/natural alignment).
 * @return Pointer to the allocated memory block, or NULL on allocation failure.
 */
void *rmalloc_align(uint32_t flags, size_t bytes, uint32_t alignment);

/**
 * @brief Allocate memory block with default natural alignment.
 *
 * Convenience wrapper calling rmalloc_align() with alignment set to 0.
 *
 * @param[in] flags Memory allocation flags, see SOF_MEM_FLAG_...
 * @param[in] bytes Size in bytes to allocate.
 * @return Pointer to the allocated memory block, or NULL on allocation failure.
 */
void *rmalloc(uint32_t flags, size_t bytes);

/**
 * @brief Allocate zero-initialized memory block with default natural alignment.
 *
 * Allocates memory via rmalloc() and, if successful, zeroes the entire block
 * with memset() before returning.
 *
 * @param[in] flags Memory allocation flags, see SOF_MEM_FLAG_...
 * @param[in] bytes Size in bytes to allocate.
 * @return Pointer to zero-initialized memory block, or NULL on allocation failure.
 */
void *rzalloc(uint32_t flags, size_t bytes);

/**
 * @brief Allocate runtime stream buffer with explicit alignment.
 *
 * Runtime buffer allocator designed for audio stream buffers. Unlike rmalloc_align(),
 * rballoc_align() actively prioritizes the Virtual Memory Heap (VMH / virtual_buffers_heap)
 * when CONFIG_VIRTUAL_HEAP is enabled to preserve internal SRAM. Supports
 * #SOF_MEM_FLAG_L3 and #SOF_MEM_FLAG_USER_SHARED_BUFFER. Respects
 * #SOF_MEM_FLAG_COHERENT for system and shared heaps, but note that the L3 heap
 * path returns cached memory directly and does not support coherent uncached views.
 *
 * @param[in] flags Memory allocation flags, see SOF_MEM_FLAG_...
 * @param[in] bytes Size in bytes to allocate.
 * @param[in] alignment Required alignment in bytes.
 * @return Pointer to allocated buffer, or NULL on allocation failure.
 */
void *rballoc_align(uint32_t flags, size_t bytes,
		    uint32_t alignment);

/**
 * @brief Allocate runtime stream buffer aligned to platform data cache line size.
 *
 * Helper calling rballoc_align() with alignment set to PLATFORM_DCACHE_ALIGN.
 *
 * @param[in] flags Memory allocation flags, see SOF_MEM_FLAG_...
 * @param[in] bytes Size in bytes to allocate.
 * @return Pointer to allocated buffer aligned to cache line, or NULL on failure.
 */
static inline void *rballoc(uint32_t flags, size_t bytes)
{
	return rballoc_align(flags, bytes, PLATFORM_DCACHE_ALIGN);
}

/**
 * @brief Free memory block allocated by SOF RTOS allocators.
 *
 * Unified memory deallocator. Automatically determines the origin heap
 * (L3 heap, Virtual Memory Heap, shared buffer heap, or system heap) using
 * address boundary checks and returns memory to the respective pool.
 * Passing a NULL pointer is safely treated as a no-op.
 *
 * @param[in,out] ptr Pointer to the memory block to free (can be NULL).
 */
void rfree(void *ptr);

/**
 * @brief Save and flush L3 heap state and metadata across DSP reset or power gating.
 *
 * Flushes dirty cache lines for the L3 heap region and copies metadata into
 * preserved storage to ensure data persistence across low-power reset cycles.
 */
void l3_heap_save(void);

/**
 * @brief Allocate memory from a specified Zephyr heap or default system heap.
 * @param[in,out] heap Target Zephyr k_heap (or NULL to let implementation choose).
 * @param[in] flags Memory allocation flags (SOF_MEM_FLAG_...).
 * @param[in] bytes Size in bytes to allocate.
 * @param[in] alignment Required memory alignment in bytes.
 * @return Pointer to allocated buffer, or NULL on allocation failure.
 */
#ifdef CONFIG_SOF_USERSPACE_INTERFACE_ALLOC
__syscall void *sof_heap_alloc(struct k_heap *heap, uint32_t flags, size_t bytes,
			       size_t alignment);
/**
 * @brief Free memory allocated via sof_heap_alloc().
 * @param[in,out] heap Target Zephyr k_heap.
 * @param[in,out] addr Address of allocated buffer to free.
 */
__syscall void sof_heap_free(struct k_heap *heap, void *addr);
#include <zephyr/syscalls/alloc.h>
#else
void *sof_heap_alloc(struct k_heap *heap, uint32_t flags, size_t bytes,
		     size_t alignment);
void sof_heap_free(struct k_heap *heap, void *addr);
#endif

/**
 * @brief Get the system kernel heap descriptor.
 * @return Pointer to the global system k_heap structure (sof_heap), or NULL in unit test mode.
 */
#if CONFIG_SOF_FULL_ZEPHYR_APPLICATION
struct k_heap *sof_sys_heap_get(void);
#else
/* for unit-testing */
static inline struct k_heap *sof_sys_heap_get(void)
{
	return NULL;
}
#endif

/**
 * @brief Get the heap object to use for audio application allocations.
 *
 * Returns zephyr_ll_user_heap() when CONFIG_SOF_USERSPACE_LL is enabled,
 * or NULL to allow sof_heap_alloc() to pick the default heap.
 *
 * @note Audio processing modules should use mod_alloc() instead.
 *
 * @return Pointer to application k_heap, or NULL for default heap selection.
 */
struct k_heap *sof_sys_user_heap_get(void);

/**
 * @brief Trace and log memory allocation statistics across all active heaps.
 * @deprecated No-op stub. Heap diagnostics are tracked via Zephyr runtime
 *             statistics (CONFIG_SYS_HEAP_RUNTIME_STATS), IPC4 telemetry,
 *             and VMH diagnostics (vmh_log_stats()).
 * @param[in] force Force dumping heap trace even if logging threshold is not met.
 */
static inline void heap_trace_all(int force) {}

#if CONFIG_SOF_USERSPACE_USE_SHARED_HEAP
/**
 * @brief Returns the start address of shared memory heap for buffers.
 * @return Pointer address to shared memory heap start.
 */
uintptr_t get_shared_buffer_heap_start(void);

/**
 * @brief Returns the total size of shared memory heap for buffers.
 * @return Total size of shared memory heap in bytes.
 */
size_t get_shared_buffer_heap_size(void);

#endif

#include <sof/lib/vregion.h>

/**
 * @brief Module and pipeline allocation context aggregating a Zephyr heap and optional virtual region.
 */
struct mod_alloc_ctx {
	struct k_heap *heap;    /**< Underlying Zephyr heap descriptor. */
	struct vregion *vreg;   /**< Optional virtual region for interim/lifetime partitioning. */
};

/**
 * @brief Allocate memory from an allocation context.
 *
 * When the context has an associated vregion, allocates from the vregion interim
 * partition using vregion_alloc_coherent_align() (if #SOF_MEM_FLAG_COHERENT is set)
 * or vregion_alloc_align(). When no vregion is attached, falls back to
 * sof_heap_alloc().
 *
 * @param[in,out] ctx Allocation context (heap + optional vregion).
 * @param[in] flags Allocation flags (SOF_MEM_FLAG_*).
 * @param[in] size Size in bytes to allocate.
 * @param[in] alignment Required alignment in bytes.
 * @return Pointer to allocated memory, or NULL on failure.
 */
static inline void *sof_ctx_alloc(struct mod_alloc_ctx *ctx, uint32_t flags,
				  size_t size, size_t alignment)
{
	if (!ctx || !ctx->vreg)
		return sof_heap_alloc(ctx ? ctx->heap : NULL, flags, size, alignment);

	if (flags & SOF_MEM_FLAG_COHERENT)
		return vregion_alloc_coherent_align(ctx->vreg, size, alignment);

	return vregion_alloc_align(ctx->vreg, size, alignment);
}

/**
 * @brief Allocate zero-initialized memory from an allocation context.
 *
 * Allocates memory via sof_ctx_alloc() and clears it with memset() if successful.
 *
 * @param[in,out] ctx Allocation context.
 * @param[in] flags Allocation flags (SOF_MEM_FLAG_*).
 * @param[in] size Size in bytes to allocate.
 * @param[in] alignment Required alignment in bytes.
 * @return Pointer to zero-initialized allocated memory, or NULL on failure.
 */
static inline void *sof_ctx_zalloc(struct mod_alloc_ctx *ctx, uint32_t flags,
				   size_t size, size_t alignment)
{
	void *ptr = sof_ctx_alloc(ctx, flags, size, alignment);

	if (ptr)
		memset(ptr, 0, size);

	return ptr;
}

/**
 * @brief Free memory allocated from an allocation context.
 *
 * If the context contains a vregion, frees via vregion_free(); otherwise delegates
 * to sof_heap_free(). Passing a NULL pointer is safely treated as a no-op.
 *
 * @param[in,out] ctx Allocation context.
 * @param[in,out] ptr Pointer to memory to free (can be NULL).
 */
static inline void sof_ctx_free(struct mod_alloc_ctx *ctx, void *ptr)
{
	if (!ptr)
		return;

	if (ctx && ctx->vreg)
		vregion_free(ctx->vreg, ptr);
	else
		sof_heap_free(ctx ? ctx->heap : NULL, ptr);
}

/** @}*/

#endif /* __ZEPHYR_RTOS_ALLOC_H__ */
