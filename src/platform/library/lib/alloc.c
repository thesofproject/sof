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
#include <sof/lib/alloc.h>
#include <sof/lib/mm_heap.h>

/* testbench mem alloc definition */

void *rmalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes)
{
	return malloc(bytes);
}

void *rzalloc(enum mem_zone zone, uint32_t flags, uint32_t caps, size_t bytes)
{
	return calloc(bytes, 1);
}

void rfree(void *ptr)
{
	free(ptr);
}

void *rballoc_align(uint32_t flags, uint32_t caps, size_t bytes,
		    uint32_t alignment)
{
	return calloc(bytes, 1);
}

void *rbrealloc_align(void *ptr, uint32_t flags, uint32_t caps, size_t bytes,
		      size_t old_bytes, uint32_t alignment)
{
	return realloc(ptr, bytes);
}

void heap_trace(struct mm_heap *heap, int size)
{
	malloc_info(0, stdout);
}

void heap_trace_all(int force)
{
	heap_trace(NULL, 0);
}
