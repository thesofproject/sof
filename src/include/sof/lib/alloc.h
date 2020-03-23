/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_LIB_ALLOC_H__
#define __SOF_LIB_ALLOC_H__

#include <sof/bit.h>
#include <sof/string.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <config.h>
#include <stddef.h>
#include <stdint.h>

/* Heap Memory Zones
 *
 * The heap has three different zones from where memory can be allocated :-
 *
 * 1) System Zone. Fixed size heap where alloc always succeeds and is never
 * freed. Used by any init code that will never give up the memory.
 *
 * 2) Runtime Zone. Main and larger heap zone where allocs are not guaranteed to
 * succeed. Memory can be freed here.
 *
 * 3) Buffer Zone. Largest heap zone intended for audio buffers.
 *
 * 4) System Runtime Zone. Heap zone intended for runtime objects allocated
 * by the kernel part of the code.
 *
 * See platform/memory.h for heap size configuration and mappings.
 */

/* heap zone types */
enum mem_zone {
	SOF_MEM_ZONE_SYS = 0,
	SOF_MEM_ZONE_SYS_RUNTIME,
	SOF_MEM_ZONE_RUNTIME,
	SOF_MEM_ZONE_BUFFER,
};

/* heap zone flags */
#define SOF_MEM_FLAG_SHARED	BIT(0)

/* heap allocation and free */
void *rmalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes);

void *rzalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes);

void *rballoc_align(uint32_t flags, uint32_t caps, size_t bytes,
		    uint32_t alignment);

static inline void *rballoc(uint32_t flags, uint32_t caps, size_t bytes)
{
	return rballoc_align(flags, caps, bytes, PLATFORM_DCACHE_ALIGN);
}

void *rrealloc(void *ptr, enum mem_zone zone, uint32_t flags, uint32_t caps,
	       size_t bytes);

void *rbrealloc_align(void *ptr, uint32_t flags, uint32_t caps, size_t bytes,
		      uint32_t alignment);

static inline void *rbrealloc(void *ptr, uint32_t flags, uint32_t caps,
			      size_t bytes)
{
	return rbrealloc_align(ptr, flags, caps, bytes, PLATFORM_DCACHE_ALIGN);
}

void rfree(void *ptr);

/* system heap allocation for specific core */
void *rzalloc_core_sys(int core, size_t bytes);

/* utility */
#define bzero(ptr, size) \
	arch_bzero(ptr, size)

int rstrlen(const char *s);
int rstrcmp(const char *s1, const char *s2);

#endif /* __SOF_LIB_ALLOC_H__ */
