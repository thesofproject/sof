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
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/audio/component.h>
#include <sof/audio/component_ext.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/buffer.h>
#include <sof/ipc/common.h>
#include <sof/ipc/topology.h>
#include <rtos/task.h>
#include <rtos/userspace_helper.h>
#include <ipc4/fw_reg.h>
#include <ipc4/module.h>
#include <ipc4/gateway.h>
#include <ipc4/header.h>
#include <ipc4/pipeline.h>
#include <ipc4/base_fw_vendor.h>
#include <module/ipc4/base-config.h>
#include <rimage/sof/user/manifest.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include <zephyr/app_memory/app_memdomain.h>

#include <stddef.h> /* offsetof() */
#include <string.h>

LOG_MODULE_DECLARE(sof_boot_test, LOG_LEVEL_DBG);

/* f11818eb-e92e-4082-82a3-dc54c604ebf3 */
SOF_DEFINE_UUID("test_task", test_task_uuid, 0xf11818eb, 0xe92e, 0x4082,
		0x82,  0xa3, 0xdc, 0x54, 0xc6, 0x04, 0xeb, 0xf3);

K_APPMEM_PARTITION_DEFINE(userspace_ll_part);

/* Global variable for test runs counter, accessible from user-space */
K_APP_BMEM(userspace_ll_part) static int test_runs;

/* User-space thread for pipeline_two_components test */
#define PPL_USER_STACKSIZE 4096

static struct k_thread ppl_user_thread;
static K_THREAD_STACK_DEFINE(ppl_user_stack, PPL_USER_STACKSIZE);

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
	k_sleep(K_MSEC(100));

	/* Cancel the task to stop any scheduled execution */
	ret = schedule_task_cancel(task);
	zassert_equal(ret, 0);

	/* Free task resources */
	ret = schedule_task_free(task);
	zassert_equal(ret, 0);

	k_mem_domain_remove_partition(zephyr_ll_mem_domain(), &userspace_ll_part);
	zephyr_ll_task_free(task);

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

static struct dma_info dummy_dma_info = {
	.dma_array = NULL,
	.num_dmas = 0,
};

static void *userspace_ll_setup(void)
{
	struct sof *sof = sof_get();

	k_mem_domain_add_partition(zephyr_ll_mem_domain(), &userspace_ll_part);
	sof->dma_info = &dummy_dma_info;
	sof->platform_timer_domain = zephyr_domain_init(19200000);
	scheduler_init_ll(sof->platform_timer_domain);
	return NULL;
}

ZTEST_SUITE(userspace_ll, NULL, userspace_ll_setup, NULL, NULL, NULL);


