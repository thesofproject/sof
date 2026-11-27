// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <rtos/alloc.h>

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#undef malloc
#undef calloc
#undef realloc
#undef free
#include <stdlib.h>

void *rmalloc_align(uint32_t flags, size_t bytes, uint32_t alignment)
{
	(void)flags;
	(void)alignment;
	return malloc(bytes);
}

void *rmalloc(uint32_t flags, size_t bytes)
{
	(void)flags;
	return malloc(bytes);
}

void *rzalloc(uint32_t flags, size_t bytes)
{
	(void)flags;
	return calloc(bytes, 1);
}

void *rballoc_align(uint32_t flags, size_t bytes, uint32_t alignment)
{
	(void)flags;
	(void)alignment;
	return malloc(bytes);
}

void rfree(void *ptr)
{
	free(ptr);
}

void *sof_heap_alloc(struct k_heap *heap, uint32_t flags, size_t bytes,
		     size_t alignment)
{
	(void)heap;
	(void)flags;
	(void)alignment;
	return malloc(bytes);
}

void sof_heap_free(struct k_heap *heap, void *addr)
{
	(void)heap;
	free(addr);
}

struct k_heap *sof_sys_user_heap_get(void)
{
	return NULL;
}

static int stub_cpu_info(int feature)
{
	(void)feature;
	return 0;
}

int (*WebRtc_GetCPUInfo)(int feature) = stub_cpu_info;

void WebRtcAec_InitAec_SSE2(void)
{
}

void aec_rdft_init_sse2(void)
{
}
