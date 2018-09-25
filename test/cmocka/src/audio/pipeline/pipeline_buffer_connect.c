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
	*state = get_standard_connect_objects();
	return 0;
}
static int teardown(void **state)
{
	free(*state);
	return 0;
}
static void test_audio_pipeline_buffer_connect_return(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	/*Testing component*/
	int err = pipeline_buffer_connect
	(
		&result,
		test_data->b2,
		test_data->first
	);

	assert_int_equal(err, 0);
}
static void test_audio_pipeline_buffer_connect_list_status(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);
	/*Testing component */
	pipeline_buffer_connect
	(
		&result,
		test_data->b2,
		test_data->first
	);

	assert_ptr_equal
	(
		test_data->b2->sink_list.next,
		&test_data->first->bsource_list
	);
}
static void test_audio_pipeline_buffer_connect_source_pointer(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);

	/*Testing component */
	pipeline_buffer_connect
	(
		&result,
		test_data->b2,
		test_data->first
	);

	assert_ptr_equal(test_data->b2->sink, test_data->first);
}
static void test_audio_pipeline_buffer_connect_sink_connected(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);
	test_data->b2->sink = test_data->second;

	/*Testing component*/
	pipeline_buffer_connect
	(
		&result,
		test_data->b2,
		test_data->first
	);

	assert_int_equal(test_data->b2->connected, 1);
}
static void test_audio_pipeline_buffer_connect_sink_not_connected(void **state)
{
	struct pipeline_connect_data *test_data = *state;
	struct pipeline result = test_data->p;

	cleanup_test_data(test_data);
	test_data->b2->connected = 0;
	test_data->b2->source = NULL;

	/*Testing component*/
	pipeline_buffer_connect
	(
		&result,
		test_data->b2,
		test_data->first
	);

	assert_int_equal(test_data->b2->connected, 0);
}
int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test
		(
			test_audio_pipeline_buffer_connect_return
		),
		cmocka_unit_test
		(
			test_audio_pipeline_buffer_connect_list_status
		),
		cmocka_unit_test
		(
			test_audio_pipeline_buffer_connect_source_pointer
		),
		cmocka_unit_test
		(
			test_audio_pipeline_buffer_connect_sink_connected
		),
		cmocka_unit_test
		(
			test_audio_pipeline_buffer_connect_sink_not_connected
		),
	};
	cmocka_set_message_output(CM_OUTPUT_TAP);
	return cmocka_run_group_tests(tests, setup, teardown);
}
