// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
//
// Author: Jyri Sarha <jyri.sarha@linux.intel.com>

#include <sof/lib/fast-get.h>
#include <rtos/sof.h>
#include <rtos/alloc.h>
#include <sof/lib/mm_heap.h>
#include <sof/lib/memory.h>
#include <sof/common.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <cmocka.h>
#include <assert.h>

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

static void test_simple_fast_get_put(void **state)
{
	const void *ret;

	(void)state; /* unused */

	ret = fast_get(testdata[0], sizeof(testdata[0]));

	assert(ret);
	assert(!memcmp(ret, testdata[0], sizeof(testdata[0])));

	fast_put(ret);
}

static void test_fast_get_size_missmatch_test(void **state)
{
	const void *ret[2];

	(void)state; /* unused */

	ret[0] = fast_get(testdata[0], sizeof(testdata[0]));

	assert(ret[0]);
	assert(!memcmp(ret[0], testdata[0], sizeof(testdata[0])));

	ret[1] = fast_get(testdata[0], sizeof(testdata[0]) + 1);
	assert(!ret[1]);

	fast_put(ret);
}

static void test_over_32_fast_gets_and_puts(void **state)
{
	const void *copy[ARRAY_SIZE(testdata)];
	int i;

	(void)state; /* unused */

	for (i = 0; i < ARRAY_SIZE(copy); i++)
		copy[i] = fast_get(testdata[i], sizeof(testdata[0]));

	for (i = 0; i < ARRAY_SIZE(copy); i++)
		assert(!memcmp(copy[i], testdata[i], sizeof(testdata[0])));

	for (i = 0; i < ARRAY_SIZE(copy); i++)
		fast_put(copy[i]);
}

static void test_fast_get_refcounting(void **state)
{
	const void *copy[2][ARRAY_SIZE(testdata)];
	int i;
	(void)state; /* unused */

	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		copy[0][i] = fast_get(testdata[i], sizeof(testdata[0]));

	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		copy[1][i] = fast_get(testdata[i], sizeof(testdata[0]));

	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		assert(copy[0][i] == copy[1][i]);

	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		assert(!memcmp(copy[0][i], testdata[i], sizeof(testdata[0])));

	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		fast_put(copy[0][i]);

	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		assert(!memcmp(copy[1][i], testdata[i], sizeof(testdata[0])));

	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		fast_put(copy[1][i]);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_simple_fast_get_put),
		cmocka_unit_test(test_fast_get_size_missmatch_test),
		cmocka_unit_test(test_over_32_fast_gets_and_puts),
		cmocka_unit_test(test_fast_get_refcounting),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}

void *__wrap_rzalloc(uint32_t flags, size_t bytes);
void *__wrap_rmalloc(uint32_t flags, size_t bytes);
void __wrap_rfree(void *ptr);

void *__wrap_rzalloc(uint32_t flags, size_t bytes)
{
	void *ret;
	(void)flags;

	ret = malloc(bytes);

	assert(ret);

	memset(ret, 0, bytes);

	return ret;
}

void *__wrap_rmalloc(uint32_t flags, size_t bytes)
{
	void *ret;
	(void)flags;

	ret = malloc(bytes);

	assert(ret);

	return ret;
}

void __wrap_rfree(void *ptr)
{
	free(ptr);
}
