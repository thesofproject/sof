// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// These contents may have been developed with support from one or more
// Intel-operated generative artificial intelligence solutions.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/audio/pipeline/pipeline_new.c:
// Author: Jakub Dabek <jakub.dabek@linux.intel.com>

#include <zephyr/ztest.h>

#include <rtos/alloc.h>
#include <rtos/sof.h>
#include <sof/audio/pipeline.h>
#include <sof/ipc/msg.h>

#include <stdint.h>

#define TEST_PIPE_ID	1
#define TEST_PRIORITY	2
#define TEST_COMP_ID	3

static void *pipeline_new_setup(void)
{
	pipeline_posn_init(sof_get());

	return NULL;
}

/**
 * @brief Verify that pipeline_new() creates a pipeline
 */
ZTEST(pipeline_new_suite, test_audio_pipeline_pipeline_new_creation)
{
	/*Testing component*/
	struct pipeline *result = pipeline_new(NULL, TEST_PIPE_ID, TEST_PRIORITY,
					       TEST_COMP_ID, NULL);

	/*Pipeline should have been created so pointer can't be null*/
	zassert_not_null(result);
	/* IPC3 pipelines get a stream position message */
	zassert_not_null(result->msg);

	rfree(result->msg->tx_data);
	rfree(result->msg);
	/* The alloc context is allocated from the SOF heap with a NULL heap */
	sof_heap_free(NULL, result->alloc);
	rfree(result);
}

ZTEST_SUITE(pipeline_new_suite, NULL, pipeline_new_setup, NULL, NULL, NULL);
