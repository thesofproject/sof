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

/*
 * for compatibility with the initial `flags` meaning
 * SOF_MEM_FLAG_ should start at BIT(2)
 * the first two positions are reserved for SOF_BUF_ flags
 */

/** \name Allocation flags
 *  @{
 */

 /** \brief Allocate DMA-able memory. */
#define SOF_MEM_FLAG_DMA		BIT(2)
/** \brief realloc() skips copying the original content. */
#define SOF_MEM_FLAG_NO_COPY		BIT(3)
/** \brief Allocate uncached address. */
#define SOF_MEM_FLAG_COHERENT		BIT(4)
/** \brief Allocate L3 address. */
#define SOF_MEM_FLAG_L3			BIT(5)
/** \brief Allocate Low power memory address. */
#define SOF_MEM_FLAG_LOW_POWER		BIT(6)
/** \brief Allocate kernel memory address. */
#define SOF_MEM_FLAG_KERNEL		BIT(7)
/** \brief Allocate user memory address. */
#define SOF_MEM_FLAG_USER		BIT(8)
/** \brief Allocate shared user memory address. */
#define SOF_MEM_FLAG_USER_SHARED_BUFFER	BIT(9)
/** \brief Use allocation method for large buffers. */
#define SOF_MEM_FLAG_LARGE_BUFFER	BIT(10)

/** @} */

/**
 * Allocates memory block.
 * @param flags Flags, see SOF_MEM_FLAG_....
 * @param bytes Size in bytes.
 * @param alignment	Alignment in bytes.
 * @return Pointer to the allocated memory or NULL if failed.
 *
 */
void *rmalloc_align(uint32_t flags, size_t bytes, uint32_t alignment);

/**
 * Similar to rmalloc_align(), but no alignment can be specified.
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
 * Frees the memory block.
 * @param ptr Pointer to the memory block.
 */
void rfree(void *ptr);

/**
 * Save L3 heap over DSP reset
 */
void l3_heap_save(void);

__syscall void *sof_heap_alloc(struct k_heap *heap, uint32_t flags, size_t bytes,
			      size_t alignment);

void *z_impl_sof_heap_alloc(struct k_heap *heap, uint32_t flags, size_t bytes,
			    size_t alignment);

__syscall void sof_heap_free(struct k_heap *heap, void *addr);

void z_impl_sof_heap_free(struct k_heap *heap, void *addr);

struct k_heap *sof_sys_heap_get(void);

/* TODO: remove - debug only - only needed for linking */
static inline void heap_trace_all(int force) {}

/** @}*/

#if CONFIG_SOF_USERSPACE_USE_SHARED_HEAP
/**
 * Returns the start address of shared memory heap for buffers.
 *
 * @return pointer to shared memory heap start
 */
uintptr_t get_shared_buffer_heap_start(void);

/**
 * Returns the size of shared memory heap for buffers.
 *
 * @return size of shared memory heap start
 */
size_t get_shared_buffer_heap_size(void);

#endif

#include <zephyr/syscalls/alloc.h>

#endif /* __ZEPHYR_RTOS_ALLOC_H__ */
