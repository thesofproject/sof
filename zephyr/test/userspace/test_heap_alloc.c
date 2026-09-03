// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

/*
 * Test cases for sof_heap_alloc() / sof_heap_free() use from a Zephyr
 * user-space thread.
 */

#include <sof/boot_test.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <rtos/alloc.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include <zephyr/app_memory/app_memdomain.h>

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

#define USER_STACKSIZE	2048

static struct k_thread user_thread;
static K_THREAD_STACK_DEFINE(user_stack, USER_STACKSIZE);

K_APPMEM_PARTITION_DEFINE(heap_alloc_test_part);
K_APP_BMEM(heap_alloc_test_part) static uint8_t non_heap_byte;

enum heap_alloc_test_case {
	HEAP_ALLOC_VALID,
	HEAP_ALLOC_NULL_HEAP,
	HEAP_ALLOC_FORBIDDEN_FLAGS,
	HEAP_FREE_NON_HEAP_POINTER,
	HEAP_FREE_INACCESSIBLE_POINTER,
};

static void user_function(void *p1, void *p2, void *p3)
{
	struct k_heap *heap = p1;
	enum heap_alloc_test_case test_case = (enum heap_alloc_test_case)(uintptr_t)p2;
	void *free_ptr = p3;
	void *ptr;

	__ASSERT(k_is_user_context(), "isn't user");

	LOG_INF("SOF thread %s (%s)",
		k_is_user_context() ? "UserSpace!" : "privileged mode.",
		CONFIG_BOARD_TARGET);

	switch (test_case) {
	case HEAP_ALLOC_VALID:
		ptr = sof_heap_alloc(heap, SOF_MEM_FLAG_USER, 128, 0);
		zassert_not_null(ptr, "sof_heap_alloc returned NULL");
		sof_heap_free(heap, ptr);
		return;
	case HEAP_ALLOC_NULL_HEAP:
		(void)sof_heap_alloc(NULL, SOF_MEM_FLAG_USER, 128, 0);
		break;
	case HEAP_ALLOC_FORBIDDEN_FLAGS:
		(void)sof_heap_alloc(heap, SOF_MEM_FLAG_LARGE_BUFFER, 128, 0);
		break;
	case HEAP_FREE_NON_HEAP_POINTER:
		sof_heap_free(heap, &non_heap_byte);
		break;
	case HEAP_FREE_INACCESSIBLE_POINTER:
		sof_heap_free(heap, free_ptr);
		break;
	default:
		zassert_unreachable("unknown heap allocation test case");
	}

	zassert_unreachable("syscall security check did not fault");
}

static void run_user_heap_alloc_case(enum heap_alloc_test_case test_case, struct k_heap *heap,
				     void *ptr, bool grant_ll_domain, bool expect_fault)
{
	int ret;

	k_thread_create(&user_thread, user_stack, USER_STACKSIZE,
			user_function, heap, (void *)(uintptr_t)test_case, ptr,
			-1, K_USER, K_FOREVER);

	if (grant_ll_domain)
		k_mem_domain_add_thread(zephyr_ll_mem_domain(), &user_thread);

	if (expect_fault)
		sof_boot_test_set_fault_valid(&user_thread);

	k_thread_start(&user_thread);
	ret = k_thread_join(&user_thread, K_FOREVER);
	zassert_equal(ret, 0, "user thread join failed: %d", ret);
}

static void test_user_thread_heap_alloc(enum heap_alloc_test_case test_case)
{
	bool grant_ll_domain = true;
	bool expect_fault = false;
	void *ptr = NULL;
	struct k_heap *heap;

	heap = zephyr_ll_user_heap();
	zassert_not_null(heap, "user heap not found");

	switch (test_case) {
	case HEAP_ALLOC_VALID:
		break;
	case HEAP_ALLOC_NULL_HEAP:
	case HEAP_ALLOC_FORBIDDEN_FLAGS:
	case HEAP_FREE_NON_HEAP_POINTER:
		expect_fault = true;
		break;
	case HEAP_FREE_INACCESSIBLE_POINTER:
		ptr = sof_heap_alloc(heap, SOF_MEM_FLAG_USER, 128, 0);
		zassert_not_null(ptr, "kernel heap allocation failed");
		grant_ll_domain = false;
		expect_fault = true;
		break;
	default:
		zassert_unreachable("unknown heap allocation test case");
	}

	run_user_heap_alloc_case(test_case, heap, ptr, grant_ll_domain, expect_fault);

	sof_heap_free(heap, ptr);
}

ZTEST(sof_boot, user_space_heap_alloc)
{
	test_user_thread_heap_alloc(HEAP_ALLOC_VALID);
}

ZTEST(sof_boot, user_space_heap_alloc_rejects_null_heap)
{
	test_user_thread_heap_alloc(HEAP_ALLOC_NULL_HEAP);
}

ZTEST(sof_boot, user_space_heap_alloc_rejects_forbidden_flags)
{
	test_user_thread_heap_alloc(HEAP_ALLOC_FORBIDDEN_FLAGS);
}

ZTEST(sof_boot, user_space_heap_free_rejects_non_heap_pointer)
{
	k_mem_domain_add_partition(zephyr_ll_mem_domain(), &heap_alloc_test_part);

	test_user_thread_heap_alloc(HEAP_FREE_NON_HEAP_POINTER);

	k_mem_domain_remove_partition(zephyr_ll_mem_domain(), &heap_alloc_test_part);
}

ZTEST(sof_boot, user_space_heap_free_rejects_inaccessible_pointer)
{
	test_user_thread_heap_alloc(HEAP_FREE_INACCESSIBLE_POINTER);
}
