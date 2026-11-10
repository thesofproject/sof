// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

/*
 * Tests for scheduler_dp_kick(): the DP thread objects are taken from the
 * kernel-only DP task records, never from the caller, and only the LL user
 * thread may call it.
 */

#include <sof/boot_test.h>
#include <sof/lib/cpu.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/userspace/syscall_caller.h>
#include <rtos/task.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

#define USER_STACKSIZE	2048
#define TEST_DEADLINE	1000

/* registry slot used for the test thread, not used by any LL thread here */
#define TEST_LL_ROLE_CORE	(CONFIG_CORE_COUNT - 1)

static struct k_thread user_thread;
static K_THREAD_STACK_DEFINE(user_stack, USER_STACKSIZE);

/* stand-in DP thread, never started, only its deadline is checked */
static struct k_thread dp_thread;
static K_THREAD_STACK_DEFINE(dp_stack, 512);
static struct k_event dp_event;

/* DP tasks, only their addresses are used as record keys */
static struct task task_here;
static struct task task_other_core;
static struct task task_unknown;

static struct scheduler_dp_kick kernel_only_kicks[1];

enum kick_test_case {
	/* kick the task of this core */
	KICK_THIS_CORE,
	/* kick the task of another core */
	KICK_OTHER_CORE,
	/* kick a task without a record */
	KICK_UNKNOWN_TASK,
	/* pass more entries than allowed */
	KICK_TOO_MANY,
	/* pass a kernel address as the array */
	KICK_KERNEL_ARRAY,
};

static void dp_thread_fn(void *p1, void *p2, void *p3)
{
}

static void user_function(void *p1, void *p2, void *p3)
{
	enum kick_test_case test_case = (enum kick_test_case)(uintptr_t)p1;
	struct scheduler_dp_kick kicks[SCHEDULER_DP_KICK_MAX + 1] = {
		[0] = {
			.flags = SCHEDULER_DP_KICK_PROCESS | SCHEDULER_DP_KICK_DEADLINE,
			.deadline = TEST_DEADLINE,
		},
	};

	zassert_true(k_is_user_context(), "isn't user");

	switch (test_case) {
	case KICK_THIS_CORE:
		kicks[0].task = &task_here;
		zassert_equal(scheduler_dp_kick(kicks, 1), 0);
		return;
	case KICK_OTHER_CORE:
		kicks[0].task = &task_other_core;
		zassert_equal(scheduler_dp_kick(kicks, 1), 1);
		return;
	case KICK_UNKNOWN_TASK:
		kicks[0].task = &task_unknown;
		zassert_equal(scheduler_dp_kick(kicks, 1), 1);
		return;
	case KICK_TOO_MANY:
		(void)scheduler_dp_kick(kicks, ARRAY_SIZE(kicks));
		break;
	case KICK_KERNEL_ARRAY:
		(void)scheduler_dp_kick(kernel_only_kicks, 1);
		break;
	default:
		zassert_unreachable("unknown test case");
	}

	zassert_unreachable("syscall security check did not fault");
}

static void run_case(enum kick_test_case test_case, bool register_ll, bool expect_fault)
{
	int ret;

	k_thread_create(&user_thread, user_stack, USER_STACKSIZE,
			user_function, (void *)(uintptr_t)test_case, NULL, NULL,
			-1, K_USER, K_FOREVER);
	k_mem_domain_add_thread(zephyr_ll_mem_domain(), &user_thread);
	/* records are checked against the core the kick runs on */
	zassert_equal(k_thread_cpu_pin(&user_thread, cpu_get_id()), 0);

	if (register_ll)
		sof_sysuser_thread_set(SOF_SYSUSER_LL, TEST_LL_ROLE_CORE, &user_thread);
	if (expect_fault)
		sof_boot_test_set_fault_valid(&user_thread);

	k_thread_start(&user_thread);
	ret = k_thread_join(&user_thread, K_FOREVER);

	sof_sysuser_thread_set(SOF_SYSUSER_LL, TEST_LL_ROLE_CORE, NULL);
	sof_boot_test_set_fault_valid(NULL);

	zassert_equal(ret, 0, "user thread join failed: %d", ret);
}

static void *dp_kick_setup(void)
{
	unsigned int other_core = (cpu_get_id() + 1) % CONFIG_CORE_COUNT;

	k_event_init(&dp_event);
	k_thread_create(&dp_thread, dp_stack, K_THREAD_STACK_SIZEOF(dp_stack),
			dp_thread_fn, NULL, NULL, NULL, 0, 0, K_FOREVER);

	zassert_equal(scheduler_dp_test_record_add(&task_here, &dp_event, &dp_thread,
						   cpu_get_id()), 0);
	zassert_equal(scheduler_dp_test_record_add(&task_other_core, &dp_event, &dp_thread,
						   other_core), 0);

	return NULL;
}

static void dp_kick_teardown(void *fixture)
{
	scheduler_dp_test_record_del(&task_here);
	scheduler_dp_test_record_del(&task_other_core);
	k_thread_abort(&dp_thread);
}

static void dp_kick_before(void *fixture)
{
	k_event_clear(&dp_event, UINT32_MAX);
}

ZTEST(userspace_dp_kick, ll_thread_kicks_own_core_task)
{
	uint32_t before = k_cycle_get_32();
	uint32_t after;
	uint32_t start;

	run_case(KICK_THIS_CORE, true, false);
	after = k_cycle_get_32();

	zassert_equal(k_event_test(&dp_event, UINT32_MAX), DP_TASK_EVENT_PROCESS);

	/* the deadline is relative to the time of the call */
	start = (uint32_t)dp_thread.base.prio_deadline - TEST_DEADLINE;
	zassert_true(start - before <= after - before, "unexpected deadline");
}

ZTEST(userspace_dp_kick, other_core_task_skipped)
{
	if (CONFIG_CORE_COUNT < 2)
		ztest_test_skip();

	run_case(KICK_OTHER_CORE, true, false);
	zassert_equal(k_event_test(&dp_event, UINT32_MAX), 0);
}

ZTEST(userspace_dp_kick, unknown_task_skipped)
{
	run_case(KICK_UNKNOWN_TASK, true, false);
	zassert_equal(k_event_test(&dp_event, UINT32_MAX), 0);
}

ZTEST(userspace_dp_kick, unregistered_thread_rejected)
{
	run_case(KICK_THIS_CORE, false, true);
	zassert_equal(k_event_test(&dp_event, UINT32_MAX), 0);
}

ZTEST(userspace_dp_kick, too_many_entries_rejected)
{
	run_case(KICK_TOO_MANY, true, true);
}

ZTEST(userspace_dp_kick, kernel_array_rejected)
{
	run_case(KICK_KERNEL_ARRAY, true, true);
}

ZTEST_SUITE(userspace_dp_kick, sof_boot_test_standalone_predicate, dp_kick_setup,
	    dp_kick_before, NULL, dp_kick_teardown);

/**
 * SOF main has booted up and IPC handling is stopped.
 * Run test suites with ztest_run_all.
 */
static int run_tests(void)
{
	ztest_run_test_suite(userspace_dp_kick, false, 1, 1, NULL);
	return 0;
}

SYS_INIT(run_tests, APPLICATION, 99);
