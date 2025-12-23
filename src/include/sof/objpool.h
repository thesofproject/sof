/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation.
 */

#ifndef __ZEPHYR_OBJPOOL_H__
#define __ZEPHYR_OBJPOOL_H__

struct list_item;
/**
 * Allocate memory tracked as part of an object pool.
 *
 * @param head Pointer to the object pool list head.
 * @param size Size in bytes of memory blocks to allocate.
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
 */
void *objpool_alloc(struct list_item *head, size_t size);

/**
 * Return a block to the object pool
 *
 * @param head Pointer to the object pool list head.
 * @param data Pointer to the object to return (can be NULL)
 *
 * @return 0 on success or a negative error code.
 *
 * Return a block to the object pool. Memory is never freed by design, unused
 * blocks are kept in the object pool for future re-use.
 */
int objpool_free(struct list_item *head, void *data);

#endif
