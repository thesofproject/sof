// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Jakub Dabek <jakub.dabek@linux.intel.com>

#include <stdint.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/task.h>
#include "pipeline_connection_mocks.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <malloc.h>
#include <cmocka.h>

/* mock free() - dont free as we inspect contents */
void rfree(void *ptr)
{
}

static int setup(void **state)
{
	pipeline_posn_init(sof_get());
	*state = get_standard_connect_objects();
	return 0;
}

static int teardown(void **state)
{
	free(*state);
	return 0;
}

static void test_audio_pipeline_free_comp_busy(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	result.source_comp = test_data->first;
	result.sched_comp->state = 3;

	/*Testing component*/
	int err = pipeline_free(&result);

	assert_int_equal(err, -EBUSY);
}

static void test_audio_pipeline_free_return_value(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	result.source_comp = test_data->first;
	result.sched_comp->state = COMP_STATE_READY;

	/*Testing component*/
	int err = pipeline_free(&result);

	assert_int_equal(err, 0);
}

static void test_audio_pipeline_free_sheduler_task_free(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	result.source_comp = test_data->first;

	/*Testing component*/
	pipeline_free(&result);

	assert_int_equal(result.pipe_task->state, SOF_TASK_STATE_FREE);
	assert_ptr_equal(NULL, result.pipe_task->data);
	assert_ptr_equal(NULL, result.pipe_task->ops.run);
}

static void test_audio_pipeline_free_disconnect_full(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;
	struct sof_ipc_comp *first_comp;
	struct sof_ipc_comp *second_comp;

	cleanup_test_data(test_data);

	/*Set pipeline to check that is null later*/
	result.source_comp = test_data->first;
	test_data->first->pipeline = &result;
	test_data->second->pipeline = &result;
	first_comp = dev_comp(test_data->first);
	second_comp = dev_comp(test_data->second);
	second_comp->pipeline_id = PIPELINE_ID_SAME;
	first_comp->pipeline_id = PIPELINE_ID_SAME;
	test_data->b1->source = test_data->first;
	list_item_append(&result.sched_comp->bsink_list,
					 &test_data->b1->source_list);
	test_data->b1->sink = test_data->second;

	/*Testing component*/
	pipeline_free(&result);

	assert_ptr_equal(NULL, test_data->second->pipeline);
	assert_ptr_equal(NULL, test_data->first->pipeline);
}

static void test_audio_pipeline_free_disconnect_list_del
(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	result.source_comp = test_data->first;
	test_data->b1->source = test_data->first;
	list_item_append(&result.sched_comp->bsink_list,
					 &test_data->b1->source_list);
	test_data->b1->sink = test_data->second;

	/*Testing component*/
	pipeline_free(&result);

	assert_true(list_is_empty(&test_data->second->bsink_list));
	assert_true(list_is_empty(&test_data->first->bsink_list));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(
			test_audio_pipeline_free_comp_busy
		),
		cmocka_unit_test(
			test_audio_pipeline_free_return_value
		),
		cmocka_unit_test(
			test_audio_pipeline_free_sheduler_task_free
		),
		cmocka_unit_test(
			test_audio_pipeline_free_disconnect_full
		),
		cmocka_unit_test(
			test_audio_pipeline_free_disconnect_list_del
		),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup, teardown);
}
