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
#include "testbench/common_test.h"

/* testbench mem alloc definition */

void *_malloc(int zone, uint32_t caps, size_t bytes)
{
	return malloc(bytes);
}

void *_zalloc(int zone, uint32_t caps, size_t bytes)
{
	return calloc(bytes, 1);
}

void rfree(void *ptr)
{
	free(ptr);
}

void *_balloc(int zone, uint32_t caps, size_t bytes,
	      uint32_t alignment)
{
	return malloc(bytes);
}

void *_realloc(void *ptr, int zone, uint32_t caps, size_t bytes)
{
	return realloc(ptr, bytes);
}

void *_brealloc(void *ptr, int zone, uint32_t caps, size_t bytes,
		uint32_t alignment)
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
