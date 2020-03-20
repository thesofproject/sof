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
void *_malloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes);
void *_zalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes);
void *_balloc(uint32_t flags, uint32_t caps, size_t bytes, uint32_t alignment);
void *_realloc(void *ptr, enum mem_zone zone, uint32_t flags, uint32_t caps,
	       size_t bytes);
void *_brealloc(void *ptr, uint32_t flags, uint32_t caps, size_t bytes,
		uint32_t alignment);
void rfree(void *ptr);

#define rmalloc(zone, flags, caps, bytes) \
	_malloc(zone, flags, caps, bytes)
#define rzalloc(zone, flags, caps, bytes) \
	_zalloc(zone, flags, caps, bytes)
#define rballoc(flags, caps, bytes) \
	_balloc(flags, caps, bytes, PLATFORM_DCACHE_ALIGN)
#define rballoc_align(flags, caps, bytes, alignment) \
	_balloc(flags, caps, bytes, alignment)
#define rrealloc(ptr, zone, flags, caps, bytes) \
	_realloc(ptr, zone, flags, caps, bytes)
#define rbrealloc(ptr, flags, caps, bytes) \
	_brealloc(ptr, flags, caps, bytes, PLATFORM_DCACHE_ALIGN)
#define rbrealloc_align(ptr, flags, caps, bytes, alignment) \
	_brealloc(ptr, flags, caps, bytes, alignment)

/* system heap allocation for specific core */
void *rzalloc_core_sys(int core, size_t bytes);

/* utility */
#define bzero(ptr, size) \
	arch_bzero(ptr, size)

int rstrlen(const char *s);
int rstrcmp(const char *s1, const char *s2);

#endif /* __SOF_LIB_ALLOC_H__ */
