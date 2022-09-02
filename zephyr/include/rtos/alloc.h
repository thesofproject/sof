/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifndef __ZEPHYR_RTOS_ALLOC_H__
#define __ZEPHYR_RTOS_ALLOC_H__

#include <rtos/bit.h>
#include <rtos/string.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

#include <stddef.h>
#include <stdint.h>

/** \addtogroup alloc_api Memory Allocation API
 *  @{
 */

/**
 * \brief Heap Memory Zones
 *
 * The heap has three different zones from where memory can be allocated :-
 *
 * 1) System Zone. Fixed size heap where alloc always succeeds and is never
 * freed. Used by any init code that will never give up the memory.
 *
 * 2) System Runtime Zone. Heap zone intended for runtime objects allocated
 * by the kernel part of the code.
 *
 * 3) Runtime Zone. Main and larger heap zone where allocs are not guaranteed to
 * succeed. Memory can be freed here.
 *
 * 4) Buffer Zone. Largest heap zone intended for audio buffers.
 *
 * 5) Runtime Shared Zone. Similar to Runtime Zone, but content may be used and
 * fred from any enabled core.
 *
 * 6) System Shared Zone. Similar to System Zone, but content may be used from
 * any enabled core.
 *
 * See platform/memory.h for heap size configuration and mappings.
 */
enum mem_zone {
	SOF_MEM_ZONE_SYS = 0,		/**< System zone */
	SOF_MEM_ZONE_SYS_RUNTIME,	/**< System-runtime zone */
	SOF_MEM_ZONE_RUNTIME,		/**< Runtime zone */
	SOF_MEM_ZONE_BUFFER,		/**< Buffer zone */
	SOF_MEM_ZONE_RUNTIME_SHARED,	/**< Runtime shared zone */
	SOF_MEM_ZONE_SYS_SHARED,	/**< System shared zone */
};

/** \name Heap zone flags
 *  @{
 */

/** \brief Indicates that original content should not be copied by realloc. */
#define SOF_MEM_FLAG_NO_COPY	BIT(1)
/** \brief Indicates that if we should return uncached address. */
#define SOF_MEM_FLAG_COHERENT  BIT(2)

/** @} */

/**
 * Allocates memory block.
 * @param zone Zone to allocate memory from, see enum mem_zone.
 * @param flags Flags, see SOF_MEM_FLAG_...
 * @param caps Capabilities, see SOF_MEM_CAPS_...
 * @param bytes Size in bytes.
 * @return Pointer to the allocated memory or NULL if failed.
 *
 * @note Do not use for buffers (SOF_MEM_ZONE_BUFFER zone).
 *       Use rballoc(), rballoc_align() to allocate memory for buffers.
 */
void *rmalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes);

/**
 * Similar to rmalloc(), guarantees that returned block is zeroed.
 *
 * @note Do not use  for buffers (SOF_MEM_ZONE_BUFFER zone).
 *       rballoc(), rballoc_align() to allocate memory for buffers.
 */
void *rzalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes);

/**
 * Allocates memory block from SOF_MEM_ZONE_BUFFER.
 * @param flags Flags, see SOF_MEM_FLAG_...
 * @param caps Capabilities, see SOF_MEM_CAPS_...
 * @param bytes Size in bytes.
 * @param alignment Alignment in bytes.
 * @return Pointer to the allocated memory or NULL if failed.
 */
void *rballoc_align(uint32_t flags, uint32_t caps, size_t bytes,
		    uint32_t alignment);

/**
 * Similar to rballoc_align(), returns buffer aligned to PLATFORM_DCACHE_ALIGN.
 */
static inline void *rballoc(uint32_t flags, uint32_t caps, size_t bytes)
{
	return rballoc_align(flags, caps, bytes, PLATFORM_DCACHE_ALIGN);
}

/**
 * Changes size of the memory block allocated from SOF_MEM_ZONE_BUFFER.
 * @param ptr Address of the block to resize.
 * @param flags Flags, see SOF_MEM_FLAG_...
 * @param caps Capabilities, see SOF_MEM_CAPS_...
 * @param bytes New size in bytes.
 * @param old_bytes Old size in bytes.
 * @param alignment Alignment in bytes.
 * @return Pointer to the resized memory of NULL if failed.
 */
void *rbrealloc_align(void *ptr, uint32_t flags, uint32_t caps, size_t bytes,
		      size_t old_bytes, uint32_t alignment);

/**
 * Similar to rballoc_align(), returns resized buffer aligned to
 * PLATFORM_DCACHE_ALIGN.
 */
static inline void *rbrealloc(void *ptr, uint32_t flags, uint32_t caps,
			      size_t bytes, size_t old_bytes)
{
	return rbrealloc_align(ptr, flags, caps, bytes, old_bytes,
			       PLATFORM_DCACHE_ALIGN);
}

/**
 * Frees the memory block.
 * @param ptr Pointer to the memory block.
 *
 * @note Blocks from SOF_MEM_ZONE_SYS cannot be freed, such a call causes
 *       panic.
 */
void rfree(void *ptr);

/* TODO: remove - debug only - only needed for linking */
static inline void heap_trace_all(int force) {}

/** @}*/

#endif /* __ZEPHYR_RTOS_ALLOC_H__ */
