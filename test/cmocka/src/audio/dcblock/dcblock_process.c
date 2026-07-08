// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <cmocka.h>
#include <kernel/header.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/audio_buffer.h>
#include <sof/audio/source_api.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/format.h>
#include <ipc/control.h>

#include "dcblock.h"

#include "../../util.h"
#include "../../../include/cmocka_chirp_2ch.h"

/* Allow a few LSB of error due to the internal Q-format rounding. */
#define ERROR_TOLERANCE_S16	2
#define ERROR_TOLERANCE_S24	4
#define ERROR_TOLERANCE_S32	2048

/* Q2.30 DC blocking coefficient (~0.98) used for all channels. */
#define R_COEF	1052266987

/* Thresholds for frames count jitter for rand() function */
#define THR_RAND_PLUS_ONE ((RAND_MAX >> 1) + (RAND_MAX >> 2))
#define THR_RAND_MINUS_ONE ((RAND_MAX >> 1) - (RAND_MAX >> 2))

struct buffer_state {
	int idx;
};

static struct buffer_state buffer_fill_data;
static struct buffer_state buffer_verify_data;

/* Floating point reference filter state, one per channel. */
struct ref_state {
	double x_prev;
	double y_prev;
};

static struct ref_state ref_states[PLATFORM_MAX_CHANNELS];

static double dcblock_ref(int ch, double x)
{
	struct ref_state *s = &ref_states[ch];
	double r = (double)R_COEF / (double)ONE_Q2_30;
	double y = x - s->x_prev + r * s->y_prev;

	/* The fixed point filter saturates the Q1.31 state after every
	 * sample (sat_int32). The DC blocker has a passband gain slightly
	 * above one, so without this clamp the reference state would drift
	 * away from the implementation for near full-scale input.
	 */
	if (y > 2147483647.0 / 2147483648.0)
		y = 2147483647.0 / 2147483648.0;
	else if (y < -1.0)
		y = -1.0;

	s->x_prev = x;
	s->y_prev = y;
	return y;
}

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
	sys_comp_module_dcblock_interface_init();
	return 0;
}

static struct sof_ipc_comp_process *create_dcblock_comp_ipc(struct test_data *td)
{
	struct sof_ipc_comp_process *ipc;
	size_t ipc_size = sizeof(struct sof_ipc_comp_process);
	const struct sof_uuid uuid = SOF_REG_UUID(dcblock);

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

/* Send a configuration blob holding the per-channel R coefficients. */
static int dcblock_send_config(struct processing_module *mod)
{
	const size_t coeff_size = PLATFORM_MAX_CHANNELS * sizeof(int32_t);
	size_t cdata_size = sizeof(struct sof_ipc_ctrl_data) +
			    sizeof(struct sof_abi_hdr) + coeff_size;
	struct sof_ipc_ctrl_data *cdata;
	int32_t coeffs[PLATFORM_MAX_CHANNELS];
	int ret;
	int i;

	cdata = calloc(1, cdata_size);
	if (!cdata)
		return -ENOMEM;

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++)
		coeffs[i] = R_COEF;

	cdata->cmd = SOF_CTRL_CMD_BINARY;
	cdata->num_elems = coeff_size;
	cdata->data[0].magic = SOF_ABI_MAGIC;
	cdata->data[0].type = 0;
	cdata->data[0].size = coeff_size;
	cdata->data[0].abi = SOF_ABI_VERSION;
	memcpy_s(cdata->data[0].data, coeff_size, coeffs, coeff_size);

	ret = module_adapter_cmd(mod->dev, COMP_CMD_SET_DATA, cdata, cdata_size);

	free(cdata);
	return ret;
}

static void prepare_sink(struct test_data *td, struct processing_module *mod)
{
	struct test_parameters *parameters = td->params;
	struct module_data *md = &mod->priv;
	size_t size;
	size_t free;

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

static void cleanup_test_data(struct test_data *td)
{
	struct processing_module *mod;

	if (!td)
		return;

	if (td->dev) {
		mod = comp_mod(td->dev);
		if (mod) {
			test_free(mod->input_buffers);
			mod->input_buffers = NULL;
			test_free(mod->output_buffers);
			mod->output_buffers = NULL;
			test_free(mod->stream_params);
			mod->stream_params = NULL;
		}
	}

	if (td->source) {
		free_test_source(td->source);
		td->source = NULL;
	}

	if (td->sink) {
		free_test_sink(td->sink);
		td->sink = NULL;
	}

	if (td->dev) {
		comp_free(td->dev);
		td->dev = NULL;
	}

	test_free(td->params);
	test_free(td);
}

static int setup(void **state)
{
	struct test_parameters *params = *state;
	struct processing_module *mod;
	struct test_data *td;
	struct sof_ipc_comp_process *ipc = NULL;
	struct comp_dev *dev;
	struct sof_source *sources[1];
	struct sof_sink *sinks[1];
	int ret;
	int i;

	td = test_malloc(sizeof(*td));
	if (!td) {
		*state = NULL;
		return -EINVAL;
	}

	*td = (struct test_data){ 0 };
	*state = td;
	td->params = test_malloc(sizeof(*params));
	if (!td->params) {
		ret = -ENOMEM;
		goto error;
	}

	memcpy_s(td->params, sizeof(*td->params), params, sizeof(*params));
	ipc = create_dcblock_comp_ipc(td);
	if (!ipc) {
		ret = -ENOMEM;
		goto error;
	}

	buffer_fill_data.idx = 0;
	buffer_verify_data.idx = 0;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
		ref_states[i].x_prev = 0.0;
		ref_states[i].y_prev = 0.0;
	}

	dev = comp_new((struct sof_ipc_comp *)ipc);
	free(ipc);
	ipc = NULL;
	if (!dev) {
		ret = -EINVAL;
		goto error;
	}

	td->dev = dev;
	dev->frames = params->frames;
	mod = comp_mod(dev);

	ret = dcblock_send_config(mod);
	if (ret)
		goto error;

	prepare_sink(td, mod);
	prepare_source(td, mod);

	mod->input_buffers = test_malloc(sizeof(struct input_stream_buffer));
	if (!mod->input_buffers) {
		ret = -ENOMEM;
		goto error;
	}
	mod->input_buffers[0].data = &td->source->stream;
	mod->output_buffers = test_malloc(sizeof(struct output_stream_buffer));
	if (!mod->output_buffers) {
		ret = -ENOMEM;
		goto error;
	}
	mod->output_buffers[0].data = &td->sink->stream;
	mod->stream_params = test_malloc(sizeof(struct sof_ipc_stream_params));
	if (!mod->stream_params) {
		ret = -ENOMEM;
		goto error;
	}
	mod->stream_params->channels = params->channels;
	mod->period_bytes = get_frame_bytes(params->source_format, params->channels) * 48000 / 1000;

	sources[0] = audio_buffer_get_source(&td->source->audio_buffer);
	sinks[0] = audio_buffer_get_sink(&td->sink->audio_buffer);
	ret = module_prepare(mod, sources, 1, sinks, 1);
	if (ret)
		goto error;

	td->continue_loop = true;

	*state = td;
	return 0;

error:
	free(ipc);
	cleanup_test_data(td);
	*state = NULL;
	return ret;
}

static int teardown(void **state)
{
	struct test_data *td = *state;

	cleanup_test_data(td);
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
	int nch = td->params->channels;
	int32_t delta;
	int32_t ref;
	int32_t out;
	int16_t *x;
	int16_t in;
	int samples;
	int i;

	sb = comp_dev_get_first_data_consumer(dev);
	ss = &sb->stream;
	samples = mod->output_buffers[0].size >> 1;
	for (i = 0; i < samples; i++) {
		x = audio_stream_read_frag_s16(ss, i);
		out = *x;
		in = sat_int16(Q_SHIFT_RND(chirp_2ch[buffer_verify_data.idx], 31, 15));
		ref = (int32_t)round(dcblock_ref(buffer_verify_data.idx % nch,
						 (double)in / 32768.0) * 32768.0);
		ref = sat_int16(ref);
		buffer_verify_data.idx++;
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
	int nch = td->params->channels;
	int32_t delta;
	int32_t ref;
	int32_t out;
	int32_t *x;
	int32_t in;
	int samples;
	int i;

	sb = comp_dev_get_first_data_consumer(dev);
	ss = &sb->stream;
	samples = mod->output_buffers[0].size >> 2;
	for (i = 0; i < samples; i++) {
		x = audio_stream_read_frag_s32(ss, i);
		out = (*x << 8) >> 8; /* Make sure there's no 24 bit overflow */
		in = sat_int24(Q_SHIFT_RND(chirp_2ch[buffer_verify_data.idx], 31, 23));
		ref = (int32_t)round(dcblock_ref(buffer_verify_data.idx % nch,
						 (double)in / 8388608.0) * 8388608.0);
		ref = sat_int24(ref);
		buffer_verify_data.idx++;
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
	int nch = td->params->channels;
	int64_t delta;
	int32_t ref;
	int32_t out;
	int32_t *x;
	int32_t in;
	int samples;
	int i;

	sb = comp_dev_get_first_data_consumer(dev);
	ss = &sb->stream;
	samples = mod->output_buffers[0].size >> 2;
	for (i = 0; i < samples; i++) {
		x = audio_stream_read_frag_s32(ss, i);
		out = *x;
		in = chirp_2ch[buffer_verify_data.idx];
		ref = (int32_t)round(dcblock_ref(buffer_verify_data.idx % nch,
						 (double)in / 2147483648.0) * 2147483648.0);
		buffer_verify_data.idx++;
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

static void test_audio_dcblock(void **state)
{
	struct test_data *td = *state;
	struct processing_module *mod = comp_mod(td->dev);
	struct comp_buffer *source = td->source;
	struct comp_buffer *sink = td->sink;
	struct sof_source *sources[1];
	struct sof_sink *sinks[1];
	uint32_t avail_before;
	int ret;
	int frames;

	sources[0] = audio_buffer_get_source(&source->audio_buffer);
	sinks[0] = audio_buffer_get_sink(&sink->audio_buffer);

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
		default:
			assert(0);
			break;
		}

		/* Process exactly the frames just filled. The source/sink API
		 * updates the buffer read/write positions internally, so record
		 * the sink fill level to know how many bytes were produced.
		 */
		td->dev->frames = mod->input_buffers[0].size;
		avail_before = audio_stream_get_avail_bytes(&sink->stream);
		ret = module_process_sink_src(mod, sources, 1, sinks, 1);
		assert_int_equal(ret, 0);
		mod->output_buffers[0].size =
			audio_stream_get_avail_bytes(&sink->stream) - avail_before;

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
};

int main(void)
{
	int i;

	struct CMUnitTest tests[ARRAY_SIZE(parameters)];

	for (i = 0; i < ARRAY_SIZE(parameters); i++) {
		tests[i].name = "test_audio_dcblock";
		tests[i].test_func = test_audio_dcblock;
		tests[i].setup_func = setup;
		tests[i].teardown_func = teardown;
		tests[i].initial_state = &parameters[i];
	}

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup_group, NULL);
}
