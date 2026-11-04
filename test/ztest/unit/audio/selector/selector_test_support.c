// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <rtos/alloc.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * @brief Allocate aligned runtime memory for the standalone test.
 */
void *rmalloc_align(uint32_t flags, size_t bytes, uint32_t alignment)
{
	(void)flags;
	(void)alignment;
	return malloc(bytes);
}

/**
 * @brief Allocate runtime memory for the standalone test.
 */
void *rmalloc(uint32_t flags, size_t bytes)
{
	(void)flags;
	return malloc(bytes);
}

/**
 * @brief Allocate zero-initialized runtime memory for the standalone test.
 */
void *rzalloc(uint32_t flags, size_t bytes)
{
	(void)flags;
	return calloc(bytes, 1);
}

/**
 * @brief Allocate aligned audio-buffer memory for the standalone test.
 */
void *rballoc_align(uint32_t flags, size_t bytes, uint32_t alignment)
{
	(void)flags;
	(void)alignment;
	return malloc(bytes);
}

/**
 * @brief Release standalone runtime memory.
 */
void rfree(void *ptr)
{
	free(ptr);
}

/**
 * @brief Allocate through the test heap abstraction.
 */
void *sof_heap_alloc(struct k_heap *heap, uint32_t flags, size_t bytes, size_t alignment)
{
	(void)heap;
	(void)flags;
	(void)alignment;
	return malloc(bytes);
}

/**
 * @brief Release memory allocated through the test heap abstraction.
 */
void sof_heap_free(struct k_heap *heap, void *addr)
{
	(void)heap;
	free(addr);
}

/**
 * @brief Return the absent user heap for this standalone test.
 */
struct k_heap *sof_sys_user_heap_get(void)
{
	return NULL;
}
