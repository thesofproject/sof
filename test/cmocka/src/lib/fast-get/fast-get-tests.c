// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
//
// Author: Jyri Sarha <jyri.sarha@linux.intel.com>

#include "../../util.h"
#include "../../audio/module_adapter.h"

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/format.h>
#include <sof/lib/fast-get.h>
#include <rtos/sof.h>
#include <rtos/alloc.h>
#include <sof/lib/mm_heap.h>
#include <sof/lib/memory.h>
#include <sof/common.h>
#include <volume/volume.h>

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

struct test_data {
	struct comp_dev *dev;
	struct processing_module *mod;
	struct comp_data *cd;
	struct processing_module_test_data *vol_state;
};

struct test_data _td;

static int setup(void **state)
{
	struct test_data *td = &_td;
	struct module_data *md;
	struct vol_data *cd;

	/* allocate new state */
	td->vol_state = test_malloc(sizeof(struct processing_module_test_data));
	td->vol_state->num_sources = 1;
	td->vol_state->num_sinks = 1;
	module_adapter_test_setup(td->vol_state);

	/* allocate and set new data */
	cd = test_malloc(sizeof(*cd));
	md = &td->vol_state->mod->priv;
	md->private = cd;
	cd->is_passthrough = false;
	td->dev = td->vol_state->mod->dev;
	td->mod = td->vol_state->mod;

	/* malloc memory to store current volume 4 times to ensure the address
	 * is 8-byte aligned for multi-way xtensa intrinsic operations.
	 */
	const size_t vol_size = sizeof(int32_t) * SOF_IPC_MAX_CHANNELS * 4;

	cd->vol = test_malloc(vol_size);
	cd->ramp_type = SOF_VOLUME_LINEAR;
	*state = td;

	return 0;
}

static int teardown(void **state)
{
	struct test_data *td = &_td;
	struct vol_data *cd = module_get_private_data(td->vol_state->mod);

	test_free(cd->vol);
	test_free(cd);
	module_adapter_test_free(td->vol_state);
	test_free(td->vol_state);

	return 0;
}

static void test_simple_fast_get_put(void **state)
{
	struct test_data *td = *((struct test_data **)state);
	const void *ret;

	ret = fast_get(td->mod, testdata[0], sizeof(testdata[0]));

	assert(ret);
	assert(!memcmp(ret, testdata[0], sizeof(testdata[0])));

	fast_put(td->mod, ret);
}

static void test_fast_get_size_missmatch_test(void **state)
{
	struct test_data *td = *((struct test_data **)state);
	const void *ret[2];

	ret[0] = fast_get(td->mod, testdata[0], sizeof(testdata[0]));

	assert(ret[0]);
	assert(!memcmp(ret[0], testdata[0], sizeof(testdata[0])));

	/* this test is designed to test size mismatch handling */
	ret[1] = fast_get(td->mod, testdata[0], sizeof(testdata[0]) + 1);
	assert(!ret[1]);
	fast_put(td->mod, ret);
}

static void test_over_32_fast_gets_and_puts(void **state)
{
	struct test_data *td = *((struct test_data **)state);
	const void *copy[ARRAY_SIZE(testdata)];
	int i;

	for (i = 0; i < ARRAY_SIZE(copy); i++)
		copy[i] = fast_get(td->mod, testdata[i], sizeof(testdata[i]));

	for (i = 0; i < ARRAY_SIZE(copy); i++)
		assert(!memcmp(copy[i], testdata[i], sizeof(testdata[i])));

	for (i = 0; i < ARRAY_SIZE(copy); i++)
		fast_put(td->mod, copy[i]);
}

static void test_fast_get_refcounting(void **state)
{
	struct test_data *td = *((struct test_data **)state);
	const void *copy[2][ARRAY_SIZE(testdata)];
	int i;

	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		copy[0][i] = fast_get(td->mod, testdata[i], sizeof(testdata[0]));

	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		copy[1][i] = fast_get(td->mod, testdata[i], sizeof(testdata[0]));

	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		assert(copy[0][i] == copy[1][i]);

	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		assert(!memcmp(copy[0][i], testdata[i], sizeof(testdata[0])));

	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		fast_put(td->mod, copy[0][i]);

	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		assert(!memcmp(copy[1][i], testdata[i], sizeof(testdata[0])));

	for (i = 0; i < ARRAY_SIZE(copy[0]); i++)
		fast_put(td->mod, copy[1][i]);
}

int main(void)
{
	struct CMUnitTest tests[] = {
		cmocka_unit_test(test_simple_fast_get_put),
		cmocka_unit_test(test_fast_get_size_missmatch_test),
		cmocka_unit_test(test_over_32_fast_gets_and_puts),
		cmocka_unit_test(test_fast_get_refcounting),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup, teardown);
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
