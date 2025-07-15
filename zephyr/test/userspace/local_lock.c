// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2025 Intel Corporation.
 */

#include <sof/boot_test.h>
#include <sof/sof_syscall.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

#define USER_STACKSIZE	2048

static struct k_thread user_thread;
static K_THREAD_STACK_DEFINE(user_stack, USER_STACKSIZE);

static void user_function(void *p1, void *p2, void *p3)
{
	__ASSERT(k_is_user_context(), "isn't user");
	LOG_INF("SOF thread %s (%s)",
	       k_is_user_context() ? "UserSpace!" : "privileged mode.",
	       CONFIG_BOARD_TARGET);
}

static void user_lock_function(void *p1, void *p2, void *p3)
{
	uint32_t flags = sof_local_lock();

	__ASSERT(k_is_user_context(), "isn't user");
	LOG_INF("SOF thread %s (%s)",
	       k_is_user_context() ? "UserSpace!" : "privileged mode.",
	       CONFIG_BOARD_TARGET);
	sof_local_unlock(flags);
}

static void test_user_thread(void)
{
	k_thread_create(&user_thread, user_stack, USER_STACKSIZE,
			user_function, NULL, NULL, NULL,
			-1, K_USER, K_MSEC(0));
	k_thread_join(&user_thread, K_FOREVER);
}

static void test_user_thread_with_lock(void)
{
	k_thread_create(&user_thread, user_stack, USER_STACKSIZE,
			user_lock_function, NULL, NULL, NULL,
			-1, K_USER, K_MSEC(0));
	k_thread_join(&user_thread, K_FOREVER);
}

ZTEST(sof_boot, user_space)
{
	test_user_thread();
	test_user_thread_with_lock();

	ztest_test_pass();
}
