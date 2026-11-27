// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/audio/mux/mux_get_processing_function.c:
// Author: Daniel Bogdzia <danielx.bogdzia@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <zephyr/ztest.h>

#include <rtos/sof.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/lib/uuid.h>
#include <mux/mux.h>

#include <stdint.h>
#include <string.h>

#include "mux_test_util.h"

struct mux_get_processing_function_fixture {
	struct comp_dev *dev;
	struct processing_module *mod;
	struct comp_buffer *sink;
};

static struct mux_get_processing_function_fixture mux_gpf_fixture;

/**
 * @brief Register the mux driver for the suite.
 */
static void *mux_get_processing_function_setup(void)
{
	sys_comp_init(sof_get());
	sys_comp_module_mux_interface_init();

	return &mux_gpf_fixture;
}

/**
 * @brief Create a mux with all zero routing masks and one 4 byte sink buffer.
 */
static void mux_get_processing_function_before(void *data)
{
	struct mux_get_processing_function_fixture *td = data;
	const struct sof_uuid uuid = SOF_REG_UUID(mux);

	td->dev = mux_test_comp_new(&uuid, NULL);
	zassert_not_null(td->dev, "comp_new() failed");

	td->mod = comp_mod(td->dev);
	td->sink = mux_test_create_sink(td->dev, 0, 0, 0, 4);
}

/**
 * @brief Free the mux and its sink buffer.
 */
static void mux_get_processing_function_after(void *data)
{
	struct mux_get_processing_function_fixture *td = data;

	mux_test_free_sink(td->sink);
	if (td->dev)
		comp_free(td->dev);

	memset(td, 0, sizeof(*td));
}

#if CONFIG_FORMAT_FLOAT
ZTEST_F(mux_get_processing_function, test_mux_get_processing_function_valid_float)
{
	mux_func func = NULL;

	audio_stream_set_frm_fmt(&fixture->sink->stream, SOF_IPC_FRAME_FLOAT);

	func = mux_get_processing_function(fixture->mod);

	zassert_not_null(func);
}
#endif /* CONFIG_FORMAT_FLOAT */

ZTEST_F(mux_get_processing_function, test_mux_get_processing_function_invalid_format)
{
	mux_func func = NULL;

	/* set frame format value to unsupported value */
	audio_stream_set_frm_fmt(&fixture->sink->stream, SOF_IPC_FRAME_INVALID);

	func = mux_get_processing_function(fixture->mod);

	zassert_is_null(func);
}

#if CONFIG_FORMAT_S16LE
ZTEST_F(mux_get_processing_function, test_mux_get_processing_function_valid_s16le)
{
	mux_func func = NULL;

	audio_stream_set_frm_fmt(&fixture->sink->stream, SOF_IPC_FRAME_S16_LE);

	func = mux_get_processing_function(fixture->mod);

	zassert_not_null(func);
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
ZTEST_F(mux_get_processing_function, test_mux_get_processing_function_valid_s24_4le)
{
	mux_func func = NULL;

	audio_stream_set_frm_fmt(&fixture->sink->stream, SOF_IPC_FRAME_S24_4LE);

	func = mux_get_processing_function(fixture->mod);

	zassert_not_null(func);
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
ZTEST_F(mux_get_processing_function, test_mux_get_processing_function_valid_s32le)
{
	mux_func func = NULL;

	audio_stream_set_frm_fmt(&fixture->sink->stream, SOF_IPC_FRAME_S32_LE);

	func = mux_get_processing_function(fixture->mod);

	zassert_not_null(func);
}
#endif /* CONFIG_FORMAT_S32LE */

ZTEST_SUITE(mux_get_processing_function, NULL, mux_get_processing_function_setup,
	    mux_get_processing_function_before, mux_get_processing_function_after, NULL);
