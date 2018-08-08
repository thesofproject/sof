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

static int setup(void **state)
{
	struct pipeline_new_setup_data *pipeline_data = malloc(
		sizeof(struct pipeline_new_setup_data));

	if (pipeline_data == NULL)
		return -1;

	struct sof_ipc_pipe_new pipe_desc = { .core = 1, .priority = 2 };

	pipeline_data->ipc_data = pipe_desc;

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
	struct pipeline *result = pipeline_new(&test_data->ipc_data,
	test_data->comp_data);

	/*Pipeline should have been created so pointer can't be null*/
	assert_non_null(result);
}

static void test_audio_pipeline_new_list_initialization(void **state)
{
	struct pipeline_new_setup_data *test_data = *state;

	/*Testing component*/
	struct pipeline *result = pipeline_new(&test_data->ipc_data,
	test_data->comp_data);

	/*Check list initialization*/
	assert_ptr_equal(&result->comp_list, result->comp_list.next);
	assert_ptr_equal(&result->buffer_list, result->buffer_list.next);
}

static void test_audio_pipeline_new_sheduler_init(void **state)
{
	struct pipeline_new_setup_data *test_data = *state;

	/*Testing component*/
	struct pipeline *result = pipeline_new(&test_data->ipc_data,
	test_data->comp_data);

	/*Check if all parameters are passed to sheduler
	 *(initialization)
	 */
	assert_int_equal(result->pipe_task.state, TASK_STATE_INIT);
	assert_int_equal(result->pipe_task.core, test_data->ipc_data.core);
}

static void test_audio_pipeline_new_sheduler_config(void **state)
{
	struct pipeline_new_setup_data *test_data = *state;

	/*Testing component*/
	struct pipeline *result = pipeline_new(&test_data->ipc_data,
	test_data->comp_data);

	/*Pipeline should end in pre init state untli sheduler runs
	 *task that initializes it
	 */
	assert_int_equal(COMP_STATE_INIT, result->status);
}

static void test_audio_pipeline_new_ipc_data_coppy(void **state)
{
	struct pipeline_new_setup_data *test_data = *state;

	/*Testing component*/
	struct pipeline *result = pipeline_new(&test_data->ipc_data,
	test_data->comp_data);

	assert_memory_equal(&test_data->ipc_data, &result->ipc_pipe,
	sizeof(struct sof_ipc_pipe_new));
}

int main(void)
{

	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_audio_pipeline_pipeline_new_creation),
		cmocka_unit_test(test_audio_pipeline_new_list_initialization),
		cmocka_unit_test(test_audio_pipeline_new_sheduler_init),
		cmocka_unit_test(test_audio_pipeline_new_sheduler_config),
		cmocka_unit_test(test_audio_pipeline_new_ipc_data_coppy),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup, teardown);
}
