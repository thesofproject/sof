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
#include <platform/lib/telemetry_sched.h>

bool __attribute__((weak)) host_platform_account_alloc(const void *ptr, uint32_t flags,
						       size_t bytes);
void __attribute__((weak)) host_platform_account_free(const void *ptr, size_t bytes);

/* testbench mem alloc definition */

static void *tb_tracked_alloc(uint32_t flags, size_t bytes, bool zero_init)
{
	void *ptr;
	size_t actual;

	if (bytes == 0)
		bytes = 1;

	ptr = zero_init ? calloc(bytes, 1) : malloc(bytes);
	if (!ptr)
		return NULL;

	actual = malloc_usable_size(ptr);
	if (actual < bytes)
		actual = bytes;

	if (host_platform_account_alloc && !host_platform_account_alloc(ptr, flags, actual)) {
		free(ptr);
		return NULL;
	}

	return ptr;
}

static void tb_tracked_free(void *ptr)
{
	if (!ptr)
		return;
	if (host_platform_account_free)
		host_platform_account_free(ptr, malloc_usable_size(ptr));
	free(ptr);
}

void *rmalloc_align(uint32_t flags, size_t bytes, uint32_t alignment)
{
	(void)alignment;
	return tb_tracked_alloc(flags, bytes, false);
}

void *rmalloc(uint32_t flags, size_t bytes)
{
	return tb_tracked_alloc(flags, bytes, false);
}

void *rzalloc(uint32_t flags, size_t bytes)
{
	return tb_tracked_alloc(flags, bytes, true);
}

void rfree(void *ptr)
{
	tb_tracked_free(ptr);
}

void *rballoc_align(uint32_t flags, size_t bytes,
		    uint32_t alignment)
{
	(void)alignment;
	return tb_tracked_alloc(flags, bytes, false);
}

void *sof_heap_alloc(struct k_heap *heap, uint32_t flags, size_t bytes,
		     size_t alignment)
{
	(void)heap;
	(void)alignment;
	return tb_tracked_alloc(flags, bytes, false);
}

void sof_heap_free(struct k_heap *heap, void *addr)
{
	(void)heap;
	tb_tracked_free(addr);
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

struct k_heap *sof_sys_heap_get(void)
{
	return NULL;
}

struct k_heap *sof_sys_user_heap_get(void)
{
	return NULL;
}
