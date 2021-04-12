// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/schedule.h>

#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <math.h>
#include <stdint.h>
#include <cmocka.h>

static void test_audio_buffer_new(void **state)
{
	(void)state;

	struct sof_ipc_buffer test_buf_desc = {
		.size = 256
	};

	struct comp_buffer *buf = buffer_new(&test_buf_desc);

	assert_non_null(buf);
	assert_int_equal(audio_stream_get_avail_bytes(&buf->stream), 0);
	assert_int_equal(audio_stream_get_free_bytes(&buf->stream), 256);
	assert_ptr_equal(buf->stream.w_ptr, buf->stream.r_ptr);

	buffer_free(buf);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_audio_buffer_new)
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
