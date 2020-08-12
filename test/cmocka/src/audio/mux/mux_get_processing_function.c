// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Daniel Bogdzia <danielx.bogdzia@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include "../../util.h"

#include <sof/audio/component_ext.h>
#include <sof/audio/mux.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <cmocka.h>

struct test_data {
	struct comp_dev *dev;
	struct comp_data *cd;
	struct comp_buffer *sink;
};

static int setup_group(void **state)
{
	sys_comp_init(sof_get());
	sys_comp_mux_init();

	return 0;
}

static int setup_test_case(void **state)
{
	struct test_data *td = malloc(sizeof(struct test_data));
	struct sof_ipc_comp_process ipc = {
		.comp = {
			.hdr = {
				.size = sizeof(struct sof_ipc_comp_process)
			},
			.type = SOF_COMP_MUX
		},
		.config = {
			.hdr = {
				.size = sizeof(struct sof_ipc_comp_config)
			}
		}
	};

	td->dev = comp_new((struct sof_ipc_comp *)&ipc);
	if (!td->dev)
		return -EINVAL;

	td->cd = (struct comp_data *)td->dev->priv_data;

	td->sink = create_test_sink(td->dev, 0, 0, 0, 4);

	*state = td;

	return 0;
}

static int teardown_test_case(void **state)
{
	struct test_data *td = *state;

	free_test_sink(td->sink);
	comp_free(td->dev);
	free(td);

	return 0;
}

#if CONFIG_FORMAT_FLOAT
static void test_mux_get_processing_function_invalid_float(void **state)
{
	struct test_data *td = *state;
	mux_func func = NULL;

	/* set frame format value to unsupported value */
	td->sink->stream.frame_fmt = SOF_IPC_FRAME_FLOAT;

	func = mux_get_processing_function(td->dev);

	assert_ptr_equal(func, NULL);
}
#endif /* CONFIG_FORMAT_FLOAT */

#if CONFIG_FORMAT_S16LE
static void test_mux_get_processing_function_valid_s16le(void **state)
{
	struct test_data *td = *state;
	mux_func func = NULL;

	td->sink->stream.frame_fmt = SOF_IPC_FRAME_S16_LE;

	func = mux_get_processing_function(td->dev);

	assert_ptr_not_equal(func, NULL);
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static void test_mux_get_processing_function_valid_s24_4le(void **state)
{
	struct test_data *td = *state;
	mux_func func = NULL;

	td->sink->stream.frame_fmt = SOF_IPC_FRAME_S24_4LE;

	func = mux_get_processing_function(td->dev);

	assert_ptr_not_equal(func, NULL);
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static void test_mux_get_processing_function_valid_s32le(void **state)
{
	struct test_data *td = *state;
	mux_func func = NULL;

	td->sink->stream.frame_fmt = SOF_IPC_FRAME_S32_LE;

	func = mux_get_processing_function(td->dev);

	assert_ptr_not_equal(func, NULL);
}
#endif /* CONFIG_FORMAT_S32LE */

#define TEST_CASE(name) \
	cmocka_unit_test_setup_teardown(name, \
					setup_test_case, \
					teardown_test_case)

int main(void)
{
	const struct CMUnitTest tests[] = {
#if CONFIG_FORMAT_FLOAT
		TEST_CASE(test_mux_get_processing_function_invalid_float),
#endif /* CONFIG_FORMAT_FLOAT */
#if CONFIG_FORMAT_S16LE
		TEST_CASE(test_mux_get_processing_function_valid_s16le),
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
		TEST_CASE(test_mux_get_processing_function_valid_s24_4le),
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
		TEST_CASE(test_mux_get_processing_function_valid_s32le),
#endif /* CONFIG_FORMAT_S32LE */
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup_group, NULL);
}
