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
#include <sof/alloc.h>
#include <kernel.h>

/* testbench mem alloc definition */

void *rmalloc(int zone, uint32_t caps, size_t bytes)
{
	return k_malloc(bytes);
}

void *rzalloc(int zone, uint32_t caps, size_t bytes)
{
	return k_calloc(bytes, 1);
}

void rfree(void *ptr)
{
	k_free(ptr);
}

void *rballoc(int zone, uint32_t caps, size_t bytes)
{
	return k_malloc(bytes);
}

void heap_trace(struct mm_heap *heap, int size)
{
}

void heap_trace_all(int force)
{
	heap_trace(NULL, 0);
}
