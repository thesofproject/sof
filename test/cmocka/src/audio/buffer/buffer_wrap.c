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

static void test_audio_buffer_write_fill_10_bytes_and_write_5(void **state)
{
	(void)state;

	struct sof_ipc_buffer test_buf_desc = {
		.size = 10
	};

	struct comp_buffer *buf = buffer_new(&test_buf_desc);

	assert_non_null(buf);
	assert_int_equal(audio_stream_get_avail_bytes(&buf->stream), 0);
	assert_int_equal(audio_stream_get_free_bytes(&buf->stream), 10);
	assert_ptr_equal(buf->stream.w_ptr, buf->stream.r_ptr);

	uint8_t bytes[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

	int i;
	uint8_t *ptr;

	for (i = 0; i < ARRAY_SIZE(bytes); i++) {
		ptr = audio_stream_write_frag(&buf->stream, i, sizeof(uint8_t));
		*ptr = bytes[i];
	}
	comp_update_buffer_produce(buf, sizeof(bytes));

	assert_int_equal(audio_stream_get_avail_bytes(&buf->stream), sizeof(bytes));
	assert_int_equal(audio_stream_get_free_bytes(&buf->stream), 0);
	assert_ptr_equal(buf->stream.w_ptr, buf->stream.r_ptr);

	uint8_t more_bytes[5] = {10, 11, 12, 13, 14};

	for (i = 0; i < ARRAY_SIZE(more_bytes); i++) {
		ptr = audio_stream_write_frag(&buf->stream, i, sizeof(uint8_t));
		*ptr = more_bytes[i];
	}
	comp_update_buffer_produce(buf, sizeof(more_bytes));

	uint8_t ref_1[5] = {5, 6, 7, 8, 9};
	uint8_t ref_2[5] = {10, 11, 12, 13, 14};

	assert_int_equal(audio_stream_get_avail_bytes(&buf->stream), 10);
	assert_int_equal(audio_stream_get_free_bytes(&buf->stream), 0);
	assert_ptr_equal(buf->stream.w_ptr, buf->stream.r_ptr);
	for (i = 0; i < ARRAY_SIZE(ref_1); i++) {
		ptr = audio_stream_read_frag(&buf->stream, i, sizeof(uint8_t));
		assert_int_equal(*ptr, ref_1[i]);
	}
	comp_update_buffer_consume(buf, sizeof(ref_1));
	for (i = 0; i < ARRAY_SIZE(ref_2); i++) {
		ptr = audio_stream_read_frag(&buf->stream, i, sizeof(uint8_t));
		assert_int_equal(*ptr, ref_2[i]);
	}

	buffer_free(buf);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test
			(test_audio_buffer_write_fill_10_bytes_and_write_5)
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
