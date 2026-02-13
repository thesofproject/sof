// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

/*
 * Test case for sof_heap_alloc() / sof_heap_free() use from a Zephyr
 * user-space thread.
 */

#include <sof/boot_test.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <rtos/alloc.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

#define USER_STACKSIZE	2048

static struct k_thread user_thread;
static K_THREAD_STACK_DEFINE(user_stack, USER_STACKSIZE);

static void user_function(void *p1, void *p2, void *p3)
{
	struct k_heap *heap = (struct k_heap *)p1;
	void *ptr;

	__ASSERT(k_is_user_context(), "isn't user");

	LOG_INF("SOF thread %s (%s)",
		k_is_user_context() ? "UserSpace!" : "privileged mode.",
		CONFIG_BOARD_TARGET);

	/* allocate a block from the user heap */
	ptr = sof_heap_alloc(heap, SOF_MEM_FLAG_USER, 128, 0);
	zassert_not_null(ptr, "sof_heap_alloc returned NULL");

	LOG_INF("sof_heap_alloc returned %p", ptr);

	/* free the block */
	sof_heap_free(heap, ptr);

	LOG_INF("sof_heap_free done");
}

static void test_user_thread_heap_alloc(void)
{
	struct k_heap *heap;

	heap = zephyr_ll_user_heap();
	zassert_not_null(heap, "user heap not found");

	k_thread_create(&user_thread, user_stack, USER_STACKSIZE,
			user_function, heap, NULL, NULL,
			-1, K_USER, K_FOREVER);

	/* Add thread to LL memory domain so it can access the user heap */
	k_mem_domain_add_thread(zephyr_ll_mem_domain(), &user_thread);

	k_thread_start(&user_thread);
	k_thread_join(&user_thread, K_FOREVER);
}

ZTEST(sof_boot, user_space_heap_alloc)
{
	test_user_thread_heap_alloc();

	ztest_test_pass();
}
