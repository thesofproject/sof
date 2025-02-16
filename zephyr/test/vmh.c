// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Guennadi Liakhovetski <guennadi.liakhovetski@linux.intel.com>
 */

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#include <adsp_memory_regions.h>
#include <sof/boot_test.h>
#include <sof/lib/regions_mm.h>

#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

LOG_MODULE_DECLARE(sof_boot_test, CONFIG_SOF_LOG_LEVEL);

/* Test creating and freeing a virtual memory heap */
static void test_vmh_init_and_free_heap(int memory_region_attribute,
					struct vmh_heap_config *config,
					int core_id,
					bool allocating_continuously,
					bool expect_success)
{
	struct vmh_heap *heap = vmh_init_heap(config, memory_region_attribute,
					      core_id, allocating_continuously);
	if (expect_success)
		zassert_not_null(heap, "Heap initialization expected to succeed but failed");
	else
		zassert_is_null(heap, "Heap initialization expected to fail but succeeded");

	if (heap) {
		int ret = vmh_free_heap(heap);

		zassert_equal(ret, 0, "Failed to free heap");
	}
}

/* Test for vmh_alloc and vmh_free */
static void test_vmh_alloc_free_no_check(struct vmh_heap *heap,
					 uint32_t alloc_size,
					 bool expect_success)
{
	void *ptr = vmh_alloc(heap, alloc_size);

	if (expect_success)
		zassert_not_null(ptr, "Allocation expected to succeed but failed");
	else
		zassert_is_null(ptr, "Allocation expected to fail but succeeded");

	if (ptr) {
		int ret = vmh_free(heap, ptr);

		zassert_equal(ret, 0, "Failed to free allocated memory");
	}
}

/* Fill memory with test pattern */
static void verify_memory_content(void *ptr, uint32_t alloc_size, bool fill)
{
	const uint32_t pattern1 = (uint32_t)ptr ^ 0xDEADBEEF;
	const uint32_t pattern2 = (uint32_t)ptr ^ 0xCAFEBABE;
	const uint32_t pattern3 = (uint32_t)ptr ^ 0xFEEDFACE;

	zassert_true(alloc_size >= 16, "alloc size is below the minimum value.");

	/* Calculate check positions end and middle if applicable */
	uint32_t *start_ptr = (uint32_t *)ptr;
	uint32_t *middle_ptr = UINT_TO_POINTER(ALIGN_DOWN(POINTER_TO_UINT(ptr) +
							  alloc_size / 2 - sizeof(uint32_t),
							  sizeof(uint32_t)));
	uint32_t *end_ptr = UINT_TO_POINTER(ALIGN_DOWN(POINTER_TO_UINT(ptr) + alloc_size -
						       sizeof(uint32_t), sizeof(uint32_t)));

	if (fill) {
		/* Write test pattern to the allocated memory beginning middle and end */
		*start_ptr = pattern1;
		*middle_ptr = pattern2;
		*end_ptr = pattern3;
	} else {
		/* Verify the written test pattern at all points */
		zassert_equal(*start_ptr, pattern1,
			      "Memory content verification failed at the start");
		zassert_equal(*middle_ptr, pattern2,
			      "Memory content verification failed in the middle");
		zassert_equal(*end_ptr, pattern3,
			      "Memory content verification failed at the end");
	}
}

/* Test function for vmh_alloc and vmh_free with memory read/write */
static void test_vmh_alloc_free_check(struct vmh_heap *heap,
				      uint32_t alloc_size,
				      bool expect_success)
{
	void *ptr = vmh_alloc(heap, alloc_size);

	if (expect_success)
		zassert_not_null(ptr, "Allocation expected to succeed but failed");
	else {
		zassert_is_null(ptr, "Allocation expected to fail but succeeded");
		return;
	}

	if (ptr) {
		verify_memory_content(ptr, alloc_size, true);
		verify_memory_content(ptr, alloc_size, false);
	}

	int ret = vmh_free(heap, ptr);

	zassert_equal(ret, 0, "Failed to free allocated memory");
}

/* Test function for multiple allocations on the same heap with read/write */
static void test_vmh_multiple_allocs(struct vmh_heap *heap, int num_allocs,
				     uint32_t min_alloc_size,
				     uint32_t max_alloc_size)
{
	void *ptrs[num_allocs];
	uint32_t sizes[num_allocs];
	uint32_t alloc_size;
	bool success;
	int ret;

	/* Perform multiple allocations */
	for (int i = 0; i < num_allocs; i++) {
		/* Generate a random allocation size between min_alloc_size and max_alloc_size */
		alloc_size = min_alloc_size +
		k_cycle_get_32() % (max_alloc_size - min_alloc_size + 1);

		ptrs[i] = vmh_alloc(heap, alloc_size);
		sizes[i] = alloc_size;

		if (!ptrs[i])
			LOG_INF("Test allocation failed for size: %d", alloc_size);

		zassert_true(ptrs[i] != NULL,
			     "Allocation of size %u expected to succeed but failed",
			     alloc_size);

		if (ptrs[i])
			verify_memory_content(ptrs[i], alloc_size, true);
	}

	/* Verify buffer contents */
	for (int i = 0; i < num_allocs; i++) {
		if (ptrs[i])
			verify_memory_content(ptrs[i], sizes[i], false);
	}

	for (int i = 0; i < num_allocs; i++) {
		if (ptrs[i]) {
			ret = vmh_free(heap, ptrs[i]);
			zassert_equal(ret, 0, "Failed to free allocated memory");
		}
	}
}

/* Test case for multiple allocations */
static void test_vmh_alloc_multiple_times(bool allocating_continuously)
{
	struct vmh_heap *heap = vmh_init_heap(NULL, MEM_REG_ATTR_CORE_HEAP, 0,
					      allocating_continuously);

	zassert_not_null(heap, "Heap initialization failed");

	/* Test multiple allocations with small sizes */
	test_vmh_multiple_allocs(heap, 16, 16, 64);
	test_vmh_multiple_allocs(heap, 64, 16, 64);
	test_vmh_multiple_allocs(heap, 16, 16, 1024);
	test_vmh_multiple_allocs(heap, 64, 16, 1024);
	if (allocating_continuously) {
		test_vmh_multiple_allocs(heap, 16, 1024, 4096);
		test_vmh_multiple_allocs(heap, 16, 4096, 8192);
	}

	/* Clean up the heap after testing */
	int ret = vmh_free_heap(heap);

	zassert_equal(ret, 0, "Failed to free heap after multiple allocations");
}

/* Test case for vmh_alloc and vmh_free */
static void test_vmh_alloc_free(bool allocating_continuously)
{
	struct vmh_heap *heap = vmh_init_heap(NULL, MEM_REG_ATTR_CORE_HEAP, 0,
					      allocating_continuously);

	zassert_not_null(heap, "Heap initialization failed");

	test_vmh_alloc_free_no_check(heap, 512, true);
	test_vmh_alloc_free_no_check(heap, 1024, true);
	test_vmh_alloc_free_no_check(heap, sizeof(int), true);
	test_vmh_alloc_free_no_check(heap, 0, false);

	test_vmh_alloc_free_check(heap, 512, true);
	test_vmh_alloc_free_check(heap, 1024, true);

	int ret = vmh_free_heap(heap);

	zassert_equal(ret, 0, "Failed to free heap");

	/* Could add tests with configs for heaps*/
}

/* Test case for vmh_alloc and vmh_free with and without config */
static void test_heap_creation(void)
{
	test_vmh_init_and_free_heap(MEM_REG_ATTR_CORE_HEAP, NULL, 0, false, true);

	/* Try to setup with pre defined heap config */
	struct vmh_heap_config config = {0};

	config.block_bundles_table[0].block_size = 8;
	config.block_bundles_table[0].number_of_blocks = 1024;

	config.block_bundles_table[1].block_size = 16;
	config.block_bundles_table[1].number_of_blocks = 512;

	config.block_bundles_table[2].block_size = 4096;
	config.block_bundles_table[2].number_of_blocks = 2;

	test_vmh_init_and_free_heap(MEM_REG_ATTR_CORE_HEAP, &config, 0, false, true);
}

/* Test case for alloc/free on configured heap */
static void test_alloc_on_configured_heap(bool allocating_continuously)
{
	/* Try to setup with pre defined heap config */
	struct vmh_heap_config config = {0};

	config.block_bundles_table[0].block_size = 32;
	config.block_bundles_table[0].number_of_blocks = 256;

	/* Create continuous allocation heap for success test */
	struct vmh_heap *heap = vmh_init_heap(&config, MEM_REG_ATTR_CORE_HEAP, 0,
					      allocating_continuously);

	/* Will succeed on continuous and fail with single block alloc */
	test_vmh_alloc_free_check(heap, 512, allocating_continuously);

	int ret = vmh_free_heap(heap);

	zassert_equal(ret, 0, "Failed to free heap");
}

/* Test cases for initializing heaps on all available regions */
static void test_vmh_init_all_heaps(void)
{
	int num_regions = CONFIG_MP_MAX_NUM_CPUS + VIRTUAL_REGION_COUNT;
	int i;
	const struct sys_mm_drv_region *virtual_memory_region =
	sys_mm_drv_query_memory_regions();

	/* Test initializing all types of heaps */
	for (i = 0; i < num_regions; i++) {
		/* Zeroed size symbolizes end of regions table */
		if (!virtual_memory_region[i].size)
			break;

		struct vmh_heap *heap = vmh_init_heap(NULL, virtual_memory_region[i].attr, i, true);

		zassert_not_null(heap, "Heap initialization expected to succeed but failed");

		/* Test if it fails when heap already exists */
		test_vmh_init_and_free_heap(virtual_memory_region[i].attr, NULL, i, true, false);

		if (heap) {
			int ret = vmh_free_heap(heap);

			zassert_equal(ret, 0, "Failed to free heap");
		}
	}
}

/* Test allocation of all available buffers */
static void test_vmh_full_alloc(void)
{
	const struct vmh_heap_config config = { {
		{ 512,	8 },
		{ 1024,	4 },
		{ 4096,	2 },
		{ 8192,	2 }
	} };
	void *ptrs[32] = {0};
	int ret, counter = 0;
	struct vmh_heap *heap = vmh_init_heap(&config, MEM_REG_ATTR_CORE_HEAP, 0, false);

	zassert_not_null(heap, "Failed to init heap");

	/* Allocate all buffers */
	for (int bundle = 0; bundle < MAX_MEMORY_ALLOCATORS_COUNT; bundle++) {
		if (!config.block_bundles_table[bundle].block_size)
			break;

		for (int i = 0; i < config.block_bundles_table[bundle].number_of_blocks &&
		     counter < ARRAY_SIZE(ptrs); i++, counter++) {
			ptrs[counter] = vmh_alloc(heap,
						  config.block_bundles_table[bundle].block_size);
			zassert_not_null(ptrs[counter], "Failed to alloc buffer");

			verify_memory_content(ptrs[counter],
					      config.block_bundles_table[bundle].block_size, true);
		}
	}

	/* Check buffers content */
	counter = 0;
	for (int bundle = 0; bundle < MAX_MEMORY_ALLOCATORS_COUNT; bundle++) {
		if (!config.block_bundles_table[bundle].block_size)
			break;

		for (int i = 0; i < config.block_bundles_table[bundle].number_of_blocks &&
		     counter < ARRAY_SIZE(ptrs); i++, counter++) {
			verify_memory_content(ptrs[counter],
					      config.block_bundles_table[bundle].block_size, false);
		}
	}

	/* Free all buffers */
	ARRAY_FOR_EACH(ptrs, i) {
		if (ptrs[i]) {
			ret = vmh_free(heap, ptrs[i]);
			zassert_equal(ret, 0, "Failed to free buffer");
		}
	}

	ret = vmh_free_heap(heap);
	zassert_equal(ret, 0, "Failed to free heap");
}

ZTEST(sof_boot, virtual_memory_heap)
{
	test_heap_creation();
	test_vmh_init_all_heaps();
	test_alloc_on_configured_heap(true);
	test_alloc_on_configured_heap(false);
	test_vmh_alloc_free(true);
	test_vmh_alloc_free(false);
	test_vmh_alloc_multiple_times(true);
	test_vmh_alloc_multiple_times(false);
	test_vmh_full_alloc();

	TEST_CHECK_RET(true, "virtual_memory_heap");
}
