// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <rtos/alloc.h>
#include <rtos/bit.h>
#include <sof/objpool.h>
#include <sof/common.h>
#include <sof/list.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct objpool {
	struct list_item list;
	unsigned int n;
	uint32_t mask;
	size_t size;
	uint8_t data[];
};

#define OBJPOOL_BITS (sizeof(((struct objpool *)0)->mask) * 8)

static int objpool_add(struct objpool_head *head, unsigned int n, size_t size, uint32_t flags)
{
	if (n > OBJPOOL_BITS)
		return -ENOMEM;

	if (!is_power_of_2(n))
		return -EINVAL;

	size_t aligned_size = n * ALIGN_UP(size, sizeof(int));

	if (!head->heap)
		head->heap = sof_sys_heap_get();

	struct objpool *pobjpool = sof_heap_alloc(head->heap, flags,
						  aligned_size + sizeof(*pobjpool), 0);

	if (!pobjpool)
		return -ENOMEM;

	/* Initialize with 0 to give caller a chance to identify new allocations */
	memset(pobjpool->data, 0, aligned_size);

	pobjpool->n = n;
	/* clear bit means free */
	pobjpool->mask = 0;
	pobjpool->size = size;

	list_item_append(&pobjpool->list, &head->list);

	return 0;
}

void *objpool_alloc(struct objpool_head *head, size_t size, uint32_t flags)
{
	size_t aligned_size = ALIGN_UP(size, sizeof(int));
	struct list_item *list;
	struct objpool *pobjpool;

	/* Make sure size * 32 still fits in OBJPOOL_BITS bits */
	if (!size || aligned_size > (UINT_MAX >> 5) - sizeof(*pobjpool))
		return NULL;

	if (!list_is_empty(&head->list) && head->flags != flags)
		/* List isn't empty, and flags don't match */
		return NULL;

	list_for_item(list, &head->list) {
		pobjpool = container_of(list, struct objpool, list);

		uint32_t free_mask = MASK(pobjpool->n - 1, 0) & ~pobjpool->mask;

		/* sanity check */
		if (size != pobjpool->size)
			return NULL;

		if (!free_mask)
			continue;

		/* Find first free - guaranteed valid now */
		unsigned int bit = ffs(free_mask) - 1;

		pobjpool->mask |= BIT(bit);

		return pobjpool->data + aligned_size * bit;
	}

	/* no free elements found */
	unsigned int new_n;

	if (list_is_empty(&head->list)) {
		head->flags = flags;
		new_n = 2;
	} else {
		/* Check the last one */
		pobjpool = container_of(head->list.prev, struct objpool, list);

		if (pobjpool->n == OBJPOOL_BITS)
			new_n = OBJPOOL_BITS;
		else
			new_n = pobjpool->n << 1;
	}

	if (objpool_add(head, new_n, size, flags) < 0)
		return NULL;

	/* Return the first element of the new objpool, which is now the last one in the list */
	pobjpool = container_of(head->list.prev, struct objpool, list);
	pobjpool->mask = 1;

	return pobjpool->data;
}

int objpool_free(struct objpool_head *head, void *data)
{
	struct list_item *list;
	struct objpool *pobjpool;

	if (!data)
		return 0;

	list_for_item(list, &head->list) {
		pobjpool = container_of(list, struct objpool, list);

		size_t aligned_size = ALIGN_UP(pobjpool->size, sizeof(int));

		if ((uint8_t *)data >= pobjpool->data &&
		    (uint8_t *)data < pobjpool->data + aligned_size * pobjpool->n) {
			unsigned int n = ((uint8_t *)data - pobjpool->data) / aligned_size;

			if ((uint8_t *)data != pobjpool->data + n * aligned_size)
				return -EINVAL;

			pobjpool->mask &= ~BIT(n);

			return 0;
		}
	}

	return -EINVAL;
}

void objpool_prune(struct objpool_head *head)
{
	struct list_item *next, *tmp;

	list_for_item_safe(next, tmp, &head->list) {
		list_item_del(next);
		sof_heap_free(head->heap, container_of(next, struct objpool, list));
	}
}

int objpool_iterate(struct objpool_head *head, objpool_iterate_cb cb, void *arg)
{
	struct list_item *list;

	list_for_item(list, &head->list) {
		struct objpool *pobjpool = container_of(list, struct objpool, list);
		size_t aligned_size = ALIGN_UP(pobjpool->size, sizeof(int));
		uint32_t mask = pobjpool->mask;
		unsigned int bit;

		if (!mask)
			/* all free */
			continue;

		for (; mask; mask &= ~BIT(bit)) {
			bit = ffs(mask) - 1;
			if (cb(pobjpool->data + bit * aligned_size, arg))
				return 0;
		}
	}

	return -ENOENT;
}
