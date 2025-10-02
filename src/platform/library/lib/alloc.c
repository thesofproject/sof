// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <malloc.h>
#include <rtos/alloc.h>
#include <sof/lib/mm_heap.h>
#include <platform/lib/heap_usage.h>

struct platform_library_heap_usage sof_platform_library_heap_usage = {0};

/* testbench mem alloc definition */

void *rmalloc(uint32_t flags, size_t bytes)
{
	if (sof_platform_library_heap_usage.enable)
		sof_platform_library_heap_usage.rmalloc_size += bytes;

	return malloc(bytes);
}

void *rzalloc(uint32_t flags, size_t bytes)
{
	if (sof_platform_library_heap_usage.enable)
		sof_platform_library_heap_usage.rzalloc_size += bytes;

	return calloc(bytes, 1);
}

void rfree(void *ptr)
{
	free(ptr);
}

void *rballoc_align(uint32_t flags, size_t bytes,
		    uint32_t alignment)
{
	if (sof_platform_library_heap_usage.enable)
		sof_platform_library_heap_usage.rballoc_align_size += bytes;

	return malloc(bytes);
}

void *rbrealloc_align(void *ptr, uint32_t flags, size_t bytes,
		      size_t old_bytes, uint32_t alignment)
{
	if (sof_platform_library_heap_usage.enable && bytes > old_bytes)
		sof_platform_library_heap_usage.rballoc_align_size += bytes;

	return realloc(ptr, bytes);
}

void heap_trace(struct mm_heap *heap, int size)
{
#if MALLOC_DEBUG
	malloc_info(0, stdout);
#endif
}

void heap_trace_all(int force)
{
	heap_trace(NULL, 0);
}
