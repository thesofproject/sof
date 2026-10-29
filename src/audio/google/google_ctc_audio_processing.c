// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Google LLC.
//
// Author: Eddy Hsu <eddyhsu@google.com>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <rtos/init.h>

#include <google_ctc_audio_processing.h>

#include "google_ctc_audio_processing.h"

LOG_MODULE_REGISTER(google_ctc_audio_processing, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(google_ctc_audio_processing);

/**
 * \brief Converts a float sample to signed 16-bit integer with clamping and NaN guard.
 * \param[in] data Float sample in range [-1.0, 1.0].
 * \return Signed 16-bit integer sample.
 */
static inline int16_t convert_float_to_int16(float data)
{
	if (data >= 0.9999f)
		return SHRT_MAX;
	if (data <= -0.9999f)
		return SHRT_MIN;
	if (data != data)
		return 0;
	return (int16_t)(data * 32767.0f);
}

/**
 * \brief Converts a signed 16-bit integer sample to float.
 * \param[in] data Signed 16-bit integer sample.
 * \return Float sample in range [-1.0, 1.0].
 */
static inline float convert_int16_to_float(int16_t data)
{
	return (float)data * (1.0f / 32768.0f);
}

/**
 * \brief Converts a float sample to signed 32-bit integer with clamping and NaN guard.
 * \param[in] data Float sample in range [-1.0, 1.0].
 * \return Signed 32-bit integer sample.
 */
static inline int32_t convert_float_to_int32(float data)
{
	if (data >= 0.999999f)
		return INT_MAX;
	if (data <= -0.999999f)
		return INT_MIN;
	if (data != data)
		return 0;
	return (int32_t)(data * 2147483647.0f);
}

/**
 * \brief Converts a signed 32-bit integer sample to float.
 * \param[in] data Signed 32-bit integer sample.
 * \return Float sample in range [-1.0, 1.0].
 */
static inline float convert_int32_to_float(int32_t data)
{
	return (float)data * (1.0f / 2147483648.0f);
}

static const int kChunkFrames = 48;
static const int kMaxChannels = 2;

/**
 * \brief Passthrough fallback when CTC processing is disabled.
 * \param[in] source Audio source stream.
 * \param[in,out] sink Audio sink stream.
 * \param[in,out] input_buffers Input stream buffers.
 * \param[in,out] output_buffers Output stream buffers.
 * \param[in] frames Number of audio frames to copy.
 */
static void ctc_passthrough(const struct audio_stream *source,
			    struct audio_stream *sink,
			    struct input_stream_buffer *input_buffers,
			    struct output_stream_buffer *output_buffers,
			    uint32_t frames)
{
	int n_ch = audio_stream_get_channels(source);
	int samples = frames * n_ch;

	audio_stream_copy(source, 0, sink, 0, samples);
	module_update_buffer_position(&input_buffers[0], &output_buffers[0], frames);
}

#if CONFIG_FORMAT_S16LE
static void ctc_s16_default(struct google_ctc_audio_processing_comp_data *cd,
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    struct input_stream_buffer *input_buffers,
			    struct output_stream_buffer *output_buffers,
			    uint32_t frames)
{
	int n_ch = audio_stream_get_channels(source);
	int samples = frames * n_ch;

	if (!cd->enabled) {
		ctc_passthrough(source, sink, input_buffers, output_buffers, frames);
		return;
	}

	int16_t *src = audio_stream_get_rptr(source);
	int16_t *dest = audio_stream_get_wptr(sink);
	int free_samples = audio_stream_get_free_samples(sink);
	int total_consumed = 0;
	int total_produced = 0;

	/* 1. Drain any leftover outputs produced in prior passes */
	while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
	       total_produced < free_samples) {
		int to_write = MIN(cd->chunk_frames * n_ch - cd->next_avail_output_samples,
				   free_samples - total_produced);
		to_write = MIN(to_write, audio_stream_samples_without_wrap_s16(sink, dest));
		for (int i = 0; i < to_write; i++)
			dest[i] = convert_float_to_int16(cd->output[cd->next_avail_output_samples++]);
		dest = audio_stream_wrap(sink, dest + to_write);
		total_produced += to_write;
	}

	/* 2. Process new input blocks only if we have room and no pending outputs */
	while (cd->next_avail_output_samples == cd->chunk_frames * n_ch &&
	       total_consumed < samples) {
		int needed = cd->chunk_frames * n_ch - cd->input_samples;
		int to_read = MIN(samples - total_consumed, needed);
		to_read = MIN(to_read, audio_stream_samples_without_wrap_s16(source, src));
		for (int i = 0; i < to_read; i++)
			cd->input[cd->input_samples++] = convert_int16_to_float(src[i]);
		src = audio_stream_wrap(source, src + to_read);
		total_consumed += to_read;

		if (cd->input_samples == cd->chunk_frames * n_ch) {
			GoogleCtcAudioProcessingProcess(cd->state, cd->input, cd->output,
							cd->chunk_frames, n_ch);
			cd->input_samples = 0;
			cd->next_avail_output_samples = 0;
			while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
			       total_produced < free_samples) {
				int to_write = MIN(cd->chunk_frames * n_ch - cd->next_avail_output_samples,
						   free_samples - total_produced);
				to_write = MIN(to_write, audio_stream_samples_without_wrap_s16(sink, dest));
				for (int i = 0; i < to_write; i++)
					dest[i] = convert_float_to_int16(cd->output[cd->next_avail_output_samples++]);
				dest = audio_stream_wrap(sink, dest + to_write);
				total_produced += to_write;
			}
		}
	}

	input_buffers->consumed = audio_stream_frame_bytes(source) * total_consumed / n_ch;
	output_buffers->size = audio_stream_frame_bytes(sink) * total_produced / n_ch;
}
#endif

#if CONFIG_FORMAT_S24LE
static void ctc_s24_default(struct google_ctc_audio_processing_comp_data *cd,
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    struct input_stream_buffer *input_buffers,
			    struct output_stream_buffer *output_buffers,
			    uint32_t frames)
{
	int n_ch = audio_stream_get_channels(source);
	int samples = frames * n_ch;

	if (!cd->enabled) {
		ctc_passthrough(source, sink, input_buffers, output_buffers, frames);
		return;
	}

	int32_t *src = audio_stream_get_rptr(source);
	int32_t *dest = audio_stream_get_wptr(sink);
	int free_samples = audio_stream_get_free_samples(sink);
	int total_consumed = 0;
	int total_produced = 0;

	/* 1. Drain any leftover outputs produced in prior passes */
	while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
	       total_produced < free_samples) {
		int to_write = MIN(cd->chunk_frames * n_ch - cd->next_avail_output_samples,
				   free_samples - total_produced);
		to_write = MIN(to_write, audio_stream_samples_without_wrap_s24(sink, dest));
		for (int i = 0; i < to_write; i++)
			dest[i] = convert_float_to_int32(cd->output[cd->next_avail_output_samples++]);
		dest = audio_stream_wrap(sink, dest + to_write);
		total_produced += to_write;
	}

	/* 2. Process new input blocks only if we have room and no pending outputs */
	while (cd->next_avail_output_samples == cd->chunk_frames * n_ch &&
	       total_consumed < samples) {
		int needed = cd->chunk_frames * n_ch - cd->input_samples;
		int to_read = MIN(samples - total_consumed, needed);
		to_read = MIN(to_read, audio_stream_samples_without_wrap_s24(source, src));
		for (int i = 0; i < to_read; i++)
			cd->input[cd->input_samples++] = convert_int32_to_float(src[i]);
		src = audio_stream_wrap(source, src + to_read);
		total_consumed += to_read;

		if (cd->input_samples == cd->chunk_frames * n_ch) {
			GoogleCtcAudioProcessingProcess(cd->state, cd->input, cd->output,
							cd->chunk_frames, n_ch);
			cd->input_samples = 0;
			cd->next_avail_output_samples = 0;
			while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
			       total_produced < free_samples) {
				int to_write = MIN(cd->chunk_frames * n_ch - cd->next_avail_output_samples,
						   free_samples - total_produced);
				to_write = MIN(to_write, audio_stream_samples_without_wrap_s24(sink, dest));
				for (int i = 0; i < to_write; i++)
					dest[i] = convert_float_to_int32(cd->output[cd->next_avail_output_samples++]);
				dest = audio_stream_wrap(sink, dest + to_write);
				total_produced += to_write;
			}
		}
	}

	input_buffers->consumed = audio_stream_frame_bytes(source) * total_consumed / n_ch;
	output_buffers->size = audio_stream_frame_bytes(sink) * total_produced / n_ch;
}
#endif

#if CONFIG_FORMAT_S32LE
static void ctc_s32_default(struct google_ctc_audio_processing_comp_data *cd,
			    const struct audio_stream *source,
			    struct audio_stream *sink,
			    struct input_stream_buffer *input_buffers,
			    struct output_stream_buffer *output_buffers,
			    uint32_t frames)
{
	int n_ch = audio_stream_get_channels(source);
	int samples = frames * n_ch;

	if (!cd->enabled) {
		ctc_passthrough(source, sink, input_buffers, output_buffers, frames);
		return;
	}

	int32_t *src = audio_stream_get_rptr(source);
	int32_t *dest = audio_stream_get_wptr(sink);
	int free_samples = audio_stream_get_free_samples(sink);
	int total_consumed = 0;
	int total_produced = 0;

	/* 1. Drain any leftover outputs produced in prior passes */
	while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
	       total_produced < free_samples) {
		int to_write = MIN(cd->chunk_frames * n_ch - cd->next_avail_output_samples,
				   free_samples - total_produced);
		to_write = MIN(to_write, audio_stream_samples_without_wrap_s32(sink, dest));
		for (int i = 0; i < to_write; i++)
			dest[i] = convert_float_to_int32(cd->output[cd->next_avail_output_samples++]);
		dest = audio_stream_wrap(sink, dest + to_write);
		total_produced += to_write;
	}

	/* 2. Process new input blocks only if we have room and no pending outputs */
	while (cd->next_avail_output_samples == cd->chunk_frames * n_ch &&
	       total_consumed < samples) {
		int needed = cd->chunk_frames * n_ch - cd->input_samples;
		int to_read = MIN(samples - total_consumed, needed);
		to_read = MIN(to_read, audio_stream_samples_without_wrap_s32(source, src));
		for (int i = 0; i < to_read; i++)
			cd->input[cd->input_samples++] = convert_int32_to_float(src[i]);
		src = audio_stream_wrap(source, src + to_read);
		total_consumed += to_read;

		if (cd->input_samples == cd->chunk_frames * n_ch) {
			GoogleCtcAudioProcessingProcess(cd->state, cd->input, cd->output,
							cd->chunk_frames, n_ch);
			cd->input_samples = 0;
			cd->next_avail_output_samples = 0;
			while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
			       total_produced < free_samples) {
				int to_write = MIN(cd->chunk_frames * n_ch - cd->next_avail_output_samples,
						   free_samples - total_produced);
				to_write = MIN(to_write, audio_stream_samples_without_wrap_s32(sink, dest));
				for (int i = 0; i < to_write; i++)
					dest[i] = convert_float_to_int32(cd->output[cd->next_avail_output_samples++]);
				dest = audio_stream_wrap(sink, dest + to_write);
				total_produced += to_write;
			}
		}
	}

	input_buffers->consumed = audio_stream_frame_bytes(source) * total_consumed / n_ch;
	output_buffers->size = audio_stream_frame_bytes(sink) * total_produced / n_ch;
}
#endif

static int ctc_free(struct processing_module *mod)
{
	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "entry");

	if (cd) {
		mod_free(mod, cd->input);
		mod_free(mod, cd->output);
		if (cd->state) {
			GoogleCtcAudioProcessingFree(cd->state);
			cd->state = NULL;
		}
		mod_data_blob_handler_free(mod, cd->tuning_handler);
		mod_free(mod, cd);
		module_set_private_data(mod, NULL);
	}

	return 0;
}

static int ctc_init(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct google_ctc_audio_processing_comp_data *cd;
	int buf_size;

	comp_info(dev, "entry");

	/* Create private component data */
	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd) {
		comp_err(dev, "Failed to create component data");
		ctc_free(mod);
		return -ENOMEM;
	}

	module_set_private_data(mod, cd);

	cd->chunk_frames = kChunkFrames;
	buf_size = cd->chunk_frames * sizeof(cd->input[0]) * kMaxChannels;

	cd->input = mod_balloc(mod, buf_size);
	if (!cd->input) {
		comp_err(dev, "Failed to allocate input buffer");
		ctc_free(mod);
		return -ENOMEM;
	}
	cd->output = mod_balloc(mod, buf_size);
	if (!cd->output) {
		comp_err(dev, "Failed to allocate output buffer");
		ctc_free(mod);
		return -ENOMEM;
	}

	cd->tuning_handler = mod_data_blob_handler_new(mod);
	if (!cd->tuning_handler) {
		comp_err(dev, "Failed to create tuning handler");
		ctc_free(mod);
		return -ENOMEM;
	}

	cd->enabled = true;

	comp_dbg(dev, "Ready");

	return 0;
}

static int google_ctc_audio_processing_reconfigure(struct processing_module *mod)
{
	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	uint8_t *config;
	size_t size;
	int ret;

	comp_dbg(dev, "entry");

	config = comp_get_data_blob(cd->tuning_handler, &size, NULL);
	if (size == 0) {
		/* No data to be handled */
		return 0;
	}

	if (!config) {
		comp_err(dev, "Tuning config not set");
		return -EINVAL;
	}

	comp_info(dev, "New tuning config %p (%zu bytes)",
		  config, size);

	cd->reconfigure = false;
	comp_info(dev,
		  "Applying config of size %zu bytes",
		  size);
	ret = GoogleCtcAudioProcessingReconfigure(cd->state, config, size);
	if (ret) {
		comp_err(dev, "GoogleCtcAudioProcessingReconfigure failed: %d",
			 ret);
		return ret;
	}

	return 0;
}

static int ctc_prepare(struct processing_module *mod,
		       struct sof_source **sources, int num_of_sources,
		       struct sof_sink **sinks, int num_of_sinks)
{
	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *source;
	int num_channels;
	uint8_t *config;
	int config_size;

	comp_info(mod->dev, "entry");

	source = comp_dev_get_first_data_producer(dev);
	if (!source) {
		comp_err(dev, "no source buffer");
		return -ENOTCONN;
	}

	switch (audio_stream_get_frm_fmt(&source->stream)) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		cd->ctc_func = ctc_s16_default;
		break;
#endif
#if CONFIG_FORMAT_S24LE
	case SOF_IPC_FRAME_S24_4LE:
		cd->ctc_func = ctc_s24_default;
		break;
#endif
#if CONFIG_FORMAT_S32LE
	case SOF_IPC_FRAME_S32_LE:
		cd->ctc_func = ctc_s32_default;
		break;
#endif
	default:
		comp_err(mod->dev, "invalid frame_fmt");
		return -EINVAL;
	}

	num_channels = audio_stream_get_channels(&source->stream);
	if (num_channels > kMaxChannels) {
		comp_err(mod->dev, "invalid number of channels");
		return -EINVAL;
	}
	cd->input_samples = 0;
	cd->next_avail_output_samples = cd->chunk_frames * num_channels;

	config = comp_get_data_blob(cd->tuning_handler, &config_size, NULL);

	if (config_size != CTC_BLOB_CONFIG_SIZE) {
		comp_info(mod->dev, "config_size not expected: %d", config_size);
		config = NULL;
		config_size = 0;
	}
	cd->state = GoogleCtcAudioProcessingCreateWithConfig(cd->chunk_frames,
							     audio_stream_get_rate(&source->stream),
							     config,
							     config_size);
	if (!cd->state) {
		comp_err(mod->dev, "failed to create CTC");
		return -ENOMEM;
	}

	return 0;
}

static int ctc_reset(struct processing_module *mod)
{
	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "entry");

	if (!cd)
		return 0;

	size_t buf_size = cd->chunk_frames * sizeof(cd->input[0]) * kMaxChannels;

	if (cd->state) {
		GoogleCtcAudioProcessingFree(cd->state);
		cd->state = NULL;
	}
	cd->ctc_func = NULL;
	cd->input_samples = 0;
	cd->next_avail_output_samples = 0;
	if (cd->input)
		memset(cd->input, 0, buf_size);
	if (cd->output)
		memset(cd->output, 0, buf_size);
	return 0;
}

static int ctc_process(struct processing_module *mod,
		       struct input_stream_buffer *input_buffers,
		       int num_input_buffers,
		       struct output_stream_buffer *output_buffers,
		       int num_output_buffers)
{
	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);
	struct audio_stream *source = input_buffers[0].data;
	struct audio_stream *sink = output_buffers[0].data;
	uint32_t frames = input_buffers[0].size;

	int ret;

	comp_dbg(mod->dev, "entry");

	if (cd->reconfigure) {
		ret = google_ctc_audio_processing_reconfigure(mod);
		if (ret)
			return ret;
	}

	if (!cd->enabled) {
		ctc_passthrough(source, sink, &input_buffers[0], &output_buffers[0], frames);
		return 0;
	}

	cd->ctc_func(cd, source, sink, &input_buffers[0], &output_buffers[0], frames);
	return 0;
}

static const struct module_interface google_ctc_audio_processing_interface = {
	.init  = ctc_init,
	.free = ctc_free,
	.process_audio_stream = ctc_process,
	.prepare = ctc_prepare,
	.set_configuration = ctc_set_config,
	.get_configuration = ctc_get_config,
	.reset = ctc_reset,
};

#if CONFIG_COMP_GOOGLE_CTC_AUDIO_PROCESSING_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("CTC", &google_ctc_audio_processing_interface,
				  1, SOF_REG_UUID(google_ctc_audio_processing), 40);

SOF_LLEXT_BUILDINFO;

#else

DECLARE_TR_CTX(google_ctc_audio_processing_tr, SOF_UUID(google_ctc_audio_processing_uuid),
	       LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(google_ctc_audio_processing_interface,
		       google_ctc_audio_processing_uuid, google_ctc_audio_processing_tr);
SOF_MODULE_INIT(google_ctc_audio_processing,
		sys_comp_module_google_ctc_audio_processing_interface_init);

#endif
