/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __INCLUDE_ALLOC__
#define __INCLUDE_ALLOC__

#include <stdlib.h>
#include <sof/string.h>
#include <stdint.h>
#include <sof/bit.h>
#include <sof/platform.h>
#include <platform/platform.h>
#include <arch/spinlock.h>
#include <ipc/topology.h>
struct sof;

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
#define RZONE_SYS		BIT(0)
#define RZONE_RUNTIME		BIT(1)
#define RZONE_BUFFER		BIT(2)
#define RZONE_SYS_RUNTIME	BIT(3)

/* heap zone flags */
#define RZONE_FLAG_UNCACHED	BIT(4)

#define RZONE_TYPE_MASK	0xf
#define RZONE_FLAG_MASK	0xf0

/* heap allocation and free */
void *rmalloc(uint32_t zone, uint32_t caps, size_t bytes);
void *rzalloc(uint32_t zone, uint32_t caps, size_t bytes);
void *rballoc(uint32_t zone, uint32_t caps, size_t bytes);
void rfree(void *ptr);

/* system heap allocation for specific core */
void *rzalloc_core_sys(int core, size_t bytes);

/* utility */
#define bzero(ptr, size) \
	arch_bzero(ptr, size)

int rstrlen(const char *s);
int rstrcmp(const char *s1, const char *s2);

/* status */
static inline void heap_trace_all(int force) {};
static inline void heap_trace(void *heap, int size) {};

void malloc_init(void);

#endif
