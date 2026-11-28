/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * These contents may have been developed with support from one or more
 * Intel-operated generative artificial intelligence solutions.
 *
 * Converted from CMock to Ztest
 * Original helpers from test/cmocka/src/audio/pipeline/pipeline_connection_mocks.h:
 * Author: Jakub Dabek <jakub.dabek@linux.intel.com>
 */

#ifndef __TEST_ZTEST_UNIT_AUDIO_PIPELINE_CONNECTION_MOCKS_H__
#define __TEST_ZTEST_UNIT_AUDIO_PIPELINE_CONNECTION_MOCKS_H__

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/schedule/schedule.h>

#define PIPELINE_ID_SAME 3
#define PIPELINE_ID_DIFFERENT 4

/**
 * @brief Pipeline, two components and two buffers used by the pipeline tests.
 *
 * The pipeline has the scheduling component @ref first, which belongs to
 * pipeline PIPELINE_ID_SAME, and an EDF pipeline task bound to a mock
 * scheduler. The component @ref second belongs to pipeline
 * PIPELINE_ID_DIFFERENT. Buffer @ref b1 goes from @ref first to @ref second and
 * buffer @ref b2 has @ref second as its source. Only the component pointers of
 * the buffers are set, none of the objects is linked to another.
 */
struct pipeline_connect_data {
	struct pipeline p;
	struct comp_dev *first;
	struct comp_dev *second;
	struct comp_buffer *b1;
	struct comp_buffer *b2;
};

/**
 * @brief Allocate and initialise the standard set of test objects.
 * @return The test objects, never NULL.
 */
struct pipeline_connect_data *get_standard_connect_objects(void);

/**
 * @brief Free the test objects returned by get_standard_connect_objects().
 * @param data Test objects, also freed.
 */
void free_standard_connect_objects(struct pipeline_connect_data *data);

/**
 * @brief Empty the component and buffer lists of the test objects.
 * @param data Test objects.
 */
void cleanup_test_data(struct pipeline_connect_data *data);

#endif /* __TEST_ZTEST_UNIT_AUDIO_PIPELINE_CONNECTION_MOCKS_H__ */
