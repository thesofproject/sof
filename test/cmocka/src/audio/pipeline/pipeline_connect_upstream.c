// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Jakub Dabek <jakub.dabek@linux.intel.com>

#include <stdint.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/schedule/edf_schedule.h>
#include "pipeline_mocks.h"
#include "pipeline_connection_mocks.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

static int setup(void **state)
{
	*state  = get_standard_connect_objects();
	return 0;
}

static int teardown(void **state)
{
	free(*state);
	return 0;
}

static void test_audio_pipeline_complete_wrong_status(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	result.status = COMP_STATE_READY;

	/*Testing component*/
	int error_code = pipeline_complete(&result, test_data->first,
					   test_data->second);

	assert_int_equal(error_code, -EINVAL);
}

static void test_audio_pipeline_complete_ready_state(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	/*Testing component*/
	int error_code = pipeline_complete(&result, test_data->first,
					   test_data->second);

	assert_int_equal(error_code, 0);
	assert_int_equal(result.status, COMP_STATE_READY);

}

static void test_audio_pipeline_complete_connect_is_endpoint(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	/*Testing component*/
	pipeline_complete(&result, test_data->first, test_data->second);

	/*Was not marked as endpoint (bsink and bsource lists empty)*/
	assert_true(list_is_empty(&result.sched_comp->bsource_list));
}

static void test_audio_pipeline_complete_connect_downstream_variable_set
	(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	/*Testing component*/
	pipeline_complete(&result, test_data->first, test_data->second);

	assert_ptr_equal(result.sched_comp->pipeline, &result);
	assert_ptr_equal(test_data->first->pipeline, &result);
}

/*Test going downstream ignoring sink from other pipeline*/
static void test_audio_pipeline_complete_connect_downstream_ignore_sink
	(void **state)
{
	struct pipeline_connect_data *test_data = *state;
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

	assert_true(list_is_empty(&test_data->first->bsource_list));
	assert_false(list_is_empty(&test_data->second->bsource_list));
}

/*Test going downstream ignoring source from other pipeline*/
static void test_audio_pipeline_complete_connect_upstream_ignore_source
	(void **state)
{
	struct pipeline_connect_data *test_data = *state;
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

	assert_true(list_is_empty(&test_data->first->bsink_list));
	assert_false(list_is_empty(&test_data->second->bsink_list));
}

/*Test going downstream all the way*/
static void test_audio_pipeline_complete_connect_downstream_full(void **state)
{
	struct pipeline_connect_data *test_data = *state;
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

	assert_ptr_equal(test_data->first->pipeline, &result);
	assert_ptr_equal(test_data->second->pipeline, &result);
}

/*Test going upstream all the way*/
static void test_audio_pipeline_complete_connect_upstream_full(void **state)
{
	struct pipeline_connect_data *test_data = *state;
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

	assert_ptr_equal(test_data->first->pipeline, &result);
	assert_ptr_equal(test_data->second->pipeline, &result);
}

/*Test going upstream all the way*/
static void test_audio_pipeline_complete_connect_upstream_other_pipeline
	(void **state)
{
	struct pipeline_connect_data *test_data = *state;
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

	assert_ptr_equal(test_data->first, result.source_comp);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(
		test_audio_pipeline_complete_wrong_status
		),
		cmocka_unit_test(
		test_audio_pipeline_complete_ready_state
		),
		cmocka_unit_test(
		test_audio_pipeline_complete_connect_is_endpoint
		),
		cmocka_unit_test(
		test_audio_pipeline_complete_connect_downstream_variable_set
		),
		cmocka_unit_test(
		test_audio_pipeline_complete_connect_downstream_ignore_sink
		),
		cmocka_unit_test(
		test_audio_pipeline_complete_connect_upstream_ignore_source
		),
		cmocka_unit_test(
		test_audio_pipeline_complete_connect_downstream_full
		),
		cmocka_unit_test(
		test_audio_pipeline_complete_connect_upstream_full
		),
		cmocka_unit_test(
		test_audio_pipeline_complete_connect_upstream_other_pipeline
		),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup, teardown);
}
