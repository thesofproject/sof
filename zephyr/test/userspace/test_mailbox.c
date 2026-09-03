// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

/*
 * Test case for sof/mailbox.h interface use from a Zephyr user
 * thread.
 */

#include <sof/boot_test.h>
#include <sof/lib/mailbox.h>
#include <rtos/userspace_helper.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include <zephyr/app_memory/app_memdomain.h>

#include <ipc4/fw_reg.h> /* mailbox definitions */

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

#define USER_STACKSIZE	2048

static struct k_thread user_thread;
static K_THREAD_STACK_DEFINE(user_stack, USER_STACKSIZE);

static void mailbox_write_to_pipeline_regs(void)
{
	unsigned int offset = offsetof(struct ipc4_fw_registers, pipeline_regs);
	struct ipc4_pipeline_registers pipe_reg;

	/*
	 * IPC4 pipe_reg struct used for test, but this test also
	 * works for IPC3 targets.
	 */
	pipe_reg.stream_start_offset = (uint64_t)-1;
	pipe_reg.stream_end_offset = (uint64_t)-1;

	LOG_INF("Write to sw_regs mailbox at offset %u", offset);

	mailbox_sw_regs_write(offset, &pipe_reg, sizeof(pipe_reg));
}

static void mailbox_test_thread(void *p1, void *p2, void *p3)
{
	zassert_true(k_is_user_context(), "isn't user");

	LOG_INF("SOF thread %s (%s)",
		k_is_user_context() ? "UserSpace!" : "privileged mode.",
		CONFIG_BOARD_TARGET);

	mailbox_write_to_pipeline_regs();
}

static void mailbox_test(void)
{
	struct k_mem_domain domain;
	int ret = k_mem_domain_init(&domain, 0, NULL);

	zassert_equal(ret, 0);

	k_thread_create(&user_thread, user_stack, USER_STACKSIZE,
			mailbox_test_thread, NULL, NULL, NULL,
			-1, K_USER, K_FOREVER);

	LOG_INF("set up user access to mailbox");

	ret = user_access_to_mailbox(&domain, &user_thread);
	zassert_equal(ret, 0);

	k_thread_start(&user_thread);

	LOG_INF("user started, waiting in kernel until test complete");

	k_thread_join(&user_thread, K_FOREVER);
}

ZTEST(userspace_mailbox, mailbox_test)
{
	/* first test from kernel */
	mailbox_write_to_pipeline_regs();

	/* then full test in userspace */
	mailbox_test();

	ztest_test_pass();
}

ZTEST_SUITE(userspace_mailbox, NULL, NULL, NULL, NULL, NULL);

/**
 * SOF main has booted up and IPC handling is stopped.
 * Run test suites with ztest_run_all.
 */
static int run_tests(void)
{
	ztest_run_test_suite(userspace_mailbox, false, 1, 1, NULL);
	return 0;
}

SYS_INIT(run_tests, APPLICATION, 99);
