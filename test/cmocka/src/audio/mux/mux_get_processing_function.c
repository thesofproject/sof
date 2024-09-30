// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Daniel Bogdzia <danielx.bogdzia@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include "../../util.h"

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/component_ext.h>
#include <mux/mux.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <cmocka.h>

struct test_data {
	struct comp_dev *dev;
	struct processing_module *mod;
	struct comp_data *cd;
	struct comp_buffer *sink;
};

static int setup_group(void **state)
{
	sys_comp_init(sof_get());
	sys_comp_module_mux_interface_init();

	return 0;
}

static struct sof_ipc_comp_process *create_mux_comp_ipc(struct test_data *td)
{
	struct sof_ipc_comp_process *ipc;
	struct sof_mux_config *mux;
	size_t ipc_size = sizeof(struct sof_ipc_comp_process);
	size_t mux_size = sizeof(struct sof_mux_config) +
		MUX_MAX_STREAMS * sizeof(struct mux_stream_data);
	const struct sof_uuid uuid = {
		.a = 0xc607ff4d, .b = 0x9cb6, .c = 0x49dc,
		.d = {0xb6, 0x78, 0x7d, 0xa3, 0xc6, 0x3e, 0xa5, 0x57}
	};
	int i, j;

	ipc = calloc(1, ipc_size + mux_size + SOF_UUID_SIZE);
	memcpy_s(ipc + 1, SOF_UUID_SIZE, &uuid, SOF_UUID_SIZE);
	mux = (struct sof_mux_config *)((char *)(ipc + 1) + SOF_UUID_SIZE);
	ipc->comp.hdr.size = ipc_size + SOF_UUID_SIZE;
	ipc->comp.type = SOF_COMP_MODULE_ADAPTER;
	ipc->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	ipc->size = mux_size;
	ipc->comp.ext_data_length = SOF_UUID_SIZE;

	mux->num_streams = MUX_MAX_STREAMS;
	for (i = 0; i < MUX_MAX_STREAMS; ++i) {
		mux->streams[i].pipeline_id = i;
		for (j = 0; j < PLATFORM_MAX_CHANNELS; ++j)
			mux->streams[i].mask[j] = 0;
	}

	return ipc;
}


static int setup_test_case(void **state)
{
	struct test_data *td;
	struct comp_dev *dev;
	struct processing_module *mod;
	struct sof_ipc_comp_process *ipc;

	td = test_malloc(sizeof(*td));
	if (!td)
		return -ENOMEM;

	ipc = create_mux_comp_ipc(td);
	dev = comp_new((struct sof_ipc_comp *)ipc);
	free(ipc);
	if (!dev)
		return -EINVAL;

	mod = comp_mod(dev);
	td->dev = dev;
	td->mod = mod;
	td->cd = module_get_private_data(mod);
	td->sink = create_test_sink(dev, 0, 0, 0, 4);

	*state = td;
	return 0;
}

static int teardown_test_case(void **state)
{
	struct test_data *td = *state;

	free_test_sink(td->sink);
	comp_free(td->dev);
	test_free(td);

	return 0;
}

#if CONFIG_FORMAT_FLOAT
static void test_mux_get_processing_function_invalid_float(void **state)
{
	struct test_data *td = *state;
	mux_func func = NULL;

	/* set frame format value to unsupported value */
	audio_stream_set_frm_fmt(&td->sink->stream, SOF_IPC_FRAME_FLOAT);

	func = mux_get_processing_function(td->mod);

	assert_ptr_equal(func, NULL);
}
#endif /* CONFIG_FORMAT_FLOAT */

#if CONFIG_FORMAT_S16LE
static void test_mux_get_processing_function_valid_s16le(void **state)
{
	struct test_data *td = *state;
	mux_func func = NULL;

	audio_stream_set_frm_fmt(&td->sink->stream, SOF_IPC_FRAME_S16_LE);

	func = mux_get_processing_function(td->mod);

	assert_ptr_not_equal(func, NULL);
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static void test_mux_get_processing_function_valid_s24_4le(void **state)
{
	struct test_data *td = *state;
	mux_func func = NULL;

	audio_stream_set_frm_fmt(&td->sink->stream, SOF_IPC_FRAME_S24_4LE);

	func = mux_get_processing_function(td->mod);

	assert_ptr_not_equal(func, NULL);
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static void test_mux_get_processing_function_valid_s32le(void **state)
{
	struct test_data *td = *state;
	mux_func func = NULL;

	audio_stream_set_frm_fmt(&td->sink->stream, SOF_IPC_FRAME_S32_LE);

	func = mux_get_processing_function(td->mod);

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
