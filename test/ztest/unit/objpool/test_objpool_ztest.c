// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#define DATA_SIZE 5
#define ALIGNED_SIZE ALIGN_UP(DATA_SIZE, sizeof(int))

#include <zephyr/ztest.h>
#include <sof/objpool.h>
#include <sof/common.h>
#include <sof/list.h>

#include <rtos/alloc.h>

#include <stdlib.h>
#include <string.h>

void *__wrap_rzalloc(uint32_t flags, size_t bytes)
{
	(void)flags;

	void *ret = malloc(bytes);

	if (ret)
		memset(ret, 0, bytes);

	return ret;
}

ZTEST(objpool_suite, test_objpool_wrong_size)
{
	struct objpool_head head = {.list = LIST_INIT(head.list)};
	/* new object pool of 2 blocks */
	uint8_t *block1 = objpool_alloc(&head, DATA_SIZE, 0);
	/* should fail because of a different size */
	uint8_t *block2 = objpool_alloc(&head, DATA_SIZE + 1, 0);
	/* second block in the first object pool */
	uint8_t *block3 = objpool_alloc(&head, DATA_SIZE, 0);
	/* new object pool of 4 blocks */
	uint8_t *block4 = objpool_alloc(&head, DATA_SIZE, 0);
	/* should fail because of a different size */
	uint8_t *block5 = objpool_alloc(&head, DATA_SIZE * 2, 0);
	/* should fail because of different flags */
	uint8_t *block6 = objpool_alloc(&head, DATA_SIZE * 2, SOF_MEM_FLAG_COHERENT);

	zassert_not_null(block1);
	zassert_is_null(block2);
	zassert_not_null(block3);
	zassert_not_null(block4);
	zassert_is_null(block5);
	zassert_is_null(block6);

	zassert_not_ok(objpool_free(&head, block1 + 1));
	zassert_ok(objpool_free(&head, block1));
	zassert_not_ok(objpool_free(&head, block3 + 1));
	zassert_ok(objpool_free(&head, block3));
	zassert_not_ok(objpool_free(&head, block4 + 1));
	zassert_ok(objpool_free(&head, block4));
}

ZTEST(objpool_suite, test_objpool)
{
	struct objpool_head head = {.list = LIST_INIT(head.list)};
	void *blocks[62]; /* 2 + 4 + 8 + 16 + 32 */
	unsigned int k = 0;

	/* Loop over all powers: 2^1..2^5 */
	for (unsigned int i = 1; i <= 5; i++) {
		unsigned int n = 1 << i;
		uint8_t *start;

		for (unsigned int j = 0; j < n; j++) {
			uint8_t *block = objpool_alloc(&head, DATA_SIZE, 0);

			zassert_not_null(block, "allocation failed loop %u iter %u", i, j);

			if (!j)
				start = block;
			else
				zassert_equal(block, start + ALIGNED_SIZE * j, "wrong pointer");

			blocks[k++] = block;
		}
	}

	while (k--)
		zassert_ok(objpool_free(&head, blocks[k]), "free failed");
}

ZTEST_SUITE(objpool_suite, NULL, NULL, NULL, NULL, NULL);
