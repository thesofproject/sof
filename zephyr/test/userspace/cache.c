// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2025 Intel Corporation.
 */

#include <sof/boot_test.h>

#include <zephyr/kernel.h>
#include <rtos/cache.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

#define USER_STACKSIZE	2048

static struct k_thread user_thread;
static K_THREAD_STACK_DEFINE(user_stack, USER_STACKSIZE);

static void user_function(void *p1, void *p2, void *p3)
{
	char stack_buf[64];

	__ASSERT(k_is_user_context(), "isn't user");

	LOG_INF("SOF thread %s (%s)",
		k_is_user_context() ? "UserSpace!" : "privileged mode.",
		CONFIG_BOARD_TARGET);

	/*
	 * Use rtos/cache.h calls as these are also used by
	 * src/audio code.
	 */

	dcache_writeback_region(stack_buf, sizeof(stack_buf));

	dcache_invalidate_region(stack_buf, sizeof(stack_buf));
}

static void test_user_thread_cache(void)
{
	k_thread_create(&user_thread, user_stack, USER_STACKSIZE,
			user_function, NULL, NULL, NULL,
			-1, K_USER, K_MSEC(0));
	k_thread_join(&user_thread, K_FOREVER);
}

ZTEST(sof_boot, user_space_cache)
{
	test_user_thread_cache();

	ztest_test_pass();
}
