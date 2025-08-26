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
#include <rtos/task.h>
#include "pipeline_connection_mocks.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#else
#include <stdlib.h>
#endif

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
	free_standard_connect_objects(*state);
	free(*state);
	return 0;
}

static void test_audio_pipeline_free_return_value(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	/*Testing component*/
	int err = pipeline_free(&result);

	assert_int_equal(err, 0);
}

static void test_audio_pipeline_free_sheduler_task_free(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	/*Testing component*/
	pipeline_free(&result);

	assert_int_equal(result.pipe_task->state, SOF_TASK_STATE_FREE);
	assert_ptr_equal(NULL, result.pipe_task->data);
	assert_ptr_equal(NULL, result.pipe_task->ops.run);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(
			test_audio_pipeline_free_return_value
		),
		cmocka_unit_test(
			test_audio_pipeline_free_sheduler_task_free
		),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup, teardown);
}
