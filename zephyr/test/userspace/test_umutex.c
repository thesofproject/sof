// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

/**
 * @file
 * @brief Test case for sof_umutex API from a Zephyr user-space thread.
 *
 * Validates that sof_umutex_init/lock/unlock/free work correctly when
 * called from user-space context.
 */

#include <sof/boot_test.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <rtos/umutex.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include <zephyr/app_memory/app_memdomain.h>

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

#define USER_STACKSIZE	2048

static struct k_thread umutex_user_thread;
static K_THREAD_STACK_DEFINE(umutex_user_stack, USER_STACKSIZE);

/* Memory partition for test data accessible from user-space */
K_APPMEM_PARTITION_DEFINE(umutex_test_part);

/* Place the sof_umutex state in the user-accessible partition */
K_APP_BMEM(umutex_test_part) static struct sof_umutex test_umutex;

static void umutex_user_function(void *p1, void *p2, void *p3)
{
	int ret;

	__ASSERT(k_is_user_context(), "isn't user");

	LOG_INF("umutex test thread %s (%s)",
		k_is_user_context() ? "UserSpace!" : "privileged mode.",
		CONFIG_BOARD_TARGET);

	/* Initialize the umutex — allocates backing k_mutex */
	ret = sof_umutex_init(&test_umutex);
	zassert_equal(ret, 0, "sof_umutex_init failed: %d", ret);

	LOG_INF("sof_umutex_init succeeded");

	/* Lock the mutex */
	ret = sof_umutex_lock(&test_umutex, K_FOREVER);
	zassert_equal(ret, 0, "sof_umutex_lock failed: %d", ret);

	LOG_INF("sof_umutex_lock succeeded");

	/* Unlock the mutex */
	ret = sof_umutex_unlock(&test_umutex);
	zassert_equal(ret, 0, "sof_umutex_unlock failed: %d", ret);

	LOG_INF("sof_umutex_unlock succeeded");

	/* Free the mutex — releases backing k_mutex */
	sof_umutex_free(&test_umutex);

	LOG_INF("sof_umutex_free done");
}

static void test_user_thread_umutex(void)
{
	/* Add test partition to LL memory domain so user thread can access test_umutex */
	k_mem_domain_add_partition(zephyr_ll_mem_domain(), &umutex_test_part);

	k_thread_create(&umutex_user_thread, umutex_user_stack, USER_STACKSIZE,
			umutex_user_function, NULL, NULL, NULL,
			-1, K_USER, K_FOREVER);

	/* Add thread to LL memory domain so it can access the partition */
	k_mem_domain_add_thread(zephyr_ll_mem_domain(), &umutex_user_thread);

	k_thread_start(&umutex_user_thread);
	k_thread_join(&umutex_user_thread, K_FOREVER);

	k_mem_domain_remove_partition(zephyr_ll_mem_domain(), &umutex_test_part);
}

ZTEST(sof_boot, user_space_umutex)
{
	test_user_thread_umutex();

	ztest_test_pass();
}
