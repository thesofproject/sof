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
}

#include <zephyr/sys/sem.h>
#include <zephyr/app_memory/mem_domain.h>

struct sem_mem {
	struct sys_sem sem1;
	struct sys_sem sem2;
	uint8_t reserved[4096 - 2 * sizeof(struct sys_sem)];
};

static struct sem_mem simple_sem __attribute__((aligned(4096)));
static struct k_mem_domain dp_mdom;

static void sys_sem_function(void *p1, void *p2, void *p3)
{
	__ASSERT(k_is_user_context(), "isn't user");
	/* This is the goal, but it hangs with this disabled too */
	sys_sem_give(&simple_sem.sem1);
	int ret = sys_sem_take(&simple_sem.sem2, K_MSEC(20));

	LOG_INF("SOF thread %s (%s) sem %p: %d",
		k_is_user_context() ? "UserSpace!" : "privileged mode.",
		CONFIG_BOARD_TARGET, &simple_sem, ret);
}

static void test_user_thread_sys_sem(void)
{
	struct k_mem_partition mpart = {
		.start = (uintptr_t)&simple_sem,
		.size = 4096,
		.attr = K_MEM_PARTITION_P_RW_U_RW/* | XTENSA_MMU_CACHED_WB*/,
	};

	k_mem_domain_init(&dp_mdom, 0, NULL);
	sys_sem_init(&simple_sem.sem1, 0, 1);
	sys_sem_init(&simple_sem.sem2, 0, 1);

	k_thread_create(&user_thread, user_stack, USER_STACKSIZE,
			sys_sem_function, NULL, NULL, NULL,
			-1, K_USER, K_FOREVER);
	k_mem_domain_add_partition(&dp_mdom, &mpart);
	k_mem_domain_add_thread(&dp_mdom, &user_thread);

	k_thread_start(&user_thread);

	/* This is what doesn't work: enabling this line crashes the DSP  */
	zassert_ok(sys_sem_take(&simple_sem.sem1, K_MSEC(20)));

	sys_sem_give(&simple_sem.sem2);

	k_thread_join(&user_thread, K_FOREVER);
	k_mem_domain_remove_partition(&dp_mdom, &mpart);
}

ZTEST(sof_boot, test_sys_sem)
{
	test_user_thread_sys_sem();
}
