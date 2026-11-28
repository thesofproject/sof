// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// These contents may have been developed with support from one or more
// Intel-operated generative artificial intelligence solutions.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/audio/pipeline/pipeline_free.c:
// Author: Jakub Dabek <jakub.dabek@linux.intel.com>

#include <zephyr/ztest.h>

#include <rtos/alloc.h>
#include <rtos/sof.h>
#include <rtos/task.h>
#include <sof/audio/pipeline.h>

#include <stdbool.h>
#include <stddef.h>

#include "pipeline_connection_mocks.h"

static struct pipeline_connect_data *test_data;

/*
 * pipeline_free() frees the pipeline, its task and its alloc context. Here
 * these are a copy of the test pipeline on the stack and objects owned by the
 * test, which are inspected after the call and freed by the after hook, so
 * sof_heap_free() does nothing while this suite runs.
 */
static bool sof_heap_free_disabled;

void __real_sof_heap_free(struct k_heap *heap, void *addr);

void __wrap_sof_heap_free(struct k_heap *heap, void *addr)
{
	if (sof_heap_free_disabled)
		return;

	__real_sof_heap_free(heap, addr);
}

static void *pipeline_free_setup(void)
{
	pipeline_posn_init(sof_get());

	return NULL;
}

static void pipeline_free_before(void *fixture)
{
	(void)fixture;

	test_data = get_standard_connect_objects();
	sof_heap_free_disabled = true;
}

static void pipeline_free_after(void *fixture)
{
	(void)fixture;

	sof_heap_free_disabled = false;
	free_standard_connect_objects(test_data);
	test_data = NULL;
}

/**
 * @brief Verify that pipeline_free() returns 0
 */
ZTEST(pipeline_free_suite, test_audio_pipeline_free_return_value)
{
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	/*Testing component*/
	int err = pipeline_free(&result);

	zassert_equal(err, 0);
}

/**
 * @brief Verify that pipeline_free() frees the pipeline task in its scheduler
 */
ZTEST(pipeline_free_suite, test_audio_pipeline_free_sheduler_task_free)
{
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	/*Testing component*/
	pipeline_free(&result);

	zassert_equal(result.pipe_task->state, SOF_TASK_STATE_FREE);
	zassert_is_null(result.pipe_task->data);
	zassert_is_null(result.pipe_task->ops.run);
}

ZTEST_SUITE(pipeline_free_suite, NULL, pipeline_free_setup, pipeline_free_before,
	    pipeline_free_after, NULL);
