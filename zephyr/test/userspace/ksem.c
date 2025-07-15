// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2025 Intel Corporation.
 */

#include <sof/boot_test.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

#define USER_STACKSIZE	2048

static struct k_thread user_thread;
static K_THREAD_STACK_DEFINE(user_stack, USER_STACKSIZE);
K_SEM_DEFINE(user_sem, 0, 1);

static void user_function(void *p1, void *p2, void *p3)
{
	__ASSERT(k_is_user_context(), "isn't user");
	LOG_INF("SOF thread %s (%s)",
	       k_is_user_context() ? "UserSpace!" : "privileged mode.",
	       CONFIG_BOARD_TARGET);
}

static void user_sem_function(void *p1, void *p2, void *p3)
{
	__ASSERT(k_is_user_context(), "isn't user");
	LOG_INF("SOF thread %s (%s)",
	       k_is_user_context() ? "UserSpace!" : "privileged mode.",
	       CONFIG_BOARD_TARGET);
	k_sem_give(&user_sem);
}

static void test_user_thread(void)
{
	k_thread_create(&user_thread, user_stack, USER_STACKSIZE,
			user_function, NULL, NULL, NULL,
			-1, K_USER, K_MSEC(0));
	k_thread_join(&user_thread, K_FOREVER);
}

static void test_user_thread_with_sem(void)
{
	/* Start in 10ms to have time to grant the thread access to the semaphore */
	k_thread_create(&user_thread, user_stack, USER_STACKSIZE,
			user_sem_function, NULL, NULL, NULL,
			-1, K_USER, K_MSEC(10));
	k_thread_access_grant(&user_thread, &user_sem);
	k_sem_take(&user_sem, K_FOREVER);
	k_thread_join(&user_thread, K_FOREVER);
}

ZTEST(sof_boot, user_space)
{
	test_user_thread();
	test_user_thread_with_sem();

	ztest_test_pass();
}
