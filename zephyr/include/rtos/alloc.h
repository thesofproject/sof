/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifndef __ZEPHYR_RTOS_ALLOC_H__
#define __ZEPHYR_RTOS_ALLOC_H__

#include <rtos/bit.h>
#include <rtos/string.h>
#include <sof/lib/memory.h> /* PLATFORM_DCACHE_ALIGN */
#include <sof/trace/trace.h>
#include <user/trace.h>

#include <stddef.h>
#include <stdint.h>

/** \addtogroup alloc_api Memory Allocation API
 *  @{
 */

/** \name Heap zone flags
 *  @{
 */

 /** \brief Indicates we should return DMA-able memory. */
#define SOF_MEM_FLAG_DMA	BIT(0)
/** \brief Indicates that original content should not be copied by realloc. */
#define SOF_MEM_FLAG_NO_COPY	BIT(1)
/** \brief Indicates that if we should return uncached address. */
#define SOF_MEM_FLAG_COHERENT	BIT(2)
/** \brief Indicates that if we should return L3 address. */
#define SOF_MEM_FLAG_L3		BIT(3)
/** \brief Indicates that if we should return Low power memory address. */
#define SOF_MEM_FLAG_LOW_POWER	BIT(4)
/** \brief Indicates that if we should return kernel memory address. */
#define SOF_MEM_FLAG_KERNEL	BIT(5)
/** \brief Indicates that if we should return user memory address. */
#define SOF_MEM_FLAG_USER	BIT(6)

/** @} */

/**
 * Allocates memory block.
 * @param flags Flags, see SOF_MEM_FLAG_....
 * @param bytes Size in bytes.
 * @return Pointer to the allocated memory or NULL if failed.
 *
 */
void *rmalloc(uint32_t flags, size_t bytes);

/**
 * Similar to rmalloc(), guarantees that returned block is zeroed.
 */
void *rzalloc(uint32_t flags, size_t bytes);

/**
 * Allocates memory block.
 * @param flags Flags, see SOF_MEM_FLAG_...
 * @param bytes Size in bytes.
 * @param alignment Alignment in bytes.
 * @return Pointer to the allocated memory or NULL if failed.
 */
void *rballoc_align(uint32_t flags, size_t bytes,
		    uint32_t alignment);

/**
 * Similar to rballoc_align(), returns buffer aligned to PLATFORM_DCACHE_ALIGN.
 */
static inline void *rballoc(uint32_t flags, size_t bytes)
{
	return rballoc_align(flags, bytes, PLATFORM_DCACHE_ALIGN);
}

/**
 * Changes size of the memory block allocated.
 * @param ptr Address of the block to resize.
 * @param flags Flags, see SOF_MEM_FLAG_...
 * @param bytes New size in bytes.
 * @param old_bytes Old size in bytes.
 * @param alignment Alignment in bytes.
 * @return Pointer to the resized memory of NULL if failed.
 */
void *rbrealloc_align(void *ptr, uint32_t flags, size_t bytes,
		      size_t old_bytes, uint32_t alignment);

/**
 * Similar to rballoc_align(), returns resized buffer aligned to
 * PLATFORM_DCACHE_ALIGN.
 */
static inline void *rbrealloc(void *ptr, uint32_t flags,
			      size_t bytes, size_t old_bytes)
{
	return rbrealloc_align(ptr, flags, bytes, old_bytes,
			       PLATFORM_DCACHE_ALIGN);
}

/**
 * Frees the memory block.
 * @param ptr Pointer to the memory block.
 */
void rfree(void *ptr);

/**
 * Save L3 heap over DSP reset
 */
void l3_heap_save(void);

void *sof_heap_alloc(struct k_heap *heap, uint32_t flags, size_t bytes,
		     size_t alignment);
void sof_heap_free(struct k_heap *heap, void *addr);

/* TODO: remove - debug only - only needed for linking */
static inline void heap_trace_all(int force) {}

/** @}*/

#endif /* __ZEPHYR_RTOS_ALLOC_H__ */
