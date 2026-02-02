/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation.
 */

#ifndef __ZEPHYR_OBJPOOL_H__
#define __ZEPHYR_OBJPOOL_H__

#include <sof/list.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct k_heap;
struct objpool_head {
	struct list_item list;
	struct k_heap *heap;
	uint32_t flags;
};

/**
 * Allocate memory tracked as part of an object pool.
 *
 * @param head Pointer to the object pool head.
 * @param size Size in bytes of memory blocks to allocate.
 * @param flags Memory allocation flags.
 *
 * @return a pointer to the allocated memory on success, NULL on failure.
 *
 * Allocate a memory block of @a size bytes. @a size is used upon the first
 * invocation to allocate memory on the heap, all consequent allocations with
 * the same @a head must use the same @a size value. First allocation with an
 * empty @a head allocates 2 blocks. After both blocks are taken and a third one
 * is requested, the next call allocates 4 blocks, then 8, 16 and 32. After that
 * 32 blocks are allocated every time. Note, that by design allocated blocks are
 * never freed. See more below.
 * TODO: @a flags are currently only used when allocating new object sets.
 * Should add a check that they're consistent with already allocated objects.
 */
void *objpool_alloc(struct objpool_head *head, size_t size, uint32_t flags);

/**
 * Return a block to the object pool
 *
 * @param head Pointer to the object pool head.
 * @param data Pointer to the object to return (can be NULL)
 *
 * @return 0 on success or a negative error code.
 *
 * Return a block to the object pool. Memory is never freed by design, unused
 * blocks are kept in the object pool for future re-use.
 */
int objpool_free(struct objpool_head *head, void *data);

/**
 * Free all of the object pool memory
 *
 * @param head Pointer to the object pool head.
 */
void objpool_prune(struct objpool_head *head);

/* returns true to stop */
typedef bool (*objpool_iterate_cb)(void *data, void *arg);

/**
 * Iterate over object pool entries until stopped
 *
 * @param head Pointer to the object pool head.
 * @param cb Callback function
 * @param arg Callback function argument
 *
 * @return 0 on success or a negative error code.
 *
 * Call the callback function for each entry in the pool, until it returns true.
 * If the callback never returns true, return an error.
 */
int objpool_iterate(struct objpool_head *head, objpool_iterate_cb cb, void *arg);

#endif
