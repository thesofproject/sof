/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>
 */

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <sof/sof.h>
#include <sof/alloc.h>

static struct sof *sof;

enum test_type {
	TEST_BULK = 0,
	TEST_ZERO,
	TEST_IMMEDIATE_FREE
};

struct test_case {
	size_t alloc_size;
	int alloc_zone;
	uint32_t alloc_caps;
	uint16_t alloc_num;
	enum test_type type;
	const char *name;
};

#define TEST_CASE(bytes, zone, caps, num, type, name_base) \
	{(bytes), (zone), (caps), (num), (type), \
	("test_lib_alloc_" name_base "__" #zone "__" #bytes "x" #num)}

static struct test_case test_cases[] = {
	/*
	 * rmalloc tests
	 */

	TEST_CASE(4,   RZONE_SYS, SOF_MEM_CAPS_RAM, 1024, TEST_IMMEDIATE_FREE,
		  "rmalloc"),

	TEST_CASE(1,   RZONE_SYS, SOF_MEM_CAPS_RAM, 2, TEST_BULK, "rmalloc"),
	TEST_CASE(4,   RZONE_SYS, SOF_MEM_CAPS_RAM, 2, TEST_BULK, "rmalloc"),
	TEST_CASE(256, RZONE_SYS, SOF_MEM_CAPS_RAM, 2, TEST_BULK, "rmalloc"),

	TEST_CASE(1,   RZONE_SYS, SOF_MEM_CAPS_RAM, 4, TEST_BULK, "rmalloc"),
	TEST_CASE(4,   RZONE_SYS, SOF_MEM_CAPS_RAM, 4, TEST_BULK, "rmalloc"),
	TEST_CASE(256, RZONE_SYS, SOF_MEM_CAPS_RAM, 4, TEST_BULK, "rmalloc"),

	TEST_CASE(1,   RZONE_SYS, SOF_MEM_CAPS_RAM, 8, TEST_BULK, "rmalloc"),
	TEST_CASE(4,   RZONE_SYS, SOF_MEM_CAPS_RAM, 8, TEST_BULK, "rmalloc"),
	TEST_CASE(256, RZONE_SYS, SOF_MEM_CAPS_RAM, 8, TEST_BULK, "rmalloc"),

	TEST_CASE(16,  RZONE_SYS, SOF_MEM_CAPS_RAM, 128, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(4,   RZONE_SYS, SOF_MEM_CAPS_RAM, 256, TEST_BULK,
		  "rmalloc"),

	TEST_CASE(1,   RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 2, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(4,   RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 2, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(256, RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 2, TEST_BULK,
		  "rmalloc"),

	TEST_CASE(1,   RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 4, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(4,   RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 4, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(256, RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 4, TEST_BULK,
		  "rmalloc"),

	TEST_CASE(1,   RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 8, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(4,   RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 8, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(256, RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 8, TEST_BULK,
		  "rmalloc"),

	TEST_CASE(16,  RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 128, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(4,   RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 256, TEST_BULK,
		  "rmalloc"),

	TEST_CASE(1,   RZONE_RUNTIME, SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA, 2,
		  TEST_BULK, "rmalloc_dma"),
	TEST_CASE(4,   RZONE_RUNTIME, SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA, 2,
		  TEST_BULK, "rmalloc_dma"),
	TEST_CASE(256, RZONE_RUNTIME, SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA, 2,
		  TEST_BULK, "rmalloc_dma"),

	/*
	 * rzalloc tests
	 */

	TEST_CASE(1,   RZONE_SYS, SOF_MEM_CAPS_RAM, 2, TEST_ZERO, "rzalloc"),
	TEST_CASE(4,   RZONE_SYS, SOF_MEM_CAPS_RAM, 2, TEST_ZERO, "rzalloc"),
	TEST_CASE(256, RZONE_SYS, SOF_MEM_CAPS_RAM, 2, TEST_ZERO, "rzalloc"),

	TEST_CASE(1,   RZONE_SYS, SOF_MEM_CAPS_RAM, 4, TEST_ZERO, "rzalloc"),
	TEST_CASE(4,   RZONE_SYS, SOF_MEM_CAPS_RAM, 4, TEST_ZERO, "rzalloc"),
	TEST_CASE(256, RZONE_SYS, SOF_MEM_CAPS_RAM, 4, TEST_ZERO, "rzalloc"),

	TEST_CASE(1,   RZONE_SYS, SOF_MEM_CAPS_RAM, 8, TEST_ZERO, "rzalloc"),
	TEST_CASE(4,   RZONE_SYS, SOF_MEM_CAPS_RAM, 8, TEST_ZERO, "rzalloc"),
	TEST_CASE(256, RZONE_SYS, SOF_MEM_CAPS_RAM, 8, TEST_ZERO, "rzalloc"),

	TEST_CASE(16,  RZONE_SYS, SOF_MEM_CAPS_RAM, 128, TEST_ZERO, "rzalloc"),
	TEST_CASE(4,   RZONE_SYS, SOF_MEM_CAPS_RAM, 256, TEST_ZERO, "rzalloc"),

	TEST_CASE(1,   RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 2, TEST_ZERO,
		  "rzalloc"),
	TEST_CASE(4,   RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 2, TEST_ZERO,
		  "rzalloc"),
	TEST_CASE(256, RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 2, TEST_ZERO,
		  "rzalloc"),

	TEST_CASE(1,   RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 4, TEST_ZERO,
		  "rzalloc"),
	TEST_CASE(4,   RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 4, TEST_ZERO,
		  "rzalloc"),
	TEST_CASE(256, RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 4, TEST_ZERO,
		  "rzalloc"),

	TEST_CASE(1,   RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 8, TEST_ZERO,
		  "rzalloc"),
	TEST_CASE(4,   RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 8, TEST_ZERO,
		  "rzalloc"),
	TEST_CASE(256, RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 8, TEST_ZERO,
		  "rzalloc"),

	TEST_CASE(16,  RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 128, TEST_ZERO,
		  "rzalloc"),
	TEST_CASE(4,   RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 512, TEST_ZERO,
		  "rzalloc"),

	TEST_CASE(1,   RZONE_RUNTIME, SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA, 2,
		  TEST_ZERO, "rzalloc_dma"),
	TEST_CASE(4,   RZONE_RUNTIME, SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA, 2,
		  TEST_ZERO, "rzalloc_dma"),
	TEST_CASE(256, RZONE_RUNTIME, SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA, 2,
		  TEST_ZERO, "rzalloc_dma"),

	/*
	 * rballoc tests
	 */

	TEST_CASE(4,   RZONE_BUFFER, SOF_MEM_CAPS_RAM, 1024,
		  TEST_IMMEDIATE_FREE, "rballoc"),

	TEST_CASE(1,   RZONE_BUFFER, SOF_MEM_CAPS_RAM, 2, TEST_BULK,
		  "rballoc"),
	TEST_CASE(4,   RZONE_BUFFER, SOF_MEM_CAPS_RAM, 2, TEST_BULK,
		  "rballoc"),
	TEST_CASE(256, RZONE_BUFFER, SOF_MEM_CAPS_RAM, 2, TEST_BULK,
		  "rballoc"),

	TEST_CASE(1,   RZONE_BUFFER, SOF_MEM_CAPS_RAM, 4, TEST_BULK,
		  "rballoc"),
	TEST_CASE(4,   RZONE_BUFFER, SOF_MEM_CAPS_RAM, 4, TEST_BULK,
		  "rballoc"),
	TEST_CASE(256, RZONE_BUFFER, SOF_MEM_CAPS_RAM, 4, TEST_BULK,
		  "rballoc"),

	TEST_CASE(1,   RZONE_BUFFER, SOF_MEM_CAPS_RAM, 8, TEST_BULK,
		  "rballoc"),
	TEST_CASE(4,   RZONE_BUFFER, SOF_MEM_CAPS_RAM, 8, TEST_BULK,
		  "rballoc"),
	TEST_CASE(256, RZONE_BUFFER, SOF_MEM_CAPS_RAM, 8, TEST_BULK,
		  "rballoc"),

	TEST_CASE(16,  RZONE_BUFFER, SOF_MEM_CAPS_RAM, 64, TEST_BULK,
		  "rballoc"),
	TEST_CASE(4,   RZONE_BUFFER, SOF_MEM_CAPS_RAM, 64, TEST_BULK,
		  "rballoc"),

	TEST_CASE(1,   RZONE_BUFFER, SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA, 2,
		  TEST_BULK, "rballoc_dma"),
	TEST_CASE(4,   RZONE_BUFFER, SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA, 2,
		  TEST_BULK, "rballoc_dma"),
	TEST_CASE(256, RZONE_BUFFER, SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA, 2,
		  TEST_BULK, "rballoc_dma")
};

static int setup(void **state)
{
	sof = malloc(sizeof(struct sof));
	init_heap(sof);

	return 0;
}

static int teardown(void **state)
{
	free(sof);

	return 0;
}

static void *alloc(struct test_case *tc)
{
	void *mem;

	if (tc->alloc_zone == RZONE_BUFFER)
		mem = rballoc(tc->alloc_zone, tc->alloc_caps, tc->alloc_size);
	else
		mem = rmalloc(tc->alloc_zone, tc->alloc_caps, tc->alloc_size);

	return mem;
}

static void test_lib_alloc_bulk_free(struct test_case *tc)
{
	void **all_mem = malloc(sizeof(void *) * tc->alloc_num);
	int i;

	for (i = 0; i < tc->alloc_num; ++i) {
		void *mem = alloc(tc);

		assert_non_null(mem);
		all_mem[i] = mem;
	}

	for (i = 0; i < tc->alloc_num; ++i)
		rfree(all_mem[i]);

	rfree(all_mem);
}

static void test_lib_alloc_immediate_free(struct test_case *tc)
{
	int i;

	for (i = 0; i < tc->alloc_num; ++i) {
		void *mem = alloc(tc);

		assert_non_null(mem);
		rfree(mem);
	}
}

static void test_lib_alloc_zero(struct test_case *tc)
{
	void **all_mem = malloc(sizeof(void *) * tc->alloc_num);
	int i;

	for (i = 0; i < tc->alloc_num; ++i) {
		char *mem = rzalloc(tc->alloc_zone, tc->alloc_caps,
				    tc->alloc_size);
		int j;

		assert_non_null(mem);
		all_mem[i] = mem;

		for (j = 0; j < tc->alloc_size; ++j)
			assert_int_equal(mem[j], 0);
	}

	for (i = 0; i < tc->alloc_num; ++i)
		rfree(all_mem[i]);

	rfree(all_mem);
}

static void test_lib_alloc(void **state)
{
	struct test_case *tc = *((struct test_case **)state);

	switch (tc->type) {
	case TEST_BULK:
		test_lib_alloc_bulk_free(tc);
		break;

	case TEST_ZERO:
		test_lib_alloc_zero(tc);
		break;

	case TEST_IMMEDIATE_FREE:
		test_lib_alloc_immediate_free(tc);
		break;
	}
}

int main(void)
{
	struct CMUnitTest tests[ARRAY_SIZE(test_cases)];

	int i;

	for (i = 0; i < ARRAY_SIZE(test_cases);  ++i) {
		struct CMUnitTest *t = &tests[i];

		t->name = test_cases[i].name;
		t->test_func = test_lib_alloc;
		t->initial_state = &test_cases[i];
		t->setup_func = NULL;
		t->teardown_func = NULL;
	}

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup, teardown);
}
