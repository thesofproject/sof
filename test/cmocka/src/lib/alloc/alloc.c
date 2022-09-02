// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <sof/sof.h>
#include <rtos/alloc.h>
#include <sof/lib/mm_heap.h>
#include <sof/lib/memory.h>
#include <ipc/header.h>
#include <ipc/topology.h>

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

	TEST_CASE(1,   SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 2, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(4,   SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 2, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(256, SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 2, TEST_BULK,
		  "rmalloc"),

	TEST_CASE(1,   SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 4, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(4,   SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 4, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(256, SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 4, TEST_BULK,
		  "rmalloc"),

	TEST_CASE(1,   SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 8, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(4,   SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 8, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(256, SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 8, TEST_BULK,
		  "rmalloc"),

	TEST_CASE(16,  SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 128, TEST_BULK,
		  "rmalloc"),

	/*
	 * TODO: Due to recent changes in relation to multicore support
	 * the RZONE_BUFFER and RZONE_RUNTIME tests will not work.
	 */

	TEST_CASE(1,   SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 2, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(4,   SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 2, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(256, SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 2, TEST_BULK,
		  "rmalloc"),

	TEST_CASE(1,   SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 4, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(4,   SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 4, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(256, SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 4, TEST_BULK,
		  "rmalloc"),

	TEST_CASE(1,   SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 8, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(4,   SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 8, TEST_BULK,
		  "rmalloc"),
	TEST_CASE(256, SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 8, TEST_BULK,
		  "rmalloc"),

	TEST_CASE(16,  SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 128, TEST_BULK,
		  "rmalloc"),

	TEST_CASE(1,   SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM |
		  SOF_MEM_CAPS_DMA, 2, TEST_BULK, "rmalloc_dma"),
	TEST_CASE(4,   SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM |
		  SOF_MEM_CAPS_DMA, 2, TEST_BULK, "rmalloc_dma"),
	TEST_CASE(256, SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM |
		  SOF_MEM_CAPS_DMA, 2, TEST_BULK, "rmalloc_dma"),

	/*
	 * rzalloc tests
	 */

	TEST_CASE(1,   SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 2, TEST_ZERO,
		  "rzalloc"),
	TEST_CASE(4,   SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 2, TEST_ZERO,
		  "rzalloc"),
	TEST_CASE(256, SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 2, TEST_ZERO,
		  "rzalloc"),

	TEST_CASE(1,   SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 4, TEST_ZERO,
		  "rzalloc"),
	TEST_CASE(4,   SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 4, TEST_ZERO,
		  "rzalloc"),
	TEST_CASE(256, SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 4, TEST_ZERO,
		  "rzalloc"),

	TEST_CASE(1,   SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 8, TEST_ZERO,
		  "rzalloc"),
	TEST_CASE(4,   SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 8, TEST_ZERO,
		  "rzalloc"),
	TEST_CASE(256, SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 8, TEST_ZERO,
		  "rzalloc"),

	TEST_CASE(16,  SOF_MEM_ZONE_SYS, SOF_MEM_CAPS_RAM, 128, TEST_ZERO,
		  "rzalloc"),

	TEST_CASE(1,   SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 2, TEST_ZERO,
		  "rzalloc"),
	TEST_CASE(4,   SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 2, TEST_ZERO,
		  "rzalloc"),
	TEST_CASE(256, SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 2, TEST_ZERO,
		  "rzalloc"),

	TEST_CASE(1,   SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 4, TEST_ZERO,
		  "rzalloc"),
	TEST_CASE(4,   SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 4, TEST_ZERO,
		  "rzalloc"),
	TEST_CASE(256, SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 4, TEST_ZERO,
		  "rzalloc"),

	TEST_CASE(1,   SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 8, TEST_ZERO,
		  "rzalloc"),
	TEST_CASE(4,   SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 8, TEST_ZERO,
		  "rzalloc"),
	TEST_CASE(256, SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 8, TEST_ZERO,
		  "rzalloc"),

	TEST_CASE(16,  SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM, 128, TEST_ZERO,
		  "rzalloc"),

	TEST_CASE(1,   SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM |
		  SOF_MEM_CAPS_DMA, 2, TEST_ZERO, "rzalloc_dma"),
	TEST_CASE(4,   SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM |
		  SOF_MEM_CAPS_DMA, 2, TEST_ZERO, "rzalloc_dma"),
	TEST_CASE(256, SOF_MEM_ZONE_RUNTIME, SOF_MEM_CAPS_RAM |
		  SOF_MEM_CAPS_DMA, 2, TEST_ZERO, "rzalloc_dma"),

	/*
	 * rballoc tests
	 */

	TEST_CASE(4,   SOF_MEM_ZONE_BUFFER, SOF_MEM_CAPS_RAM, 1024,
		  TEST_IMMEDIATE_FREE, "rballoc"),

	TEST_CASE(1,   SOF_MEM_ZONE_BUFFER, SOF_MEM_CAPS_RAM, 2, TEST_BULK,
		  "rballoc"),
	TEST_CASE(4,   SOF_MEM_ZONE_BUFFER, SOF_MEM_CAPS_RAM, 2, TEST_BULK,
		  "rballoc"),
	TEST_CASE(256, SOF_MEM_ZONE_BUFFER, SOF_MEM_CAPS_RAM, 2, TEST_BULK,
		  "rballoc"),

	TEST_CASE(1,   SOF_MEM_ZONE_BUFFER, SOF_MEM_CAPS_RAM, 4, TEST_BULK,
		  "rballoc"),
	TEST_CASE(4,   SOF_MEM_ZONE_BUFFER, SOF_MEM_CAPS_RAM, 4, TEST_BULK,
		  "rballoc"),
	TEST_CASE(256, SOF_MEM_ZONE_BUFFER, SOF_MEM_CAPS_RAM, 4, TEST_BULK,
		  "rballoc"),

	TEST_CASE(1,   SOF_MEM_ZONE_BUFFER, SOF_MEM_CAPS_RAM, 8, TEST_BULK,
		  "rballoc"),
	TEST_CASE(4,   SOF_MEM_ZONE_BUFFER, SOF_MEM_CAPS_RAM, 8, TEST_BULK,
		  "rballoc"),
	TEST_CASE(256, SOF_MEM_ZONE_BUFFER, SOF_MEM_CAPS_RAM, 8, TEST_BULK,
		  "rballoc"),

	TEST_CASE(16,  SOF_MEM_ZONE_BUFFER, SOF_MEM_CAPS_RAM, 64, TEST_BULK,
		  "rballoc"),
	TEST_CASE(4,   SOF_MEM_ZONE_BUFFER, SOF_MEM_CAPS_RAM, 64, TEST_BULK,
		  "rballoc"),

	TEST_CASE(1,   SOF_MEM_ZONE_BUFFER, SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA,
		  2, TEST_BULK, "rballoc_dma"),
	TEST_CASE(4,   SOF_MEM_ZONE_BUFFER, SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA,
		  2, TEST_BULK, "rballoc_dma"),
	TEST_CASE(256, SOF_MEM_ZONE_BUFFER, SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA,
		  2, TEST_BULK, "rballoc_dma"),
};

static int setup(void **state)
{
	platform_init_memmap(sof_get());
	init_heap(sof_get());

	return 0;
}

static int teardown(void **state)
{
	return 0;
}

static int clear_sys(void **state)
{
	struct mm *memmap = memmap_get();
	int sysheap_idx = 0;

	for (; sysheap_idx < ARRAY_SIZE(memmap->system); ++sysheap_idx) {
		struct mm_heap *cpu_heap = &memmap->system[sysheap_idx];

		cpu_heap->info.used = 0;
		cpu_heap->info.free = cpu_heap->size;
	}

	return 0;
}

static void *alloc(struct test_case *tc)
{
	void *mem;

	if (tc->alloc_zone == SOF_MEM_ZONE_BUFFER)
		mem = rballoc(0, tc->alloc_caps, tc->alloc_size);
	else
		mem = rmalloc(tc->alloc_zone, 0, tc->alloc_caps,
			      tc->alloc_size);

	return mem;
}

static void alloc_free(void **mem, struct test_case *tc)
{
	int i;

	if (tc->alloc_zone != SOF_MEM_ZONE_SYS) {
		for (i = 0; i < tc->alloc_num; ++i)
			rfree(mem[i]);
	}
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

	alloc_free(all_mem, tc);

	free(all_mem);
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
		char *mem = rzalloc(tc->alloc_zone, 0, tc->alloc_caps,
				    tc->alloc_size);
		int j;

		assert_non_null(mem);
		all_mem[i] = mem;

		for (j = 0; j < tc->alloc_size; ++j)
			assert_int_equal(mem[j], 0);
	}

	alloc_free(all_mem, tc);

	free(all_mem);
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
		t->setup_func = clear_sys;
		t->teardown_func = NULL;
	}

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup, teardown);
}
