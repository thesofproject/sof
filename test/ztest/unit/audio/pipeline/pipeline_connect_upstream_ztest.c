// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// These contents may have been developed with support from one or more
// Intel-operated generative artificial intelligence solutions.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/audio/pipeline/pipeline_connect_upstream.c:
// Author: Jakub Dabek <jakub.dabek@linux.intel.com>

#include <zephyr/ztest.h>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/list.h>

#include <errno.h>

#include "pipeline_connection_mocks.h"

/*
 * Every case gets new test objects, so that the cases do not depend on each
 * other or on their order: ztest runs them in alphabetical order.
 */
static struct pipeline_connect_data *test_data;

static void pipeline_connect_upstream_before(void *fixture)
{
	(void)fixture;

	test_data = get_standard_connect_objects();
}

static void pipeline_connect_upstream_after(void *fixture)
{
	(void)fixture;

	free_standard_connect_objects(test_data);
	test_data = NULL;
}

/**
 * @brief Verify that pipeline_complete() fails for a pipeline not in INIT state
 */
ZTEST(pipeline_connect_upstream_suite, test_audio_pipeline_complete_wrong_status)
{
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	result.status = COMP_STATE_READY;

	/*Testing component*/
	int error_code = pipeline_complete(&result, test_data->first,
					   test_data->second);

	zassert_equal(error_code, -EINVAL);
}

/**
 * @brief Verify that pipeline_complete() moves the pipeline to READY state
 */
ZTEST(pipeline_connect_upstream_suite, test_audio_pipeline_complete_ready_state)
{
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	/*Testing component*/
	int error_code = pipeline_complete(&result, test_data->first,
					   test_data->second);

	zassert_equal(error_code, 0);
	zassert_equal(result.status, COMP_STATE_READY);
}

/**
 * @brief Verify that pipeline_complete() does not connect anything to an
 *	  unconnected scheduling component
 */
ZTEST(pipeline_connect_upstream_suite, test_audio_pipeline_complete_connect_is_endpoint)
{
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	/*Testing component*/
	pipeline_complete(&result, test_data->first, test_data->second);

	/*Was not marked as endpoint (bsink and bsource lists empty)*/
	zassert_true(list_is_empty(&result.sched_comp->bsource_list));
}

/**
 * @brief Verify that pipeline_complete() sets the pipeline of the source
 *	  component
 */
ZTEST(pipeline_connect_upstream_suite, test_audio_pipeline_complete_connect_downstream_variable_set)
{
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	/*Testing component*/
	pipeline_complete(&result, test_data->first, test_data->second);

	zassert_equal_ptr(result.sched_comp->pipeline, &result);
	zassert_equal_ptr(test_data->first->pipeline, &result);
}

/**
 * @brief Test going downstream ignoring sink from other pipeline
 */
ZTEST(pipeline_connect_upstream_suite, test_audio_pipeline_complete_connect_downstream_ignore_sink)
{
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	/*Connecting first comp to second while second is from other pipeline
	 *and first has no upstream components
	 */
	list_item_append(&result.sched_comp->bsink_list,
			 &test_data->b1->source_list);
	list_item_append(&test_data->b1->source_list,
			 &result.sched_comp->bsink_list);
	list_item_append(&test_data->b1->sink_list,
			 &test_data->second->bsource_list);

	/*Testing component*/
	pipeline_complete(&result, test_data->first, test_data->second);

	zassert_true(list_is_empty(&test_data->first->bsource_list));
	zassert_false(list_is_empty(&test_data->second->bsource_list));
}

/**
 * @brief Test going downstream ignoring source from other pipeline
 */
ZTEST(pipeline_connect_upstream_suite, test_audio_pipeline_complete_connect_upstream_ignore_source)
{
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	/*Connecting first comp to second while second is from other pipeline
	 *and first has no downstream components
	 */
	list_item_append(&result.sched_comp->bsource_list,
			 &test_data->b1->sink_list);
	comp_buffer_set_sink_component(test_data->b1, result.sched_comp);
	comp_buffer_set_source_component(test_data->b1, test_data->second);
	list_item_append(&test_data->b1->source_list,
			 &test_data->second->bsink_list);
	list_item_append(&test_data->second->bsource_list,
			 &test_data->b2->sink_list);
	comp_buffer_set_sink_component(test_data->b2, test_data->second);

	/*Testing component*/
	pipeline_complete(&result, test_data->first, test_data->second);

	zassert_true(list_is_empty(&test_data->first->bsink_list));
	zassert_false(list_is_empty(&test_data->second->bsink_list));
}

/**
 * @brief Test going downstream all the way
 */
ZTEST(pipeline_connect_upstream_suite, test_audio_pipeline_complete_connect_downstream_full)
{
	struct pipeline result = test_data->p;
	struct comp_ipc_config *comp;

	cleanup_test_data(test_data);

	/*Connecting first comp to second*/
	comp = &test_data->second->ipc_config;
	comp->pipeline_id = PIPELINE_ID_SAME;
	list_item_append(&result.sched_comp->bsink_list, &test_data->b1->source_list);
	comp_buffer_set_source_component(test_data->b1, result.sched_comp);
	list_item_append(&test_data->b1->source_list, &result.sched_comp->bsink_list);
	comp_buffer_set_sink_component(test_data->b1, test_data->second);
	list_item_append(&test_data->b1->sink_list, &test_data->second->bsource_list);

	test_data->first->frames = 0;
	test_data->second->frames = 0;

	/*Testing component*/
	pipeline_complete(&result, test_data->first, test_data->second);

	zassert_equal_ptr(test_data->first->pipeline, &result);
	zassert_equal_ptr(test_data->second->pipeline, &result);
}

/**
 * @brief Test going upstream all the way
 */
ZTEST(pipeline_connect_upstream_suite, test_audio_pipeline_complete_connect_upstream_full)
{
	struct pipeline result = test_data->p;
	struct comp_ipc_config *comp;

	cleanup_test_data(test_data);

	/*Connecting first comp to second*/
	comp = &test_data->second->ipc_config;
	comp->pipeline_id = PIPELINE_ID_SAME;
	list_item_append(&result.sched_comp->bsource_list,
			 &test_data->b1->sink_list);
	comp_buffer_set_sink_component(test_data->b1, test_data->first);
	comp_buffer_set_source_component(test_data->b1, test_data->second);

	/*Testing component*/
	pipeline_complete(&result, test_data->first, test_data->second);

	zassert_equal_ptr(test_data->first->pipeline, &result);
	/*
	 * pipeline_complete() only walks downstream from the source component,
	 * so it does not reach the upstream component. The CMocka test expected
	 * &result here, which only held because the CMocka cases shared their
	 * objects: the previous case had set the pipeline of the second
	 * component to its own stack copy, at the same address as this one.
	 */
	zassert_is_null(test_data->second->pipeline);
}

/**
 * @brief Test going upstream to a component of another pipeline
 */
ZTEST(pipeline_connect_upstream_suite, test_audio_pipeline_complete_connect_upstream_other_pipeline)
{
	struct pipeline result = test_data->p;
	struct comp_ipc_config *comp;

	cleanup_test_data(test_data);

	/*Connecting first comp to second*/
	comp = &test_data->second->ipc_config;
	comp->pipeline_id = PIPELINE_ID_DIFFERENT;
	list_item_append(&result.sched_comp->bsource_list, &test_data->b1->sink_list);
	comp_buffer_set_sink_component(test_data->b1, test_data->first);
	comp_buffer_set_source_component(test_data->b1, test_data->second);
	list_item_append(&test_data->second->bsource_list, &test_data->b1->source_list);

	/*Testing component*/
	pipeline_complete(&result, test_data->first, test_data->second);

	zassert_equal_ptr(test_data->first, result.source_comp);
}

ZTEST_SUITE(pipeline_connect_upstream_suite, NULL, NULL, pipeline_connect_upstream_before,
	    pipeline_connect_upstream_after, NULL);
