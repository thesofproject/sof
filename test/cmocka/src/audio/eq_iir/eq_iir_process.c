// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <kernel/header.h>
#include <sof/audio/component_ext.h>
#include <eq_iir/eq_iir.h>
#include <sof/audio/module_adapter/module/generic.h>

#include "../../util.h"
#include "../../../include/cmocka_chirp_2ch.h"
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

struct buffer_fill {
	int idx;
} buffer_fill_data;

struct buffer_verify {
	int idx;
} buffer_verify_data;

struct test_parameters {
	uint32_t channels;
	uint32_t frames;
	uint32_t buffer_size_mult;
	uint32_t source_format;
	uint32_t sink_format;
};

struct test_data {
	struct comp_dev *dev;
	struct comp_buffer *sink;
	struct comp_buffer *source;
	struct test_parameters *params;
	struct processing_module *mod;
	bool continue_loop;
};

static int setup_group(void **state)
{
	sys_comp_init(sof_get());
	sys_comp_module_eq_iir_interface_init();
	return 0;
}

static struct sof_ipc_comp_process *create_eq_iir_comp_ipc(struct test_data *td)
{
	struct sof_ipc_comp_process *ipc;
	struct sof_eq_iir_config *eq;
	size_t ipc_size = sizeof(struct sof_ipc_comp_process);
	struct sof_abi_hdr *blob = (struct sof_abi_hdr *)iir_coef_2ch;
	const struct sof_uuid uuid = SOF_REG_UUID(eq_iir);

	ipc = calloc(1, ipc_size + blob->size + SOF_UUID_SIZE);
	memcpy_s(ipc + 1, SOF_UUID_SIZE, &uuid, SOF_UUID_SIZE);
	eq = (struct sof_eq_iir_config *)((char *)(ipc + 1) + SOF_UUID_SIZE);
	ipc->comp.hdr.size = ipc_size + SOF_UUID_SIZE;
	ipc->comp.type = SOF_COMP_MODULE_ADAPTER;
	ipc->config.hdr.size = sizeof(struct sof_ipc_comp_config);
	ipc->size = blob->size;
	ipc->comp.ext_data_length = SOF_UUID_SIZE;
	memcpy_s(eq, blob->size, blob->data, blob->size);
	return ipc;
}

static void prepare_sink(struct test_data *td, struct processing_module *mod)
{
	struct test_parameters *parameters = td->params;
	struct module_data *md = &mod->priv;
	size_t size;
	size_t free;

	/* allocate new sink buffer */
	size = parameters->frames * get_frame_bytes(parameters->sink_format, parameters->channels) *
	       parameters->buffer_size_mult;

	md->mpd.out_buff_size = parameters->frames * get_frame_bytes(parameters->sink_format,
								     parameters->channels);

	td->sink = create_test_sink(td->dev, 0, parameters->sink_format,
				    parameters->channels, size);
	free = audio_stream_get_free_bytes(&td->sink->stream);
	assert_int_equal(free, size);
}

static void prepare_source(struct test_data *td, struct processing_module *mod)
{
	struct test_parameters *parameters = td->params;
	struct module_data *md = &mod->priv;
	size_t size;
	size_t free;

	md->mpd.in_buff_size = parameters->frames * get_frame_bytes(parameters->source_format,
								     parameters->channels);

	size = parameters->frames * get_frame_bytes(parameters->source_format,
	       parameters->channels) * parameters->buffer_size_mult;

	td->source = create_test_source(td->dev, 0, parameters->source_format,
					parameters->channels, size);
	free = audio_stream_get_free_bytes(&td->source->stream);
	assert_int_equal(free, size);
}

static int setup(void **state)
{
	struct test_parameters *params = *state;
	struct processing_module *mod;
	struct test_data *td;
	struct sof_ipc_comp_process *ipc;
	struct comp_dev *dev;
	int ret;

	td = test_malloc(sizeof(*td));
	if (!td)
		return -EINVAL;

	td->params = test_malloc(sizeof(*params));
	if (!td->params)
		return -EINVAL;

	memcpy_s(td->params, sizeof(*td->params), params, sizeof(*params));
	ipc = create_eq_iir_comp_ipc(td);
	buffer_fill_data.idx = 0;
	buffer_verify_data.idx = 0;

	dev = comp_new((struct sof_ipc_comp *)ipc);
	free(ipc);
	if (!dev)
		return -EINVAL;

	td->dev = dev;
	dev->frames = params->frames;
	mod = comp_mod(dev);

	prepare_sink(td, mod);
	prepare_source(td, mod);

	/* allocate intermediate buffers */
	mod->input_buffers = test_malloc(sizeof(struct input_stream_buffer));
	mod->input_buffers[0].data = &td->source->stream;
	mod->output_buffers = test_malloc(sizeof(struct output_stream_buffer));
	mod->output_buffers[0].data = &td->sink->stream;
	mod->stream_params = test_malloc(sizeof(struct sof_ipc_stream_params));
	mod->stream_params->channels = params->channels;
	mod->period_bytes = get_frame_bytes(params->source_format, params->channels) * 48000 / 1000;

	ret = module_prepare(mod, NULL, 0, NULL, 0);
	if (ret)
		return ret;

	td->continue_loop = true;

	*state = td;
	return 0;
}

static int teardown(void **state)
{
	struct test_data *td = *state;
	struct processing_module *mod = comp_mod(td->dev);

	test_free(mod->input_buffers);
	test_free(mod->output_buffers);
	test_free(mod->stream_params);
	mod->stream_params = NULL;
	test_free(td->params);
	free_test_source(td->source);
	free_test_sink(td->sink);
	comp_free(td->dev);
	test_free(td);
	return 0;
}

#if CONFIG_FORMAT_S16LE
static void fill_source_s16(struct test_data *td, int frames_max)
{
	struct processing_module *mod = comp_mod(td->dev);
	struct comp_dev *dev = td->dev;
	struct comp_buffer *sb;
	struct audio_stream *ss;
	int16_t *x;
	int bytes_total;
	int samples;
	int frames;
	int i;
	int samples_processed = 0;

	sb = comp_dev_get_first_data_producer(dev);
	ss = &sb->stream;
	frames = MIN(audio_stream_get_free_frames(ss), frames_max);
	samples = frames * audio_stream_get_channels(ss);
	for (i = 0; i < samples; i++) {
		x = audio_stream_write_frag_s16(ss, i);
		*x = sat_int16(Q_SHIFT_RND(chirp_2ch[buffer_fill_data.idx++], 31, 15));
		samples_processed++;
		if (buffer_fill_data.idx == CHIRP_2CH_LENGTH) {
			td->continue_loop = false;
			break;
		}
	}

	if (samples_processed > 0) {
		bytes_total = samples_processed * audio_stream_sample_bytes(ss);
		comp_update_buffer_produce(sb, bytes_total);
	}

	mod->input_buffers[0].size = samples_processed / audio_stream_get_channels(ss);
}

static void verify_sink_s16(struct test_data *td)
{
	struct processing_module *mod = comp_mod(td->dev);
	struct comp_dev *dev = td->dev;
	struct comp_buffer *sb;
	struct audio_stream *ss;
	int32_t delta;
	int32_t ref;
	int32_t out;
	int16_t *x;
	int samples;
	int i;

	sb = comp_dev_get_first_data_consumer(dev);
	ss = &sb->stream;
	samples = mod->output_buffers[0].size >> 1;
	for (i = 0; i < samples; i++) {
		x = audio_stream_read_frag_s16(ss, i);
		out = *x;
		ref = sat_int16(Q_SHIFT_RND(chirp_iir_ref_2ch[buffer_verify_data.idx++], 31, 15));
		delta = ref - out;
		if (delta > ERROR_TOLERANCE_S16 || delta < -ERROR_TOLERANCE_S16)
			assert_int_equal(out, ref);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static void fill_source_s24(struct test_data *td, int frames_max)
{
	struct processing_module *mod = comp_mod(td->dev);
	struct comp_dev *dev = td->dev;
	struct comp_buffer *sb;
	struct audio_stream *ss;
	int32_t *x;
	int bytes_total;
	int samples;
	int frames;
	int i;
	int samples_processed = 0;

	sb = comp_dev_get_first_data_producer(dev);
	ss = &sb->stream;
	frames = MIN(audio_stream_get_free_frames(ss), frames_max);
	samples = frames * audio_stream_get_channels(ss);
	for (i = 0; i < samples; i++) {
		x = audio_stream_write_frag_s32(ss, i);
		*x = sat_int24(Q_SHIFT_RND(chirp_2ch[buffer_fill_data.idx++], 31, 23));
		samples_processed++;
		if (buffer_fill_data.idx == CHIRP_2CH_LENGTH) {
			td->continue_loop = false;
			break;
		}
	}

	if (samples_processed > 0) {
		bytes_total = samples_processed * audio_stream_sample_bytes(ss);
		comp_update_buffer_produce(sb, bytes_total);
	}

	mod->input_buffers[0].size = samples_processed / audio_stream_get_channels(ss);
}

static void verify_sink_s24(struct test_data *td)
{
	struct processing_module *mod = comp_mod(td->dev);
	struct comp_dev *dev = td->dev;
	struct comp_buffer *sb;
	struct audio_stream *ss;
	int32_t delta;
	int32_t ref;
	int32_t out;
	int32_t *x;
	int samples;
	int i;

	sb = comp_dev_get_first_data_consumer(dev);
	ss = &sb->stream;
	samples = mod->output_buffers[0].size >> 2;
	for (i = 0; i < samples; i++) {
		x = audio_stream_read_frag_s32(ss, i);
		out = (*x << 8) >> 8; /* Make sure there's no 24 bit overflow */
		ref = sat_int24(Q_SHIFT_RND(chirp_iir_ref_2ch[buffer_verify_data.idx++], 31, 23));
		delta = ref - out;
		if (delta > ERROR_TOLERANCE_S24 || delta < -ERROR_TOLERANCE_S24)
			assert_int_equal(out, ref);
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static void fill_source_s32(struct test_data *td, int frames_max)
{
	struct processing_module *mod = comp_mod(td->dev);
	struct comp_dev *dev = td->dev;
	struct comp_buffer *sb;
	struct audio_stream *ss;
	int32_t *x;
	int bytes_total;
	int samples;
	int frames;
	int i;
	int samples_processed = 0;

	sb = comp_dev_get_first_data_producer(dev);
	ss = &sb->stream;
	frames = MIN(audio_stream_get_free_frames(ss), frames_max);
	samples = frames * audio_stream_get_channels(ss);
	for (i = 0; i < samples; i++) {
		x = audio_stream_write_frag_s32(ss, i);
		*x = chirp_2ch[buffer_fill_data.idx++];
		samples_processed++;
		if (buffer_fill_data.idx == CHIRP_2CH_LENGTH) {
			td->continue_loop = false;
			break;
		}
	}

	if (samples_processed > 0) {
		bytes_total = samples_processed * audio_stream_sample_bytes(ss);
		comp_update_buffer_produce(sb, bytes_total);
	}

	mod->input_buffers[0].size = samples_processed / audio_stream_get_channels(ss);
}

static void verify_sink_s32(struct test_data *td)
{
	struct processing_module *mod = comp_mod(td->dev);
	struct comp_dev *dev = td->dev;
	struct comp_buffer *sb;
	struct audio_stream *ss;
	int64_t delta;
	int32_t ref;
	int32_t out;
	int32_t *x;
	int samples;
	int i;

	sb = comp_dev_get_first_data_consumer(dev);
	ss = &sb->stream;
	samples = mod->output_buffers[0].size >> 2;
	for (i = 0; i < samples; i++) {
		x = audio_stream_read_frag_s32(ss, i);
		out = *x;
		ref = chirp_iir_ref_2ch[buffer_verify_data.idx++];
		delta = (int64_t)ref - (int64_t)out;
		if (delta > ERROR_TOLERANCE_S32 || delta < -ERROR_TOLERANCE_S32)
			assert_int_equal(out, ref);
	}
}
#endif /* CONFIG_FORMAT_S32LE */

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

static void test_audio_eq_iir(void **state)
{
	struct test_data *td = *state;
	struct processing_module *mod = comp_mod(td->dev);

	struct comp_buffer *source = td->source;
	struct comp_buffer *sink = td->sink;
	int ret;
	int frames;

	while (td->continue_loop) {
		frames = frames_jitter(td->params->frames);
		switch (audio_stream_get_frm_fmt(&source->stream)) {
		case SOF_IPC_FRAME_S16_LE:
			fill_source_s16(td, frames);
			break;
		case SOF_IPC_FRAME_S24_4LE:
			fill_source_s24(td, frames);
			break;
		case SOF_IPC_FRAME_S32_LE:
			fill_source_s32(td, frames);
			break;
		case SOF_IPC_FRAME_S24_3LE:
			break;
		default:
			assert(0);
			break;
		}

		mod->input_buffers[0].consumed = 0;
		mod->output_buffers[0].size = 0;
		ret = module_process_legacy(mod, mod->input_buffers, 1,
					    mod->output_buffers, 1);
		assert_int_equal(ret, 0);

		comp_update_buffer_consume(source, mod->input_buffers[0].consumed);
		comp_update_buffer_produce(sink, mod->output_buffers[0].size);

		switch (audio_stream_get_frm_fmt(&sink->stream)) {
		case SOF_IPC_FRAME_S16_LE:
			verify_sink_s16(td);
			break;
		case SOF_IPC_FRAME_S24_4LE:
			verify_sink_s24(td);
			break;
		case SOF_IPC_FRAME_S32_LE:
			verify_sink_s32(td);
			break;
		default:
			assert(0);
			break;
		}

		comp_update_buffer_consume(sink, mod->output_buffers[0].size);
	}
}

static struct test_parameters parameters[] = {
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

int main(void)
{
	int i;

	struct CMUnitTest tests[ARRAY_SIZE(parameters)];

	for (i = 0; i < ARRAY_SIZE(parameters); i++) {
		tests[i].name = "test_audio_eq_iir";
		tests[i].test_func = test_audio_eq_iir;
		tests[i].setup_func = setup;
		tests[i].teardown_func = teardown;
		tests[i].initial_state = &parameters[i];
	}

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup_group, NULL);
}
