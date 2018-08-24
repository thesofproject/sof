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
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#define PIPELINE_ID_SAME 3
#define PIPELINE_ID_DIFFERENT 4

static int setup(void **state)
{
	struct pipeline_connect_upstream_data *pipeline_connect_data = calloc(
		sizeof(struct pipeline_connect_upstream_data), 1);

	struct sof_ipc_pipe_new pipe_desc = {
		.frames_per_sched = 5,
		.pipeline_id = PIPELINE_ID_SAME };

	struct pipeline *pipe = calloc(sizeof(struct pipeline), 1);

	pipe->ipc_pipe = pipe_desc;
	pipe->status = COMP_STATE_INIT;

	struct comp_dev *first = calloc(sizeof(struct comp_dev), 1);

	first->is_endpoint = 0;
	first->comp.id = 3;
	first->comp.pipeline_id = PIPELINE_ID_SAME;
	list_init(&first->bsink_list);
	list_init(&first->bsource_list);
	list_init(&pipe->comp_list);
	list_init(&pipe->buffer_list);
	pipeline_connect_data->first = first;
	pipe->sched_comp = first;

	struct comp_dev *second = calloc(sizeof(struct comp_dev), 1);

	second->is_endpoint = 0;
	second->comp.id = 4;
	second->comp.pipeline_id = PIPELINE_ID_DIFFERENT;
	list_init(&second->bsink_list);
	list_init(&second->bsource_list);
	pipeline_connect_data->second = second;

	struct comp_buffer *buffer = calloc(sizeof(struct comp_buffer), 1);

	buffer->source = first;
	buffer->sink = second;
	list_init(&buffer->sink_list);
	list_init(&buffer->source_list);
	pipeline_connect_data->b1 = buffer;

	struct comp_buffer *buffer_2 = calloc(sizeof(struct comp_buffer), 1);

	buffer_2->source = second;
	list_init(&buffer_2->sink_list);
	list_init(&buffer_2->source_list);
	pipeline_connect_data->b2 = buffer_2;

	pipeline_connect_data->p = *pipe;

	*state = pipeline_connect_data;

	return 0;
}

static int teardown(void **state)
{
	free(*state);
	return 0;
}

static void test_audio_pipeline_complete_wrong_status(void **state)
{
	struct pipeline_connect_upstream_data *test_data = *state;
	struct pipeline *result = calloc(sizeof(struct pipeline), 1);

	*result = test_data->p;
	result->status = COMP_STATE_READY;

	/*Testing component*/
	int error_code = pipeline_complete(result);

	assert_int_equal(error_code, -EINVAL);

	free(result);
}

static void test_audio_pipeline_complete_ready_state(void **state)
{
	struct pipeline_connect_upstream_data *test_data = *state;
	struct pipeline *result = calloc(sizeof(struct pipeline), 1);

	*result = test_data->p;

	/*Testing component*/
	int error_code = pipeline_complete(result);

	assert_int_equal(error_code, 0);
	assert_int_equal(result->status, COMP_STATE_READY);

	free(result);
}

static void test_audio_pipeline_complete_connect_is_endpoint(void **state)
{
	struct pipeline_connect_upstream_data *test_data = *state;
	struct pipeline *result = calloc(sizeof(struct pipeline), 1);

	*result = test_data->p;

	/*Testing component*/
	pipeline_complete(result);

	/*Was not marked as endpoint (bsink and bsource lists empty)*/
	assert_int_equal(result->sched_comp->is_endpoint, 1);
}

static void test_audio_pipeline_complete_connect_downstream_variable_set
	(void **state)
{
	struct pipeline_connect_upstream_data *test_data = *state;
	struct pipeline *result = calloc(sizeof(struct pipeline), 1);

	*result = test_data->p;

	/*Testing component*/
	pipeline_complete(result);

	assert_int_equal(result->sched_comp->frames,
		test_data->p.ipc_pipe.frames_per_sched);
	assert_ptr_equal(result->sched_comp->pipeline, result);

	free(result);
}

/*Test going downstream ignoring sink from other pipeline*/
static void test_audio_pipeline_complete_connect_downstream_ignore_sink
	(void **state)
{
	struct pipeline_connect_upstream_data *test_data = *state;
	struct pipeline *result = calloc(sizeof(struct pipeline), 1);

	*result = test_data->p;

	/*Connecting first comp to second while second is from other pipeline
	 *and first has no upstream components
	 */
	test_data->first->is_endpoint = 0;
	test_data->second->is_endpoint = 0;
	list_item_append(&result->sched_comp->bsink_list,
		&test_data->b1->source_list);
	list_item_append(&test_data->b1->source_list,
		&result->sched_comp->bsink_list);
	list_item_append(&test_data->b1->sink_list,
		&test_data->second->bsource_list);

	/*Testing component*/
	pipeline_complete(result);

	assert_int_equal(test_data->first->is_endpoint, 1);
	assert_int_equal(test_data->second->is_endpoint, 0);

	/*Cleanup*/
	list_init(&result->sched_comp->bsink_list);
	list_init(&test_data->b1->source_list);
	list_init(&test_data->b1->sink_list);
	free(result);
}

/*Test going downstream ignoring source from other pipeline*/
static void test_audio_pipeline_complete_connect_upstream_ignore_source
	(void **state)
{
	struct pipeline_connect_upstream_data *test_data = *state;
	struct pipeline *result = calloc(sizeof(struct pipeline), 1);

	*result = test_data->p;

	/*Connecting first comp to second while second is from other pipeline
	 *and first has no downstream components
	 */
	test_data->first->is_endpoint = 0;
	test_data->second->is_endpoint = 0;
	list_item_append(&result->sched_comp->bsource_list,
		&test_data->b1->sink_list);
	test_data->b1->sink = result->sched_comp;
	test_data->b1->source = test_data->second;
	list_item_append(&test_data->b1->source_list,
		&test_data->second->bsink_list);
	list_item_append(&test_data->second->bsource_list,
		&test_data->b2->sink_list);
	test_data->b2->sink = test_data->second;

	/*Testing component*/
	pipeline_complete(result);

	assert_int_equal(test_data->first->is_endpoint, 1);
	assert_int_equal(test_data->second->is_endpoint, 0);

	/*Cleanup*/
	list_init(&result->sched_comp->bsource_list);
	list_init(&test_data->b1->source_list);
	list_init(&test_data->second->bsource_list);
	free(result);
}

/*Test going downstream all the way*/
static void test_audio_pipeline_complete_connect_downstream_full(void **state)
{
	struct pipeline_connect_upstream_data *test_data = *state;
	struct pipeline *result = calloc(sizeof(struct pipeline), 1);

	*result = test_data->p;

	/*Connecting first comp to second*/
	test_data->first->is_endpoint = 0;
	test_data->second->is_endpoint = 0;
	test_data->second->comp.pipeline_id = PIPELINE_ID_SAME;
	list_item_append(&result->sched_comp->bsink_list,
		&test_data->b1->source_list);
	test_data->b1->source = result->sched_comp;
	list_item_append(&test_data->b1->source_list,
		&result->sched_comp->bsink_list);
	test_data->b1->sink = test_data->second;
	list_item_append(&test_data->b1->sink_list,
		&test_data->second->bsource_list);
	list_init(&test_data->second->bsink_list);

	test_data->first->frames = 0;
	test_data->second->frames = 0;

	/*Testing component*/
	pipeline_complete(result);

	assert_int_equal(test_data->first->frames,
		result->ipc_pipe.frames_per_sched);
	assert_int_equal(test_data->second->frames,
		result->ipc_pipe.frames_per_sched);

	/*Cleanup*/
	list_init(&result->sched_comp->bsink_list);
	list_init(&result->sched_comp->bsource_list);
	list_init(&test_data->b1->source_list);
	list_init(&test_data->b1->sink_list);
	list_init(&test_data->second->bsource_list);
	list_init(&test_data->second->bsink_list);
	free(result);
}

/*Test going upstream all the way*/
static void test_audio_pipeline_complete_connect_upstream_full(void **state)
{
	struct pipeline_connect_upstream_data *test_data = *state;
	struct pipeline *result = calloc(sizeof(struct pipeline), 1);

	*result = test_data->p;

	/*Connecting first comp to second*/
	test_data->first->is_endpoint = 0;
	test_data->second->is_endpoint = 0;
	test_data->second->comp.pipeline_id = PIPELINE_ID_SAME;
	list_item_append(&result->sched_comp->bsource_list,
		&test_data->b1->sink_list);
	test_data->b1->sink = test_data->first;
	test_data->b1->source = test_data->second;

	/*Testing component*/
	pipeline_complete(result);

	assert_int_equal(test_data->first->frames,
		result->ipc_pipe.frames_per_sched);
	assert_int_equal(test_data->second->frames,
		result->ipc_pipe.frames_per_sched);

	/*Cleanup*/
	list_init(&result->sched_comp->bsink_list);
	list_init(&result->sched_comp->bsource_list);
	list_init(&test_data->b1->source_list);
	list_init(&test_data->b1->sink_list);
	list_init(&test_data->second->bsource_list);
	list_init(&test_data->second->bsink_list);
	free(result);
}

/*Test going upstream all the way*/
static void test_audio_pipeline_complete_connect_upstream_other_pipeline
	(void **state)
{
	struct pipeline_connect_upstream_data *test_data = *state;
	struct pipeline *result = calloc(sizeof(struct pipeline), 1);

	*result = test_data->p;

	/*Connecting first comp to second*/
	test_data->first->is_endpoint = 0;
	test_data->second->is_endpoint = 0;
	test_data->second->comp.pipeline_id = PIPELINE_ID_DIFFERENT;
	list_item_append(&result->sched_comp->bsource_list,
		&test_data->b1->sink_list);
	test_data->b1->sink = test_data->first;
	test_data->b1->source = test_data->second;
	list_item_append(&test_data->second->bsource_list,
		&test_data->b1->source_list);

	/*Testing component*/
	pipeline_complete(result);

	assert_ptr_equal(test_data->first, result->source_comp);

	/*Cleanup*/
	list_init(&result->sched_comp->bsink_list);
	list_init(&result->sched_comp->bsource_list);
	list_init(&test_data->b1->source_list);
	list_init(&test_data->b1->sink_list);
	list_init(&test_data->second->bsource_list);
	list_init(&test_data->second->bsink_list);
	free(result);
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
