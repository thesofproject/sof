// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <zephyr/ztest.h>

#include <kernel/header.h>
#include <rtos/sof.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/format.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/common.h>
#include <sof/list.h>
#include <ipc/control.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <cmocka_chirp_2ch.h>

#include "drc/drc.h"
#include "drc_test_coef.h"

/* Maximum absolute value of a signed 24-bit sample. */
#define S24_MAX_ABS 0x800000

struct test_parameters {
	uint32_t channels;
	uint32_t frames;
	uint32_t buffer_size_mult;
	uint32_t source_format;
	uint32_t sink_format;
	const uint32_t *config;
	bool passthrough;
};

struct test_result {
	bool passed;
	uint32_t sample;
	int32_t output;
	int32_t expected;
};

struct test_data {
	struct comp_dev *dev;
	struct comp_buffer *sink;
	struct comp_buffer *source;
	struct test_parameters params;
	struct test_result result;
	bool continue_loop;
	int fill_idx;
	int verify_idx;
	int diff_count;
};

/**
 * @brief Create a test sink buffer connected to a component.
 */
static struct comp_buffer *create_test_sink(struct comp_dev *dev,
					    uint32_t pipeline_id,
					    uint32_t frame_fmt,
					    uint16_t channels,
					    uint16_t buffer_size)
{
	struct sof_ipc_buffer desc = {
		.comp = {
			.pipeline_id = pipeline_id,
		},
		.size = buffer_size,
	};
	struct comp_buffer *buffer = buffer_new(NULL, &desc, BUFFER_USAGE_NOT_SHARED);

	if (!buffer)
		return NULL;

	memset(buffer->stream.addr, 0, buffer_size);

	buffer->sink = calloc(1, sizeof(*buffer->sink));
	if (!buffer->sink) {
		buffer_free(buffer);
		return NULL;
	}

	if (dev)
		list_item_append(&buffer->source_list, &dev->bsink_list);

	buffer->sink->state = COMP_STATE_PREPARE;
	audio_stream_set_frm_fmt(&buffer->stream, frame_fmt);
	audio_stream_set_channels(&buffer->stream, channels);

	return buffer;
}

/**
 * @brief Release a test sink buffer and its component endpoint.
 */
static void free_test_sink(struct comp_buffer *buffer)
{
	if (!buffer)
		return;

	free(comp_buffer_get_sink_component(buffer));
	buffer_free(buffer);
}

/**
 * @brief Create a test source buffer connected to a component.
 */
static struct comp_buffer *create_test_source(struct comp_dev *dev,
					      uint32_t pipeline_id,
					      uint32_t frame_fmt,
					      uint16_t channels,
					      uint16_t buffer_size)
{
	struct sof_ipc_buffer desc = {
		.comp = {
			.pipeline_id = pipeline_id,
		},
		.size = buffer_size,
	};
	struct comp_buffer *buffer = buffer_new(NULL, &desc, BUFFER_USAGE_NOT_SHARED);

	if (!buffer)
		return NULL;

	memset(buffer->stream.addr, 0, buffer_size);

	buffer->source = calloc(1, sizeof(*buffer->source));
	if (!buffer->source) {
		buffer_free(buffer);
		return NULL;
	}

	if (dev)
		list_item_append(&buffer->sink_list, &dev->bsource_list);

	buffer->source->state = COMP_STATE_PREPARE;
	audio_stream_set_frm_fmt(&buffer->stream, frame_fmt);
	audio_stream_set_channels(&buffer->stream, channels);

	return buffer;
}

/**
 * @brief Release a test source buffer and its component endpoint.
 */
static void free_test_source(struct comp_buffer *buffer)
{
	if (!buffer)
		return;

	free(buffer->source);
	buffer_free(buffer);
}

/**
 * @brief Release all resources allocated for one DRC processing case.
 */
static void destroy_test_data(struct test_data *td)
{
	if (!td)
		return;

	free_test_source(td->source);
	free_test_sink(td->sink);
	if (td->dev)
		comp_free(td->dev);
	free(td);
}

/**
 * @brief Create the IPC description used to instantiate the DRC module.
 */
static struct sof_ipc_comp_process *create_drc_comp_ipc(void)
{
	struct sof_ipc_comp_process *ipc;
	const size_t ipc_size = sizeof(*ipc);
	const struct sof_uuid uuid = SOF_REG_UUID(drc);

	ipc = calloc(1, ipc_size + SOF_UUID_SIZE);
	if (!ipc)
		return NULL;

	memcpy_s(ipc + 1, SOF_UUID_SIZE, &uuid, SOF_UUID_SIZE);
	ipc->comp.hdr.size = ipc_size + SOF_UUID_SIZE;
	ipc->comp.type = SOF_COMP_MODULE_ADAPTER;
	ipc->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	ipc->size = 0;
	ipc->comp.ext_data_length = SOF_UUID_SIZE;

	return ipc;
}

/**
 * @brief Send one complete DRC configuration blob through the module API.
 */
static int drc_send_config(struct processing_module *mod, const uint32_t *config)
{
	const struct module_interface *const ops = mod->dev->drv->adapter_ops;
	const struct sof_abi_hdr *blob = (const struct sof_abi_hdr *)config;
	const size_t cdata_size = sizeof(struct sof_ipc_ctrl_data) +
		sizeof(struct sof_abi_hdr) + blob->size;
	struct sof_ipc_ctrl_data *cdata;
	int ret;

	cdata = calloc(1, cdata_size);
	if (!cdata)
		return -ENOMEM;

	cdata->cmd = SOF_CTRL_CMD_BINARY;
	cdata->num_elems = blob->size;
	cdata->data[0].magic = blob->magic;
	cdata->data[0].type = blob->type;
	cdata->data[0].size = blob->size;
	cdata->data[0].abi = blob->abi;
	memcpy_s(cdata->data[0].data, blob->size, blob->data, blob->size);

	ret = ops->set_configuration(mod, 0, MODULE_CFG_FRAGMENT_SINGLE,
				     blob->size, (const uint8_t *)cdata,
				     blob->size, NULL, 0);

	free(cdata);
	return ret;
}

/**
 * @brief Allocate and prepare the DRC component and its test buffers.
 */
static struct test_data *create_test_data(const struct test_parameters *params)
{
	struct test_data *td;
	struct processing_module *mod;
	struct sof_ipc_comp_process *ipc;
	struct comp_dev *dev;
	size_t src_size;
	size_t sink_size;
	int ret;

	td = calloc(1, sizeof(*td));
	if (!td)
		return NULL;

	td->params = *params;
	td->continue_loop = true;
	td->result.passed = true;

	ipc = create_drc_comp_ipc();
	if (!ipc)
		goto error;

	dev = comp_new((struct sof_ipc_comp *)ipc);
	free(ipc);
	if (!dev)
		goto error;

	td->dev = dev;
	dev->frames = params->frames;
	mod = comp_mod(dev);

	ret = drc_send_config(mod, params->config);
	if (ret)
		goto error;

	src_size = params->frames * get_frame_bytes(params->source_format, params->channels) *
		params->buffer_size_mult;
	sink_size = params->frames * get_frame_bytes(params->sink_format, params->channels) *
		params->buffer_size_mult;

	td->source = create_test_source(dev, 0, params->source_format, params->channels,
					src_size);
	td->sink = create_test_sink(dev, 0, params->sink_format, params->channels,
				    sink_size);
	if (!td->source || !td->sink)
		goto error;

	ret = module_prepare(mod, NULL, 0, NULL, 0);
	if (ret)
		goto error;

	return td;

error:
	destroy_test_data(td);
	return NULL;
}

/**
 * @brief Initialize circular-buffer views for the source and sink buffers.
 */
static void make_views(struct test_data *td, struct cir_buf_source *source_buf,
		       struct cir_buf_sink *sink_buf)
{
	struct audio_stream *ss = &td->source->stream;
	struct audio_stream *ds = &td->sink->stream;

	source_buf->buf_start = audio_stream_get_addr(ss);
	source_buf->buf_end = audio_stream_get_end_addr(ss);
	source_buf->ptr = audio_stream_get_addr(ss);

	sink_buf->buf_start = audio_stream_get_addr(ds);
	sink_buf->buf_end = audio_stream_get_end_addr(ds);
	sink_buf->ptr = audio_stream_get_addr(ds);
}

#if CONFIG_FORMAT_S16LE
/**
 * @brief Fill the S16 source buffer with test-vector samples.
 */
static int fill_source_s16(struct test_data *td, int frames)
{
	int16_t *x = audio_stream_get_addr(&td->source->stream);
	int samples = frames * td->params.channels;
	int available = CHIRP_2CH_LENGTH - td->fill_idx;
	int i;

	samples = MIN(samples, available);
	for (i = 0; i < samples; i++)
		x[i] = sat_int16(Q_SHIFT_RND(chirp_2ch[td->fill_idx++], 31, 15));

	if (td->fill_idx == CHIRP_2CH_LENGTH)
		td->continue_loop = false;

	return i / td->params.channels;
}

/**
 * @brief Verify S16 output against pass-through or processing expectations.
 */
static bool verify_sink_s16(struct test_data *td, int frames)
{
	int16_t *y = audio_stream_get_addr(&td->sink->stream);
	int samples = frames * td->params.channels;
	int i;

	for (i = 0; i < samples; i++) {
		const int32_t output = y[i];
		const int32_t expected =
			sat_int16(Q_SHIFT_RND(chirp_2ch[td->verify_idx++], 31, 15));

		if (td->params.passthrough && output != expected) {
			td->result.passed = false;
			td->result.sample = td->verify_idx - 1;
			td->result.output = output;
			td->result.expected = expected;
			return false;
		}

		if (!td->params.passthrough && output != expected)
			td->diff_count++;
	}

	return true;
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
/**
 * @brief Fill the S24 source buffer with test-vector samples.
 */
static int fill_source_s24(struct test_data *td, int frames)
{
	int32_t *x = audio_stream_get_addr(&td->source->stream);
	int samples = frames * td->params.channels;
	int available = CHIRP_2CH_LENGTH - td->fill_idx;
	int i;

	samples = MIN(samples, available);
	for (i = 0; i < samples; i++)
		x[i] = sat_int24(Q_SHIFT_RND(chirp_2ch[td->fill_idx++], 31, 23));

	if (td->fill_idx == CHIRP_2CH_LENGTH)
		td->continue_loop = false;

	return i / td->params.channels;
}

/**
 * @brief Verify S24 output range and pass-through or processing expectations.
 */
static bool verify_sink_s24(struct test_data *td, int frames)
{
	int32_t *y = audio_stream_get_addr(&td->sink->stream);
	int samples = frames * td->params.channels;
	int i;

	for (i = 0; i < samples; i++) {
		const int32_t output = (y[i] << 8) >> 8;
		const int32_t expected =
			sat_int24(Q_SHIFT_RND(chirp_2ch[td->verify_idx++], 31, 23));

		if (output >= S24_MAX_ABS || output < -S24_MAX_ABS) {
			td->result.passed = false;
			td->result.sample = td->verify_idx - 1;
			td->result.output = output;
			td->result.expected = expected;
			return false;
		}

		if (td->params.passthrough && output != expected) {
			td->result.passed = false;
			td->result.sample = td->verify_idx - 1;
			td->result.output = output;
			td->result.expected = expected;
			return false;
		}

		if (!td->params.passthrough && output != expected)
			td->diff_count++;
	}

	return true;
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/**
 * @brief Fill the S32 source buffer with test-vector samples.
 */
static int fill_source_s32(struct test_data *td, int frames)
{
	int32_t *x = audio_stream_get_addr(&td->source->stream);
	int samples = frames * td->params.channels;
	int available = CHIRP_2CH_LENGTH - td->fill_idx;
	int i;

	samples = MIN(samples, available);
	for (i = 0; i < samples; i++)
		x[i] = chirp_2ch[td->fill_idx++];

	if (td->fill_idx == CHIRP_2CH_LENGTH)
		td->continue_loop = false;

	return i / td->params.channels;
}

/**
 * @brief Verify S32 output against pass-through or processing expectations.
 */
static bool verify_sink_s32(struct test_data *td, int frames)
{
	int32_t *y = audio_stream_get_addr(&td->sink->stream);
	int samples = frames * td->params.channels;
	int i;

	for (i = 0; i < samples; i++) {
		const int32_t output = y[i];
		const int32_t expected = chirp_2ch[td->verify_idx++];

		if (td->params.passthrough && output != expected) {
			td->result.passed = false;
			td->result.sample = td->verify_idx - 1;
			td->result.output = output;
			td->result.expected = expected;
			return false;
		}

		if (!td->params.passthrough && output != expected)
			td->diff_count++;
	}

	return true;
}
#endif /* CONFIG_FORMAT_S32LE */

/**
 * @brief Fill the source buffer with the next part of the common test vector.
 */
static int fill_source(struct test_data *td, int frames)
{
	switch (td->params.source_format) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		return fill_source_s16(td, frames);
#endif
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		return fill_source_s24(td, frames);
#endif
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		return fill_source_s32(td, frames);
#endif
	default:
		td->result.passed = false;
		return 0;
	}
}

/**
 * @brief Verify one output buffer against the input vector or range limits.
 */
static bool verify_sink(struct test_data *td, int frames)
{
	switch (td->params.sink_format) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		return verify_sink_s16(td, frames);
#endif
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		return verify_sink_s24(td, frames);
#endif
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		return verify_sink_s32(td, frames);
#endif
	default:
		td->result.passed = false;
		return false;
	}
}

/**
 * @brief Run one DRC processing case over the complete two-channel vector.
 */
static bool run_drc_test(struct test_data *td)
{
	struct processing_module *mod = comp_mod(td->dev);
	struct drc_comp_data *cd = module_get_private_data(mod);
	struct cir_buf_source source_buf;
	struct cir_buf_sink sink_buf;
	int frames;

	while (td->continue_loop) {
		frames = fill_source(td, td->params.frames);
		if (frames <= 0)
			break;

		make_views(td, &source_buf, &sink_buf);
		cd->drc_func(mod, &source_buf, &sink_buf, frames);

		if (!verify_sink(td, frames))
			return false;
	}

	if (!td->params.passthrough && td->diff_count == 0)
		td->result.passed = false;

	return td->result.passed;
}

static const struct test_parameters drc_parameters[] = {
#if CONFIG_FORMAT_S16LE
	{ 2, 48, 2, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, drc_coef_pass_2ch, true },
	{ 2, 48, 2, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, drc_coef_enabled_2ch, false },
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	{ 2, 48, 2, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, drc_coef_pass_2ch, true },
	{ 2, 48, 2, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, drc_coef_enabled_2ch, false },
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	{ 2, 48, 2, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, drc_coef_pass_2ch, true },
	{ 2, 48, 2, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, drc_coef_enabled_2ch, false },
#endif /* CONFIG_FORMAT_S32LE */
};

/**
 * @brief Execute one DRC case and report failures through Ztest.
 */
static void run_drc_case(const struct test_parameters *params)
{
	struct test_data *td = create_test_data(params);
	struct test_result result;
	bool passed;

	if (!td) {
		zassert_true(false, "Failed to initialize DRC test case");
		return;
	}

	passed = run_drc_test(td);
	result = td->result;
	destroy_test_data(td);

	zassert_true(passed, "DRC output mismatch at sample %u: output %d, expected %d",
		     result.sample, result.output, result.expected);
}

/**
 * @brief Initialize the SOF component registry for the processing suite.
 */
static void *drc_process_suite_setup(void)
{
	sys_comp_init(sof_get());
	sys_comp_module_drc_interface_init();

	return NULL;
}

#if CONFIG_FORMAT_S16LE
/**
 * @brief Verify S16 DRC pass-through output.
 */
ZTEST(drc_process_suite, test_drc_process_s16_passthrough)
{
	run_drc_case(&drc_parameters[0]);
}

/**
 * @brief Verify enabled S16 DRC processing changes the signal.
 */
ZTEST(drc_process_suite, test_drc_process_s16_enabled)
{
	run_drc_case(&drc_parameters[1]);
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
/**
 * @brief Verify S24 DRC pass-through output.
 */
ZTEST(drc_process_suite, test_drc_process_s24_passthrough)
{
	run_drc_case(&drc_parameters[2]);
}

/**
 * @brief Verify enabled S24 DRC processing changes the signal.
 */
ZTEST(drc_process_suite, test_drc_process_s24_enabled)
{
	run_drc_case(&drc_parameters[3]);
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/**
 * @brief Verify S32 DRC pass-through output.
 */
ZTEST(drc_process_suite, test_drc_process_s32_passthrough)
{
	run_drc_case(&drc_parameters[4]);
}

/**
 * @brief Verify enabled S32 DRC processing changes the signal.
 */
ZTEST(drc_process_suite, test_drc_process_s32_enabled)
{
	run_drc_case(&drc_parameters[5]);
}
#endif /* CONFIG_FORMAT_S32LE */

ZTEST_SUITE(drc_process_suite, NULL, drc_process_suite_setup, NULL, NULL, NULL);
