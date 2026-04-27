// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

#include "test_ipc4_pipeline_util.h"
#include <user/mfcc.h>

ZTEST(userspace_ipc4_pipeline, test_pipeline_create_destroy_helpers_native)
{
	k_sem_reset(&pipeline_test_sem);

	k_thread_create(&sync_test_thread, sync_test_stack, 4096,
			pipeline_create_destroy_helpers_thread, &pipeline_test_sem, NULL, (void *)false,
			K_PRIO_COOP(1), 0, K_FOREVER);

	k_thread_start(&sync_test_thread);

	k_sem_take(&pipeline_test_sem, K_FOREVER);
	k_thread_join(&sync_test_thread, K_FOREVER);
	k_msleep(10);
}

ZTEST(userspace_ipc4_pipeline, test_pipeline_create_destroy_handlers_native)
{
	k_sem_reset(&pipeline_test_sem);

	k_thread_create(&sync_test_thread, sync_test_stack, 4096,
			pipeline_create_destroy_handlers_thread, &pipeline_test_sem, NULL, (void *)false,
			K_PRIO_COOP(1), 0, K_FOREVER);

	k_thread_start(&sync_test_thread);

	k_sem_take(&pipeline_test_sem, K_FOREVER);
	k_thread_join(&sync_test_thread, K_FOREVER);
	k_msleep(10);
}

ZTEST(userspace_ipc4_pipeline, test_ipc4_pipeline_with_dp_native)
{
	k_sem_reset(&pipeline_test_sem);

	k_thread_create(&sync_test_thread, sync_test_stack, 4096,
			pipeline_with_dp_thread, &pipeline_test_sem, NULL, (void *)false,
			K_PRIO_COOP(1), 0, K_FOREVER);

	k_thread_start(&sync_test_thread);

	/* Wait for the thread to complete */
	k_sem_take(&pipeline_test_sem, K_FOREVER);
	k_thread_join(&sync_test_thread, K_FOREVER);
	k_msleep(10);
}

ZTEST(userspace_ipc4_pipeline, test_ipc4_pipeline_full_run_native)
{
	k_sem_reset(&pipeline_test_sem);

	k_thread_create(&sync_test_thread, sync_test_stack, 4096,
			pipeline_full_run_thread, &pipeline_test_sem, NULL, (void *)false,
			K_PRIO_COOP(1), 0, K_FOREVER);

	k_thread_start(&sync_test_thread);

	/* Wait for the thread to complete */
	k_sem_take(&pipeline_test_sem, K_FOREVER);
	k_thread_join(&sync_test_thread, K_FOREVER);
	k_msleep(10);
}

ZTEST(userspace_ipc4_pipeline, test_ipc4_multiple_pipelines_native)
{
	k_sem_reset(&pipeline_test_sem);

	k_thread_create(&sync_test_thread, sync_test_stack, 4096,
			multiple_pipelines_thread, &pipeline_test_sem, NULL, (void *)false,
			K_PRIO_COOP(1), 0, K_FOREVER);

	k_thread_start(&sync_test_thread);

	/* Wait for the thread to complete */
	k_sem_take(&pipeline_test_sem, K_FOREVER);
	k_thread_join(&sync_test_thread, K_FOREVER);
	k_msleep(10);
}

ZTEST(userspace_ipc4_pipeline, test_ipc4_all_modules_ll_pipeline_native)
{
	k_sem_reset(&pipeline_test_sem);

	k_thread_create(&sync_test_thread, sync_test_stack, 4096,
			all_modules_ll_pipeline_thread, &pipeline_test_sem, NULL, (void *)false,
			K_PRIO_COOP(1), 0, K_FOREVER);

	k_thread_start(&sync_test_thread);

	/* Wait for the thread to complete */
	k_sem_take(&pipeline_test_sem, K_FOREVER);
	k_thread_join(&sync_test_thread, K_FOREVER);
	k_msleep(10);
}

