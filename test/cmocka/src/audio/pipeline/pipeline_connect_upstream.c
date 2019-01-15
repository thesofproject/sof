/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Jakub Dabek <jakub.dabek@linux.intel.com>
 */

#include <stdint.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/schedule.h>
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
	int error_code = pipeline_complete(&result, test_data->first);

	assert_int_equal(error_code, -EINVAL);
}

static void test_audio_pipeline_complete_ready_state(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	/*Testing component*/
	int error_code = pipeline_complete(&result, test_data->first);

	assert_int_equal(error_code, 0);
	assert_int_equal(result.status, COMP_STATE_READY);

}

static void test_audio_pipeline_complete_connect_is_endpoint(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	/*Testing component*/
	pipeline_complete(&result, test_data->first);

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
	pipeline_complete(&result, test_data->first);

	assert_int_equal
	(
	result.sched_comp->frames, test_data->p.ipc_pipe.frames_per_sched
	);
	assert_ptr_equal(result.sched_comp->pipeline, &result);
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
	pipeline_complete(&result, test_data->first);

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
	test_data->b1->sink = result.sched_comp;
	test_data->b1->source = test_data->second;
	list_item_append(&test_data->b1->source_list,
					 &test_data->second->bsink_list);
	list_item_append(&test_data->second->bsource_list,
					 &test_data->b2->sink_list);
	test_data->b2->sink = test_data->second;

	/*Testing component*/
	pipeline_complete(&result, test_data->first);

	assert_true(list_is_empty(&test_data->first->bsink_list));
	assert_false(list_is_empty(&test_data->second->bsink_list));
}

/*Test going downstream all the way*/
static void test_audio_pipeline_complete_connect_downstream_full(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	/*Connecting first comp to second*/
	test_data->second->comp.pipeline_id = PIPELINE_ID_SAME;
	list_item_append(&result.sched_comp->bsink_list,
					 &test_data->b1->source_list);
	test_data->b1->source = result.sched_comp;
	list_item_append(&test_data->b1->source_list,
					 &result.sched_comp->bsink_list);
	test_data->b1->sink = test_data->second;
	list_item_append(&test_data->b1->sink_list,
					 &test_data->second->bsource_list);

	test_data->first->frames = 0;
	test_data->second->frames = 0;

	/*Testing component*/
	pipeline_complete(&result, test_data->first);

	assert_int_equal(test_data->first->frames,
					 result.ipc_pipe.frames_per_sched);
	assert_int_equal(test_data->second->frames,
					 result.ipc_pipe.frames_per_sched);
}

/*Test going upstream all the way*/
static void test_audio_pipeline_complete_connect_upstream_full(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	/*Connecting first comp to second*/
	test_data->second->comp.pipeline_id = PIPELINE_ID_SAME;
	list_item_append(&result.sched_comp->bsource_list,
					 &test_data->b1->sink_list);
	test_data->b1->sink = test_data->first;
	test_data->b1->source = test_data->second;

	/*Testing component*/
	pipeline_complete(&result, test_data->first);

	assert_int_equal(test_data->first->frames,
					 result.ipc_pipe.frames_per_sched);
	assert_int_equal(test_data->second->frames,
					 result.ipc_pipe.frames_per_sched);
}

/*Test going upstream all the way*/
static void test_audio_pipeline_complete_connect_upstream_other_pipeline
	(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	/*Connecting first comp to second*/
	test_data->second->comp.pipeline_id = PIPELINE_ID_DIFFERENT;
	list_item_append(&result.sched_comp->bsource_list,
					 &test_data->b1->sink_list);
	test_data->b1->sink = test_data->first;
	test_data->b1->source = test_data->second;
	list_item_append(&test_data->second->bsource_list,
					 &test_data->b1->source_list);

	/*Testing component*/
	pipeline_complete(&result, test_data->first);

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
