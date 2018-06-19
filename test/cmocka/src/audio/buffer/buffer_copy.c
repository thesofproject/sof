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
 * Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>
 */

#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include <sof/ipc.h>

#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <math.h>
#include <cmocka.h>

static void test_audio_buffer_copy_underrun(void **state)
{
	(void)state;

	struct sof_ipc_buffer test_buf_desc = {
		.size = 256
	};

	struct comp_buffer *src = buffer_new(&test_buf_desc);
	struct comp_buffer *snk = buffer_new(&test_buf_desc);

	assert_non_null(src);
	assert_non_null(snk);

	comp_update_buffer_produce(src, 10);

	assert_int_equal(src->avail, 10);
	assert_int_equal(comp_buffer_can_copy_bytes(src, snk, 16), -1);

	buffer_free(src);
	buffer_free(snk);
}

static void test_audio_buffer_copy_overrun(void **state)
{
	(void)state;

	struct sof_ipc_buffer test_buf_desc = {
		.size = 256
	};

	struct comp_buffer *src = buffer_new(&test_buf_desc);
	struct comp_buffer *snk = buffer_new(&test_buf_desc);

	assert_non_null(src);
	assert_non_null(snk);

	comp_update_buffer_produce(src, 16);
	comp_update_buffer_produce(snk, 246);

	assert_int_equal(src->avail, 16);
	assert_int_equal(snk->free, 10);
	assert_int_equal(comp_buffer_can_copy_bytes(src, snk, 16), 1);

	buffer_free(src);
	buffer_free(snk);
}

static void test_audio_buffer_copy_success(void **state)
{
	(void)state;

	struct sof_ipc_buffer test_buf_desc = {
		.size = 256
	};

	struct comp_buffer *src = buffer_new(&test_buf_desc);
	struct comp_buffer *snk = buffer_new(&test_buf_desc);

	assert_non_null(src);
	assert_non_null(snk);

	comp_update_buffer_produce(src, 10);

	assert_int_equal(src->avail, 10);
	assert_int_equal(comp_buffer_can_copy_bytes(src, snk, 0), 0);

	buffer_free(src);
	buffer_free(snk);
}

static void test_audio_buffer_copy_fit_space_constraint(void **state)
{
	(void)state;

	struct sof_ipc_buffer test_buf_desc = {
		.size = 256
	};

	struct comp_buffer *src = buffer_new(&test_buf_desc);
	struct comp_buffer *snk = buffer_new(&test_buf_desc);

	assert_non_null(src);
	assert_non_null(snk);

	comp_update_buffer_produce(src, 16);
	comp_update_buffer_produce(snk, 246);

	assert_int_equal(src->avail, 16);
	assert_int_equal(snk->free, 10);
	assert_int_equal(comp_buffer_get_copy_bytes(src, snk), 10);

	buffer_free(src);
	buffer_free(snk);
}

static void test_audio_buffer_copy_fit_no_space_constraint(void **state)
{
	(void)state;

	struct sof_ipc_buffer test_buf_desc = {
		.size = 256
	};

	struct comp_buffer *src = buffer_new(&test_buf_desc);
	struct comp_buffer *snk = buffer_new(&test_buf_desc);

	assert_non_null(src);
	assert_non_null(snk);

	comp_update_buffer_produce(src, 16);

	assert_int_equal(src->avail, 16);
	assert_int_equal(comp_buffer_get_copy_bytes(src, snk), 16);

	buffer_free(src);
	buffer_free(snk);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_audio_buffer_copy_underrun),
		cmocka_unit_test(test_audio_buffer_copy_overrun),
		cmocka_unit_test(test_audio_buffer_copy_success),
		cmocka_unit_test(test_audio_buffer_copy_fit_space_constraint),
		cmocka_unit_test(test_audio_buffer_copy_fit_no_space_constraint)
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
