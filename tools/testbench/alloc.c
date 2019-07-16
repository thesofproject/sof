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

void *rmalloc(int zone, uint32_t caps, size_t bytes)
{
	return malloc(bytes);
}

void *rzalloc(int zone, uint32_t caps, size_t bytes)
{
	return calloc(bytes, 1);
}

void rfree(void *ptr)
{
	free(ptr);
}

void *rballoc(int zone, uint32_t caps, size_t bytes)
{
	return malloc(bytes);
}

void *rrealloc(void *ptr, int zone, uint32_t caps, size_t bytes)
{
	return realloc(ptr, bytes);
}

void *rbrealloc(void *ptr, int zone, uint32_t caps, size_t bytes)
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
