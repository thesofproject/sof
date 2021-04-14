// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Jakub Dabek <jakub.dabek@linux.intel.com>

#include <stdint.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/schedule/schedule.h>
#include "pipeline_mocks.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <malloc.h>
#include <cmocka.h>

static int setup(void **state)
{
	struct pipeline_new_setup_data *pipeline_data = malloc(
		sizeof(struct pipeline_new_setup_data));

	pipeline_posn_init(sof_get());

	if (!pipeline_data)
		return -1;

	pipeline_data->pipe_id = 1;
	pipeline_data->priority = 2;
	pipeline_data->comp_id = 3;

	*state = pipeline_data;
	return 0;
}

static int teardown(void **state)
{
	free(*state);
	return 0;
}

static void test_audio_pipeline_pipeline_new_creation(void **state)
{
	struct pipeline_new_setup_data *test_data = *state;

	/*Testing component*/
	struct pipeline *result = pipeline_new(test_data->comp_data,
					       test_data->pipe_id,
					       test_data->priority,
					       test_data->comp_id);

	/*Pipeline should have been created so pointer can't be null*/
	assert_non_null(result);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_audio_pipeline_pipeline_new_creation),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup, teardown);
}
