/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/**
 * \file xtos/include/rtos/alloc.h
 * \brief Memory Allocation API definition
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 * \author Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_LIB_ALLOC_H__
#define __SOF_LIB_ALLOC_H__

#include <rtos/bit.h>
#include <rtos/string.h>
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

/*
 * for compatibility with the initial `flags` meaning
 * SOF_MEM_FLAG_ should start at BIT(2)
 * the first two positions are reserved for SOF_BUF_ flags
 */

 /** \brief Indicates we should return DMA-able memory. */
#define SOF_MEM_FLAG_DMA		BIT(2)
/** \brief Indicates that original content should not be copied by realloc. */
#define SOF_MEM_FLAG_NO_COPY		BIT(3)
/** \brief Indicates that if we should return uncached address. */
#define SOF_MEM_FLAG_COHERENT		BIT(4)
/** \brief Indicates that if we should return L3 address. */
#define SOF_MEM_FLAG_L3			BIT(5)
/** \brief Indicates that if we should return Low power memory address. */
#define SOF_MEM_FLAG_LOW_POWER		BIT(6)
/** \brief Indicates that if we should return kernel memory address. */
#define SOF_MEM_FLAG_KERNEL		BIT(7)
/** \brief Indicates that if we should return user memory address. */
#define SOF_MEM_FLAG_USER		BIT(8)
/** \brief Indicates that if we should return shared user memory address. */
#define SOF_MEM_FLAG_USER_SHARED_BUFFER	BIT(9)

/** @} */

/**
 * Allocates memory block.
 * @param flags Flags, see SOF_MEM_FLAG_...
 * @param bytes Size in bytes.
 * @param alignment	Alignment in bytes.
 * @return Pointer to the allocated memory or NULL if failed.
 */
void *rmalloc_align(uint32_t flags, size_t bytes,
		    uint32_t alignment);

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
 * Allocates memory block from the system heap reserved for the specified core.
 * @param core Core id.
 * @param bytes Size in bytes.
 */
void *rzalloc_core_sys(int core, size_t bytes);

/**
 * Calculates length of the null-terminated string.
 * @param s String.
 * @return Length of the string in bytes.
 */
int rstrlen(const char *s);

/**
 * Compares two strings, see man strcmp.
 * @param s1 First string to compare.
 * @param s2 Second string to compare.
 * @return See man strcmp.
 */
int rstrcmp(const char *s1, const char *s2);

static inline void l3_heap_save(void) {}

/** @}*/

#endif /* __SOF_LIB_ALLOC_H__ */
