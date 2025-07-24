// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.
//
// These contents may have been developed with support from one or more Intel-operated
// generative artificial intelligence solutions.

#include <zephyr/ztest.h>
#include <sof/lib/fast-get.h>
#include <stdlib.h>

static const int testdata[33][100] = {
	{
	1, 2, 3, 4, 5, 6, 7, 9, 0,
	1, 2, 3, 4, 5, 6, 7, 9, 0,
	1, 2, 3, 4, 5, 6, 7, 9, 0,
	1, 2, 3, 4, 5, 6, 7, 9, 0,
	1, 2, 3, 4, 5, 6, 7, 9, 0,
	1, 2, 3, 4, 5, 6, 7, 9, 0,
	1, 2, 3, 4, 5, 6, 7, 9, 0,
	1, 2, 3, 4, 5, 6, 7, 9, 0,
	1, 2, 3, 4, 5, 6, 7, 9, 0,
	1, 2, 3, 4, 5, 6, 7, 9, 0,
	},
	{ 2 },
	{ 3 },
	{ 4 },
	{ 5 },
	{ 6 },
	{ 7 },
	{ 8 },
	{ 9 },
	{ 10 },
	{ 11 },
	{ 12 },
	{ 13 },
	{ 14 },
	{ 15 },
	{ 16 },
	{ 17 },
	{ 18 },
	{ 19 },
	{ 20 },
	{ 21 },
	{ 23 },
	{ 24 },
	{ 25 },
	{ 26 },
	{ 27 },
	{ 28 },
	{ 29 },
	{ 30 },
	{ 31 },
	{ 32 },
	{ 33 },
};

/* Mock memory allocation functions for testing purposes */

void *__wrap_rzalloc(uint32_t flags, size_t bytes)
{
	void *ret;
	(void)flags;

	ret = malloc(bytes);

	zassert_not_null(ret, "Memory allocation should not fail");

	memset(ret, 0, bytes);

	return ret;
}

void *__wrap_rmalloc(uint32_t flags, size_t bytes)
{
	void *ret;
	(void)flags;

	ret = malloc(bytes);

	zassert_not_null(ret, "Memory allocation should not fail");

	return ret;
}

void __wrap_rfree(void *ptr)
{
	free(ptr);
}

/**
 * @brief Test basic fast_get and fast_put functionality
 *
 * Tests that fast_get can allocate memory for data and fast_put can free it.
 * Verifies that the returned pointer is valid and data is correctly copied.
 */
ZTEST(fast_get_suite, test_simple_fast_get_put)
{
	const void *ret;

	ret = fast_get(testdata[0], sizeof(testdata[0]));

	zassert_not_null(ret, "fast_get should return valid pointer");
	zassert_mem_equal(ret, testdata[0], sizeof(testdata[0]),
			  "Returned data should match original data");

	fast_put(ret);
}

/**
 * @brief Test fast_get size mismatch behavior
 *
 * Tests that fast_get returns NULL when requested size doesn't match
 * previously allocated size for the same data pointer.
 */
ZTEST(fast_get_suite, test_fast_get_size_missmatch_test)
{
	const void *ret[2];

	ret[0] = fast_get(testdata[0], sizeof(testdata[0]));

	zassert_not_null(ret[0], "First fast_get should succeed");
	zassert_mem_equal(ret[0], testdata[0], sizeof(testdata[0]),
			  "Returned data should match original data");

	ret[1] = fast_get(testdata[0], sizeof(testdata[0]) + 1);
	zassert_is_null(ret[1], "fast_get with different size should return NULL");

	fast_put(ret[0]);
}

/**
 * @brief Test multiple fast_get and fast_put operations
 *
 * Tests that fast_get can handle more than 32 allocations and that
 * all data is correctly stored and can be retrieved.
 */
ZTEST(fast_get_suite, test_over_32_fast_gets_and_puts)
{
	const void *copy[ARRAY_SIZE(testdata)];
	int i;

	for (i = 0; i < ARRAY_SIZE(copy); i++)
		copy[i] = fast_get(testdata[i], sizeof(testdata[0]));

	for (i = 0; i < ARRAY_SIZE(copy); i++)
		zassert_mem_equal(copy[i], testdata[i], sizeof(testdata[0]),
				  "Data at index %d should match original", i);

	for (i = 0; i < ARRAY_SIZE(copy); i++)
		fast_put(copy[i]);
}

/**
 * @brief Test fast_get reference counting functionality
 *
 * Tests that fast_get implements proper reference counting - multiple
 * fast_get calls for the same data should return the same pointer,
 * and the data should remain valid until all references are released.
 */
ZTEST(fast_get_suite, test_fast_get_refcounting)
{
	const void *copy[2][ARRAY_SIZE(testdata)];
	int i;

	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		copy[0][i] = fast_get(testdata[i], sizeof(testdata[0]));

	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		copy[1][i] = fast_get(testdata[i], sizeof(testdata[0]));

	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		zassert_equal_ptr(copy[0][i], copy[1][i],
				  "Same data should return same pointer (refcounting)");

	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		zassert_mem_equal(copy[0][i], testdata[i], sizeof(testdata[0]),
				  "Data should match original after multiple fast_get calls");

	/* Release first set of references */
	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		fast_put(copy[0][i]);

	/* Data should still be valid through second set of references */
	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		zassert_mem_equal(copy[1][i], testdata[i], sizeof(testdata[0]),
				  "Data should remain valid after partial fast_put");

	/* Release second set of references */
	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		fast_put(copy[1][i]);
}

/**
 * @brief Define and initialize the fast_get test suite
 */
ZTEST_SUITE(fast_get_suite, NULL, NULL, NULL, NULL, NULL);
