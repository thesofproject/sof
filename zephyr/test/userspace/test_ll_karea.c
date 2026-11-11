// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

/*
 * Tests for the kernel-managed areas of the LL memory region: LL user
 * threads can write objects in the RW area, only read objects in the RO
 * area, and the kernel allocator state is not in user memory.
 */

#include <sof/boot_test.h>
#include <sof/schedule/ll_schedule_domain.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

#define USER_STACKSIZE	2048
#define TEST_WORDS	4
#define TEST_PATTERN	0x5ec0de42

static struct k_thread user_thread;
static K_THREAD_STACK_DEFINE(user_stack, USER_STACKSIZE);

/* kernel-only, checked to be outside the areas */
static uint32_t kernel_only_word;

static uint32_t *rw_obj;
static uint32_t *ro_obj;

enum karea_test_case {
	/* write an object in the RW area */
	KAREA_WRITE_RW,
	/* read an object in the RO area */
	KAREA_READ_RO,
	/* write an object in the RO area */
	KAREA_WRITE_RO,
};

static void user_function(void *p1, void *p2, void *p3)
{
	enum karea_test_case test_case = (enum karea_test_case)(uintptr_t)p1;
	uint32_t *obj = p2;
	int i;

	zassert_true(k_is_user_context(), "isn't user");

	switch (test_case) {
	case KAREA_WRITE_RW:
		for (i = 0; i < TEST_WORDS; i++)
			obj[i] = TEST_PATTERN + i;
		return;
	case KAREA_READ_RO:
		for (i = 0; i < TEST_WORDS; i++)
			zassert_equal(obj[i], TEST_PATTERN + i);
		return;
	case KAREA_WRITE_RO:
		obj[0] = 0;
		break;
	default:
		zassert_unreachable("unknown test case");
	}

	zassert_unreachable("write to the RO area did not fault");
}

static void run_case(enum karea_test_case test_case, uint32_t *obj, bool expect_fault)
{
	int ret;

	k_thread_create(&user_thread, user_stack, USER_STACKSIZE,
			user_function, (void *)(uintptr_t)test_case, obj, NULL,
			-1, K_USER, K_FOREVER);
	k_mem_domain_add_thread(zephyr_ll_mem_domain(), &user_thread);

	if (expect_fault)
		sof_boot_test_set_fault_valid(&user_thread);

	k_thread_start(&user_thread);
	ret = k_thread_join(&user_thread, K_FOREVER);

	sof_boot_test_set_fault_valid(NULL);

	zassert_equal(ret, 0, "user thread join failed: %d", ret);
}

static void *ll_karea_setup(void)
{
	/* the areas never free, so allocate the test objects once */
	rw_obj = zephyr_ll_karea_alloc(ZEPHYR_LL_KAREA_RW, 0, TEST_WORDS * sizeof(uint32_t), 0);
	zassert_not_null(rw_obj);
	ro_obj = zephyr_ll_karea_alloc(ZEPHYR_LL_KAREA_RO, 0, TEST_WORDS * sizeof(uint32_t), 0);
	zassert_not_null(ro_obj);

	return NULL;
}

ZTEST(userspace_ll_karea, rw_area_user_writable)
{
	int i;

	memset(rw_obj, 0, TEST_WORDS * sizeof(uint32_t));
	run_case(KAREA_WRITE_RW, rw_obj, false);

	for (i = 0; i < TEST_WORDS; i++)
		zassert_equal(rw_obj[i], TEST_PATTERN + i);
}

ZTEST(userspace_ll_karea, ro_area_user_readable)
{
	int i;

	/* kernel threads can write the RO area */
	for (i = 0; i < TEST_WORDS; i++)
		ro_obj[i] = TEST_PATTERN + i;

	run_case(KAREA_READ_RO, ro_obj, false);
}

ZTEST(userspace_ll_karea, ro_area_user_write_faults)
{
	ro_obj[0] = TEST_PATTERN;
	run_case(KAREA_WRITE_RO, ro_obj, true);
	zassert_equal(ro_obj[0], TEST_PATTERN, "RO object was written");
}

ZTEST(userspace_ll_karea, contains_and_limits)
{
	size_t sz = TEST_WORDS * sizeof(uint32_t);

	zassert_true(zephyr_ll_karea_contains(ZEPHYR_LL_KAREA_RW, rw_obj, sz));
	zassert_true(zephyr_ll_karea_contains(ZEPHYR_LL_KAREA_RO, ro_obj, sz));
	zassert_false(zephyr_ll_karea_contains(ZEPHYR_LL_KAREA_RW, ro_obj, sz));
	zassert_false(zephyr_ll_karea_contains(ZEPHYR_LL_KAREA_RO, rw_obj, sz));
	zassert_false(zephyr_ll_karea_contains(ZEPHYR_LL_KAREA_RW, &kernel_only_word,
					       sizeof(kernel_only_word)));

	/* a request larger than the area fails without using it up */
	zassert_is_null(zephyr_ll_karea_alloc(ZEPHYR_LL_KAREA_RW, 0,
					      CONFIG_SOF_ZEPHYR_LL_KAREA_RW_SIZE + 1, 0));
	zassert_is_null(zephyr_ll_karea_alloc(ZEPHYR_LL_KAREA_RO, 0, SIZE_MAX, 0));
}

ZTEST_SUITE(userspace_ll_karea, sof_boot_test_standalone_predicate, ll_karea_setup,
	    NULL, NULL, NULL);

/**
 * SOF main has booted up and IPC handling is stopped.
 * Run test suites with ztest_run_all.
 */
static int run_tests(void)
{
	ztest_run_test_suite(userspace_ll_karea, false, 1, 1, NULL);
	return 0;
}

SYS_INIT(run_tests, APPLICATION, 99);
