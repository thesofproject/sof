// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <rtos/alloc.h>
#include <rtos/bit.h>
#include <sof/batch.h>
#include <sof/common.h>
#include <sof/list.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct batch {
	struct list_item list;
	unsigned int n;
	uint32_t mask;
	size_t size;
	uint8_t data[];
};

static int batch_add(struct list_item *head, unsigned int n, size_t size)
{
	if (n > 32)
		return -ENOMEM;

	if (!is_power_of_2(n))
		return -EINVAL;

	size_t aligned_size = ALIGN_UP(size, sizeof(int));

	/* Initialize with 0 to give caller a chance to identify new allocations */
	struct batch *abatch = rzalloc(0, n * aligned_size + sizeof(*abatch));

	if (!abatch)
		return -ENOMEM;

	abatch->n = n;
	/* clear bit means free */
	abatch->mask = 0;
	abatch->size = size;

	list_item_append(&abatch->list, head);

	return 0;
}

void *batch_alloc(struct list_item *head, size_t size)
{
	size_t aligned_size = ALIGN_UP(size, sizeof(int));
	struct list_item *list;
	struct batch *abatch;

	/* Make sure size * 32 still fits in 32 bits */
	if (!size || aligned_size > (UINT_MAX >> 5) - sizeof(*abatch))
		return NULL;

	list_for_item(list, head) {
		abatch = container_of(list, struct batch, list);

		uint32_t free_mask = MASK(abatch->n - 1, 0) & ~abatch->mask;

		/* sanity check */
		if (size != abatch->size)
			return NULL;

		if (!free_mask)
			continue;

		/* Find first free - guaranteed valid now */
		unsigned int bit = ffs(free_mask) - 1;

		abatch->mask |= BIT(bit);

		return abatch->data + aligned_size * bit;
	}

	/* no free elements found */
	unsigned int new_n;

	if (list_is_empty(head)) {
		new_n = 2;
	} else {
		/* Check the last one */
		abatch = container_of(head->prev, struct batch, list);

		if (abatch->n == 32)
			new_n = 32;
		else
			new_n = abatch->n << 1;
	}

	if (batch_add(head, new_n, size) < 0)
		return NULL;

	/* Return the first element of the new batch, which is now the last one in the list */
	abatch = container_of(head->prev, struct batch, list);
	abatch->mask = 1;

	return abatch->data;
}

int batch_free(struct list_item *head, void *data)
{
	struct list_item *list;
	struct batch *abatch;

	list_for_item(list, head) {
		abatch = container_of(list, struct batch, list);

		size_t aligned_size = ALIGN_UP(abatch->size, sizeof(int));

		if ((uint8_t *)data >= abatch->data &&
		    (uint8_t *)data < abatch->data + aligned_size * abatch->n) {
			unsigned int n = ((uint8_t *)data - abatch->data) / aligned_size;

			if ((uint8_t *)data != abatch->data + n * aligned_size)
				return -EINVAL;

			abatch->mask &= ~BIT(n);

			return 0;
		}
	}

	return -EINVAL;
}
