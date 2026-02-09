// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

/*
 * Test case for creation of low-latency threads in user-space.
 */

#include <sof/boot_test.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/uuid.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/audio/pipeline.h>
#include <rtos/task.h>
#include <rtos/userspace_helper.h>
#include <ipc4/fw_reg.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include <zephyr/app_memory/app_memdomain.h>

#include <stddef.h> /* offsetof() */

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

/* f11818eb-e92e-4082-82a3-dc54c604ebf3 */
SOF_DEFINE_UUID("test_task", test_task_uuid, 0xf11818eb, 0xe92e, 0x4082,
		0x82,  0xa3, 0xdc, 0x54, 0xc6, 0x04, 0xeb, 0xf3);

K_APPMEM_PARTITION_DEFINE(userspace_ll_part);

/* Global variable for test runs counter, accessible from user-space */
K_APP_BMEM(userspace_ll_part) static int test_runs;

static enum task_state task_callback(void *arg)
{
	LOG_INF("entry");

	if (++test_runs > 3)
		return SOF_TASK_STATE_COMPLETED;

	return SOF_TASK_STATE_RESCHEDULE;
}

static void ll_task_test(void)
{
	struct task *task;
	int priority = 0;
	int core = 0;
	int ret;

	/* Initialize global test runs counter */
	test_runs = 0;

	task = zephyr_ll_task_alloc();
	zassert_not_null(task, "task allocation failed");

	/* allow user space to report status via 'test_runs' */
	k_mem_domain_add_partition(zephyr_ll_mem_domain(), &userspace_ll_part);

	/* work in progress, see pipeline-schedule.c */
	ret = schedule_task_init_ll(task, SOF_UUID(test_task_uuid), SOF_SCHEDULE_LL_TIMER,
				    priority, task_callback,
				    (void *)&test_runs, core, 0);
	zassert_equal(ret, 0);

	LOG_INF("task init done");

	/* Schedule the task to run immediately with 1ms period */
	ret = schedule_task(task, 0, 1000);  /* 0 = start now, 1000us = 1ms period */
	zassert_equal(ret, 0);

	LOG_INF("task scheduled and running");

	/* Let the task run for a bit */
	k_sleep(K_MSEC(10));

	/* Cancel the task to stop any scheduled execution */
	ret = schedule_task_cancel(task);
	zassert_equal(ret, 0);

	/* Free task resources */
	ret = schedule_task_free(task);
	zassert_equal(ret, 0);

	LOG_INF("test complete");
}

ZTEST(userspace_ll, ll_task_test)
{
	ll_task_test();
}

static void pipeline_check(void)
{
	struct pipeline *p;
	struct k_heap *heap;
	uint32_t pipeline_id = 1;
	uint32_t priority = 5;
	uint32_t comp_id = 10;
	int ret;

	heap = zephyr_ll_user_heap();
	zassert_not_null(heap, "user heap not found");

	/* Create pipeline on user heap */
	p = pipeline_new(heap, pipeline_id, priority, comp_id, NULL);
	zassert_not_null(p, "pipeline creation failed");

	/* Verify heap assignment */
	zassert_equal(p->heap, heap, "pipeline heap not equal to user heap");

	/* Verify pipeline properties */
	zassert_equal(p->pipeline_id, pipeline_id, "pipeline id mismatch");
	zassert_equal(p->priority, priority, "priority mismatch");
	zassert_equal(p->comp_id, comp_id, "comp id mismatch");

	/* Free pipeline */
	ret = pipeline_free(p);
	zassert_ok(ret, "pipeline free failed");
}

ZTEST(userspace_ll, pipeline_check)
{
	pipeline_check();
}

ZTEST_SUITE(userspace_ll, NULL, NULL, NULL, NULL, NULL);

/**
 * SOF main has booted up and IPC handling is stopped.
 * Run test suites with ztest_run_all.
 */
static int run_tests(void)
{
	ztest_run_test_suite(userspace_ll, false, 1, 1, NULL);
	return 0;
}

SYS_INIT(run_tests, APPLICATION, 99);
