// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/audio/eq_iir/eq_iir_process.c
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <zephyr/ztest.h>

#include <kernel/header.h>
#include <rtos/alloc.h>
#include <rtos/sof.h>
#include <rtos/string.h>
#include <sof/audio/audio_buffer.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/format.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/common.h>
#include <sof/list.h>
#include <ipc/control.h>
#include <user/eq.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <eq_iir/eq_iir.h>

/* Shared with the remaining CMocka tests, from test/cmocka/include */
#include <cmocka_chirp_2ch.h>

#include "cmocka_chirp_iir_ref_2ch.h"
#include "cmocka_iir_coef_2ch.h"

/* Allow some small error for fixed point. In IIR case due to float
 * reference with float coefficients the difference can be quite
 * large to scaled integer bi-quads. This could be re-visited with
 * a more implementation like reference in Octave test vector
 * generate script.
 */
#define ERROR_TOLERANCE_S16 2
#define ERROR_TOLERANCE_S24 128
#define ERROR_TOLERANCE_S32 32768

/* Thresholds for frames count jitter for rand() function */
#define THR_RAND_PLUS_ONE ((RAND_MAX >> 1) + (RAND_MAX >> 2))
#define THR_RAND_MINUS_ONE ((RAND_MAX >> 1) - (RAND_MAX >> 2))

/** @brief Stream configuration of one test case. */
struct test_parameters {
	uint32_t channels;
	uint32_t frames;
	uint32_t buffer_size_mult;
	uint32_t source_format;
	uint32_t sink_format;
};

/** @brief Component, buffers and stream positions of the running test case. */
struct eq_iir_process_fixture {
	struct comp_dev *dev;
	struct comp_buffer *sink;
	struct comp_buffer *source;
	const struct test_parameters *params;
	bool continue_loop;
	size_t input_frames;
	size_t output_bytes;
	int fill_idx;
	int verify_idx;
};

static struct eq_iir_process_fixture eq_iir_fixture;

/* Each parameter set is run through the whole chirp. */
static const struct test_parameters parameters[] = {
#if CONFIG_FORMAT_S16LE
	{ 2, 48, 2, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE },
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	{ 2, 48, 2, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE },
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	{ 2, 48, 2, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE },
#endif /* CONFIG_FORMAT_S32LE */
#if CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S16LE
	{ 2, 48, 2, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE },
#endif /* CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S24LE
	{ 2, 48, 2, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE },
#endif /* CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S24LE */
};

/**
 * @brief Look up the parameter set of a source and sink format pair.
 */
static const struct test_parameters *get_parameters(uint32_t source_format,
						    uint32_t sink_format)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(parameters); i++)
		if (parameters[i].source_format == source_format &&
		    parameters[i].sink_format == sink_format)
			return &parameters[i];

	zassert_unreachable("No parameters for formats %u -> %u", source_format, sink_format);
	return NULL;
}

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
 * @brief Create the IPC description used to instantiate the IIR EQ module.
 */
static struct sof_ipc_comp_process *create_eq_iir_comp_ipc(void)
{
	struct sof_ipc_comp_process *ipc;
	const size_t ipc_size = sizeof(*ipc);
	const struct sof_uuid uuid = SOF_REG_UUID(eq_iir);

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
 * @brief Send the reference two channel IIR EQ configuration.
 */
static int eq_iir_send_config(struct processing_module *mod)
{
	const struct module_interface *const ops = mod->dev->drv->adapter_ops;
	const struct sof_abi_hdr *blob = (const struct sof_abi_hdr *)iir_coef_2ch;
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
 * @brief Allocate the sink buffer and set the module output buffer size.
 */
static void prepare_sink(struct eq_iir_process_fixture *fx, struct processing_module *mod)
{
	const struct test_parameters *parameters = fx->params;
	struct module_data *md = &mod->priv;
	size_t size;
	size_t free_bytes;

	/* allocate new sink buffer */
	size = parameters->frames * get_frame_bytes(parameters->sink_format, parameters->channels) *
	       parameters->buffer_size_mult;

	md->mpd.out_buff_size = parameters->frames * get_frame_bytes(parameters->sink_format,
								     parameters->channels);

	fx->sink = create_test_sink(fx->dev, 0, parameters->sink_format,
				    parameters->channels, size);
	zassert_not_null(fx->sink, "Failed to create the sink buffer");
	free_bytes = audio_stream_get_free_bytes(&fx->sink->stream);
	zassert_equal(free_bytes, size);
}

/**
 * @brief Allocate the source buffer and set the module input buffer size.
 */
static void prepare_source(struct eq_iir_process_fixture *fx, struct processing_module *mod)
{
	const struct test_parameters *parameters = fx->params;
	struct module_data *md = &mod->priv;
	size_t size;
	size_t free_bytes;

	md->mpd.in_buff_size = parameters->frames * get_frame_bytes(parameters->source_format,
								     parameters->channels);

	size = parameters->frames * get_frame_bytes(parameters->source_format,
	       parameters->channels) * parameters->buffer_size_mult;

	fx->source = create_test_source(fx->dev, 0, parameters->source_format,
					parameters->channels, size);
	zassert_not_null(fx->source, "Failed to create the source buffer");
	free_bytes = audio_stream_get_free_bytes(&fx->source->stream);
	zassert_equal(free_bytes, size);
}

/**
 * @brief Create, configure and prepare the IIR EQ module for one test case.
 *
 * Anything allocated here is released by eq_iir_after(), also when an
 * assertion fails half way.
 */
static void eq_iir_test_setup(struct eq_iir_process_fixture *fx, const struct test_parameters *params)
{
	struct processing_module *mod;
	struct sof_ipc_comp_process *ipc;
	struct sof_source *sources[1];
	struct sof_sink *sinks[1];
	int ret;

	fx->params = params;
	fx->fill_idx = 0;
	fx->verify_idx = 0;

	ipc = create_eq_iir_comp_ipc();
	zassert_not_null(ipc, "Failed to allocate the component IPC");

	fx->dev = comp_new((struct sof_ipc_comp *)ipc);
	free(ipc);
	zassert_not_null(fx->dev, "Failed to create the IIR EQ component");

	fx->dev->frames = params->frames;
	mod = comp_mod(fx->dev);

	ret = eq_iir_send_config(mod);
	zassert_equal(ret, 0, "Sending the configuration failed: %d", ret);

	prepare_sink(fx, mod);
	prepare_source(fx, mod);

	/* Zeroed, CMocka test_malloc() filled all but channels with 0xba bytes */
	mod->stream_params = calloc(1, sizeof(*mod->stream_params));
	zassert_not_null(mod->stream_params, "Failed to allocate the stream parameters");
	mod->stream_params->channels = params->channels;
	mod->period_bytes = get_frame_bytes(params->source_format, params->channels) * 48000 / 1000;

	sources[0] = audio_buffer_get_source(&fx->source->audio_buffer);
	sinks[0] = audio_buffer_get_sink(&fx->sink->audio_buffer);
	ret = module_prepare(mod, sources, 1, sinks, 1);
	zassert_equal(ret, 0, "module_prepare() failed: %d", ret);

	fx->continue_loop = true;
}

#if CONFIG_FORMAT_S16LE
/**
 * @brief Write up to @p frames_max frames of the chirp to the S16 source buffer.
 */
static void fill_source_s16(struct eq_iir_process_fixture *fx, int frames_max)
{
	struct comp_buffer *sb;
	struct audio_stream *ss;
	int16_t *x;
	int bytes_total;
	int samples;
	int frames;
	int i;
	int samples_processed = 0;

	sb = comp_dev_get_first_data_producer(fx->dev);
	ss = &sb->stream;
	frames = MIN(audio_stream_get_free_frames(ss), frames_max);
	samples = frames * audio_stream_get_channels(ss);
	for (i = 0; i < samples; i++) {
		x = audio_stream_write_frag_s16(ss, i);
		*x = sat_int16(Q_SHIFT_RND(chirp_2ch[fx->fill_idx++], 31, 15));
		samples_processed++;
		if (fx->fill_idx == CHIRP_2CH_LENGTH) {
			fx->continue_loop = false;
			break;
		}
	}

	if (samples_processed > 0) {
		bytes_total = samples_processed * audio_stream_sample_bytes(ss);
		comp_update_buffer_produce(sb, bytes_total);
	}

	fx->input_frames = samples_processed / audio_stream_get_channels(ss);
}

/**
 * @brief Compare the S16 output of the last processing call to the reference.
 */
static void verify_sink_s16(struct eq_iir_process_fixture *fx)
{
	struct comp_buffer *sb;
	struct audio_stream *ss;
	int32_t delta;
	int32_t ref;
	int32_t out;
	int16_t *x;
	int samples;
	int i;

	sb = comp_dev_get_first_data_consumer(fx->dev);
	ss = &sb->stream;
	samples = fx->output_bytes >> 1;
	for (i = 0; i < samples; i++) {
		x = audio_stream_read_frag_s16(ss, i);
		out = *x;
		ref = sat_int16(Q_SHIFT_RND(chirp_iir_ref_2ch[fx->verify_idx++], 31, 15));
		delta = ref - out;
		if (delta > ERROR_TOLERANCE_S16 || delta < -ERROR_TOLERANCE_S16)
			zassert_equal(out, ref, "S16 sample %d: output %d, reference %d",
				      fx->verify_idx - 1, out, ref);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
/**
 * @brief Write up to @p frames_max frames of the chirp to the S24 source buffer.
 */
static void fill_source_s24(struct eq_iir_process_fixture *fx, int frames_max)
{
	struct comp_buffer *sb;
	struct audio_stream *ss;
	int32_t *x;
	int bytes_total;
	int samples;
	int frames;
	int i;
	int samples_processed = 0;

	sb = comp_dev_get_first_data_producer(fx->dev);
	ss = &sb->stream;
	frames = MIN(audio_stream_get_free_frames(ss), frames_max);
	samples = frames * audio_stream_get_channels(ss);
	for (i = 0; i < samples; i++) {
		x = audio_stream_write_frag_s32(ss, i);
		*x = sat_int24(Q_SHIFT_RND(chirp_2ch[fx->fill_idx++], 31, 23));
		samples_processed++;
		if (fx->fill_idx == CHIRP_2CH_LENGTH) {
			fx->continue_loop = false;
			break;
		}
	}

	if (samples_processed > 0) {
		bytes_total = samples_processed * audio_stream_sample_bytes(ss);
		comp_update_buffer_produce(sb, bytes_total);
	}

	fx->input_frames = samples_processed / audio_stream_get_channels(ss);
}

/**
 * @brief Compare the S24 output of the last processing call to the reference.
 */
static void verify_sink_s24(struct eq_iir_process_fixture *fx)
{
	struct comp_buffer *sb;
	struct audio_stream *ss;
	int32_t delta;
	int32_t ref;
	int32_t out;
	int32_t *x;
	int samples;
	int i;

	sb = comp_dev_get_first_data_consumer(fx->dev);
	ss = &sb->stream;
	samples = fx->output_bytes >> 2;
	for (i = 0; i < samples; i++) {
		x = audio_stream_read_frag_s32(ss, i);
		out = (*x << 8) >> 8; /* Make sure there's no 24 bit overflow */
		ref = sat_int24(Q_SHIFT_RND(chirp_iir_ref_2ch[fx->verify_idx++], 31, 23));
		delta = ref - out;
		if (delta > ERROR_TOLERANCE_S24 || delta < -ERROR_TOLERANCE_S24)
			zassert_equal(out, ref, "S24 sample %d: output %d, reference %d",
				      fx->verify_idx - 1, out, ref);
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/**
 * @brief Write up to @p frames_max frames of the chirp to the S32 source buffer.
 */
static void fill_source_s32(struct eq_iir_process_fixture *fx, int frames_max)
{
	struct comp_buffer *sb;
	struct audio_stream *ss;
	int32_t *x;
	int bytes_total;
	int samples;
	int frames;
	int i;
	int samples_processed = 0;

	sb = comp_dev_get_first_data_producer(fx->dev);
	ss = &sb->stream;
	frames = MIN(audio_stream_get_free_frames(ss), frames_max);
	samples = frames * audio_stream_get_channels(ss);
	for (i = 0; i < samples; i++) {
		x = audio_stream_write_frag_s32(ss, i);
		*x = chirp_2ch[fx->fill_idx++];
		samples_processed++;
		if (fx->fill_idx == CHIRP_2CH_LENGTH) {
			fx->continue_loop = false;
			break;
		}
	}

	if (samples_processed > 0) {
		bytes_total = samples_processed * audio_stream_sample_bytes(ss);
		comp_update_buffer_produce(sb, bytes_total);
	}

	fx->input_frames = samples_processed / audio_stream_get_channels(ss);
}

/**
 * @brief Compare the S32 output of the last processing call to the reference.
 */
static void verify_sink_s32(struct eq_iir_process_fixture *fx)
{
	struct comp_buffer *sb;
	struct audio_stream *ss;
	int64_t delta;
	int32_t ref;
	int32_t out;
	int32_t *x;
	int samples;
	int i;

	sb = comp_dev_get_first_data_consumer(fx->dev);
	ss = &sb->stream;
	samples = fx->output_bytes >> 2;
	for (i = 0; i < samples; i++) {
		x = audio_stream_read_frag_s32(ss, i);
		out = *x;
		ref = chirp_iir_ref_2ch[fx->verify_idx++];
		delta = (int64_t)ref - (int64_t)out;
		if (delta > ERROR_TOLERANCE_S32 || delta < -ERROR_TOLERANCE_S32)
			zassert_equal(out, ref, "S32 sample %d: output %d, reference %d",
				      fx->verify_idx - 1, out, ref);
	}
}
#endif /* CONFIG_FORMAT_S32LE */

/**
 * @brief Fill the source buffer with up to @p frames frames of the chirp.
 */
static void fill_source_for_test(struct eq_iir_process_fixture *fx, int frames)
{
	switch (audio_stream_get_frm_fmt(&fx->source->stream)) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		fill_source_s16(fx, frames);
		break;
#endif
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		fill_source_s24(fx, frames);
		break;
#endif
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		fill_source_s32(fx, frames);
		break;
#endif
	default:
		zassert_unreachable("Unsupported source format");
		break;
	}
}

/**
 * @brief Check the output of the last processing call against the reference.
 */
static void verify_sink_for_test(struct eq_iir_process_fixture *fx)
{
	switch (audio_stream_get_frm_fmt(&fx->sink->stream)) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		verify_sink_s16(fx);
		break;
#endif
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		verify_sink_s24(fx);
		break;
#endif
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		verify_sink_s32(fx);
		break;
#endif
	default:
		zassert_unreachable("Unsupported sink format");
		break;
	}
}

/**
 * @brief Randomly add or remove one frame to vary the processed block size.
 */
static int frames_jitter(int frames)
{
	int r = rand();

	if (r > THR_RAND_PLUS_ONE)
		return frames + 1;
	else if (r < THR_RAND_MINUS_ONE)
		return frames - 1;
	else
		return frames;
}

/**
 * @brief Run the whole chirp through the IIR EQ and compare to the reference.
 */
static void run_eq_iir_test(struct eq_iir_process_fixture *fx, const struct test_parameters *params)
{
	struct processing_module *mod;
	struct sof_source *sources[1];
	struct sof_sink *sinks[1];
	size_t sink_avail_before;
	int frames;
	int ret;

	eq_iir_test_setup(fx, params);
	mod = comp_mod(fx->dev);

	sources[0] = audio_buffer_get_source(&fx->source->audio_buffer);
	sinks[0] = audio_buffer_get_sink(&fx->sink->audio_buffer);

	while (fx->continue_loop) {
		frames = frames_jitter(fx->params->frames);
		fill_source_for_test(fx, frames);

		sink_avail_before = audio_stream_get_avail_bytes(&fx->sink->stream);
		ret = module_process_sink_src(mod, sources, 1, sinks, 1);
		zassert_equal(ret, 0, "module_process_sink_src() failed: %d", ret);

		fx->output_bytes = audio_stream_get_avail_bytes(&fx->sink->stream) -
			sink_avail_before;

		verify_sink_for_test(fx);

		comp_update_buffer_consume(fx->sink, fx->output_bytes);
	}
}

#if CONFIG_FORMAT_S16LE
/**
 * @brief Verify the IIR EQ S16 to S16 output against the reference.
 */
ZTEST_F(eq_iir_process, test_audio_eq_iir_s16)
{
	run_eq_iir_test(fixture, get_parameters(SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE));
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
/**
 * @brief Verify the IIR EQ S24 to S24 output against the reference.
 */
ZTEST_F(eq_iir_process, test_audio_eq_iir_s24)
{
	run_eq_iir_test(fixture, get_parameters(SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE));
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/**
 * @brief Verify the IIR EQ S32 to S32 output against the reference.
 */
ZTEST_F(eq_iir_process, test_audio_eq_iir_s32)
{
	run_eq_iir_test(fixture, get_parameters(SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE));
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S16LE
/**
 * @brief Verify the IIR EQ S32 to S16 output against the reference.
 */
ZTEST_F(eq_iir_process, test_audio_eq_iir_s32_s16)
{
	run_eq_iir_test(fixture, get_parameters(SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE));
}
#endif /* CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S24LE
/**
 * @brief Verify the IIR EQ S32 to S24 output against the reference.
 */
ZTEST_F(eq_iir_process, test_audio_eq_iir_s32_s24)
{
	run_eq_iir_test(fixture, get_parameters(SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE));
}
#endif /* CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S24LE */

/**
 * @brief Register the IIR EQ driver, once for the whole suite.
 */
static void *eq_iir_process_setup(void)
{
	sys_comp_init(sof_get());
	sys_comp_module_eq_iir_interface_init();

	return &eq_iir_fixture;
}

/**
 * @brief Start every test case without a component or buffers.
 */
static void eq_iir_before(void *data)
{
	struct eq_iir_process_fixture *fx = data;

	memset(fx, 0, sizeof(*fx));
}

/**
 * @brief Release the component and buffers of a test case.
 */
static void eq_iir_after(void *data)
{
	struct eq_iir_process_fixture *fx = data;
	struct processing_module *mod;

	if (fx->dev) {
		mod = comp_mod(fx->dev);
		free(mod->stream_params);
		mod->stream_params = NULL;
	}

	free_test_source(fx->source);
	free_test_sink(fx->sink);
	if (fx->dev)
		comp_free(fx->dev);

	memset(fx, 0, sizeof(*fx));
}

ZTEST_SUITE(eq_iir_process, NULL, eq_iir_process_setup, eq_iir_before, eq_iir_after, NULL);

/**
 * @brief Verify the DF1 delay line size for valid and invalid series counts.
 */
ZTEST(eq_iir_validator, test_iir_delay_size_df1_series)
{
	struct sof_eq_iir_header hdr = { .num_sections = 11 };
	const size_t size = 4 * 11 * sizeof(int32_t);

	hdr.num_sections_in_series = 11;
	zassert_equal(iir_delay_size_df1(&hdr), size);

	hdr.num_sections_in_series = 1;
	zassert_equal(iir_delay_size_df1(&hdr), size);

	/* iir_df1() would run 20 biquads against 11 sections of state */
	hdr.num_sections_in_series = 10;
	zassert_equal(iir_delay_size_df1(&hdr), 0);
}

ZTEST_SUITE(eq_iir_validator, NULL, NULL, NULL, NULL, NULL);
