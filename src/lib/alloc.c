// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <rtos/alloc.h>
#include <sof/lib/mm_heap.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

void *aligned_alloc(size_t alignment, size_t size);
void free_heap(void);

void *rmalloc(uint32_t flags, size_t bytes)
{
	return malloc(bytes);
}

void *rzalloc(uint32_t flags, size_t bytes)
{
	return calloc(bytes, 1);
}

void *rballoc_align(uint32_t flags, size_t bytes,
		    uint32_t alignment)
{
	return aligned_alloc(alignment, bytes);
}

void rfree(void *ptr)
{
	free(ptr);
}

void *rbrealloc_align(void *ptr, uint32_t flags, size_t bytes,
		      size_t old_bytes, uint32_t alignment)
{
	void *newptr = aligned_alloc(alignment, bytes);

	if (!newptr)
		return NULL;
	memcpy(newptr, ptr, bytes > old_bytes ? old_bytes : bytes);
	free(ptr);
	return newptr;
}

/* TODO: all mm_pm_...() routines to be implemented for IMR storage */
uint32_t mm_pm_context_size(void)
{
	return 0;
}

void free_heap(void)
{
}

/* initialise map */
void init_heap(struct sof *sof)
{
}

#if CONFIG_DEBUG_MEMORY_USAGE_SCAN
int heap_info(int index, struct mm_info *out)
{
	return 0;
}
#endif
