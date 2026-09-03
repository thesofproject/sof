// SPDX-License-Identifier: BSD-3-Clause
/* Copyright(c) 2026 Intel Corporation. All rights reserved. */

#include <zephyr/ztest.h>
#include <sof/common.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void *__wrap_rzalloc(uint32_t flags, size_t bytes)
{
	void *ret;
	(void)flags;

	ret = malloc(bytes);

	zassert_not_null(ret, "Memory allocation should not fail");

	memset(ret, 0, bytes);

	return ret;
}

void __wrap_rfree(void *ptr)
{
	free(ptr);
}

struct k_heap;
void *__wrap_sof_heap_alloc(struct k_heap *heap, uint32_t flags, size_t bytes, size_t alignment)
{
	void *ret;

	(void)flags;
	(void)heap;

	if (alignment)
		ret = aligned_alloc(alignment, ALIGN_UP(bytes, alignment));
	else
		ret = malloc(bytes);

	zassert_not_null(ret, "Memory allocation should not fail");

	return ret;
}

void __wrap_sof_heap_free(struct k_heap *heap, void *ptr)
{
	(void)heap;

	free(ptr);
}

struct k_heap *__wrap_sof_sys_heap_get(void)
{
	return NULL;
}
