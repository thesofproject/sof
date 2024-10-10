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

DECLARE_TR_CTX(google_ctc_audio_processing_tr, SOF_UUID(google_ctc_audio_processing_uuid),
	       LOG_LEVEL_INFO);

// TODO(eddyhsu): Share these utils function with RTC.
static inline float clamp_rescale(float max_val, float x)
{
	float min = -1.0f;
	float max = 1.0f - 1.0f / max_val;

	return max_val * (x < min ? min : (x > max ? max : x));
}

static inline int16_t convert_float_to_int16(float data)
{
	return (int16_t)clamp_rescale(-(float)SHRT_MIN, data);
}

static inline float convert_int16_to_float(int16_t data)
{
	float scale = -(float)SHRT_MIN;

	return (1.0f / scale) * data;
}

static inline int32_t convert_float_to_int32(float data)
{
	return (int32_t)clamp_rescale(-(float)INT_MIN, data);
}

static inline float convert_int32_to_float(int32_t data)
{
	float scale = -(float)INT_MIN;

	return (1.0f / scale) * data;
}

static const int kChunkFrames = 48;
static const int kMaxChannels = 2;

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

	int16_t *src = audio_stream_get_rptr(source);
	int16_t *dest = audio_stream_get_wptr(sink);

	int samples_to_process = MIN(samples, audio_stream_samples_without_wrap_s16(source, src));
	int samples_to_written = MIN(samples, audio_stream_samples_without_wrap_s16(sink, dest));
	int written_samples = 0;

	if (!cd->enabled) {
		ctc_passthrough(source, sink, input_buffers, output_buffers, frames);
		return;
	}

	// writes previous processed samples to the output.
	while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
	       written_samples < samples_to_written) {
		dest[written_samples++] =
			convert_float_to_int16(cd->output[cd->next_avail_output_samples]);
		cd->next_avail_output_samples++;
	}
	for (int i = 0; i < samples_to_process; ++i) {
		cd->input[cd->input_samples++] = convert_int16_to_float(src[i]);
		if (cd->input_samples == cd->chunk_frames * n_ch) {
			GoogleCtcAudioProcessingProcess(cd->state, cd->input, cd->output,
							cd->chunk_frames, n_ch);
			cd->input_samples = 0;
			cd->next_avail_output_samples = 0;
			// writes processed samples to the output.
			while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
			       written_samples < samples_to_written) {
				dest[written_samples++] =
					convert_float_to_int16(cd->output[cd->next_avail_output_samples]);
				cd->next_avail_output_samples++;
			}
		}
	}
	if (written_samples > 0) {
		dest = audio_stream_wrap(sink, dest + written_samples);
		output_buffers->size += audio_stream_frame_bytes(sink) * written_samples / n_ch;
	}
	src = audio_stream_wrap(source, src + samples_to_process);
	input_buffers->consumed += audio_stream_frame_bytes(source) * samples_to_process / n_ch;
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

	int32_t *src = audio_stream_get_rptr(source);
	int32_t *dest = audio_stream_get_wptr(sink);

	int samples_to_process = MIN(samples, audio_stream_samples_without_wrap_s24(source, src));
	int samples_to_written = MIN(samples, audio_stream_samples_without_wrap_s24(sink, dest));
	int written_samples = 0;

	if (!cd->enabled) {
		ctc_passthrough(source, sink, input_buffers, output_buffers, frames);
		return;
	}

	// writes previous processed samples to the output.
	while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
	       written_samples < samples_to_written) {
		dest[written_samples++] =
			convert_float_to_int32(cd->output[cd->next_avail_output_samples]);
		cd->next_avail_output_samples++;
	}
	for (int i = 0; i < samples_to_process; ++i) {
		cd->input[cd->input_samples++] = convert_int32_to_float(src[i]);
		if (cd->input_samples == cd->chunk_frames * n_ch) {
			GoogleCtcAudioProcessingProcess(cd->state, cd->input, cd->output,
							cd->chunk_frames, n_ch);
			cd->input_samples = 0;
			cd->next_avail_output_samples = 0;
			// writes processed samples to the output.
			while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
			       written_samples < samples_to_written) {
				dest[written_samples++] =
					convert_float_to_int32(cd->output[cd->next_avail_output_samples]);
				cd->next_avail_output_samples++;
			}
		}
	}
	if (written_samples > 0) {
		dest = audio_stream_wrap(sink, dest + written_samples);
		output_buffers->size += audio_stream_frame_bytes(sink) * written_samples / n_ch;
	}
	src = audio_stream_wrap(source, src + samples_to_process);
	input_buffers->consumed += audio_stream_frame_bytes(source) * samples_to_process / n_ch;
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

	int32_t *src = audio_stream_get_rptr(source);
	int32_t *dest = audio_stream_get_wptr(sink);

	int samples_to_process = MIN(samples, audio_stream_samples_without_wrap_s32(source, src));
	int samples_to_written = MIN(samples, audio_stream_samples_without_wrap_s32(sink, dest));
	int written_samples = 0;

	if (!cd->enabled) {
		ctc_passthrough(source, sink, input_buffers, output_buffers, frames);
		return;
	}

	// writes previous processed samples to the output.
	while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
	       written_samples < samples_to_written) {
		dest[written_samples++] =
			convert_float_to_int32(cd->output[cd->next_avail_output_samples]);
		cd->next_avail_output_samples++;
	}
	for (int i = 0; i < samples_to_process; ++i) {
		cd->input[cd->input_samples++] = convert_int32_to_float(src[i]);
		if (cd->input_samples == cd->chunk_frames * n_ch) {
			GoogleCtcAudioProcessingProcess(cd->state, cd->input, cd->output,
							cd->chunk_frames, n_ch);
			cd->input_samples = 0;
			cd->next_avail_output_samples = 0;
			// writes processed samples to the output.
			while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
			       written_samples < samples_to_written) {
				dest[written_samples++] =
					convert_float_to_int32(cd->output[cd->next_avail_output_samples]);
				cd->next_avail_output_samples++;
			}
		}
	}
	if (written_samples > 0) {
		dest = audio_stream_wrap(sink, dest + written_samples);
		output_buffers->size += audio_stream_frame_bytes(sink) * written_samples / n_ch;
	}
	src = audio_stream_wrap(source, src + samples_to_process);
	input_buffers->consumed += audio_stream_frame_bytes(source) * samples_to_process / n_ch;
}
#endif

static int ctc_free(struct processing_module *mod)
{
	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "ctc_free()");

	if (cd) {
		rfree(cd->input);
		rfree(cd->output);
		GoogleCtcAudioProcessingFree(cd->state);
		rfree(cd);
		module_set_private_data(mod, NULL);
	}

	return 0;
}

static int ctc_init(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct google_ctc_audio_processing_comp_data *cd;
	int buf_size;

	comp_info(dev, "ctc_init()");

	/* Create private component data */
	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		comp_err(dev, "ctc_init(): Failed to create component data");
		ctc_free(mod);
		return -ENOMEM;
	}

	module_set_private_data(mod, cd);

	cd->chunk_frames = kChunkFrames;
	buf_size = cd->chunk_frames * sizeof(cd->input[0]) * kMaxChannels;

	cd->input = rballoc(0, SOF_MEM_CAPS_RAM, buf_size);
	if (!cd->input) {
		comp_err(dev, "ctc_init(): Failed to allocate input buffer");
		ctc_free(mod);
		return -ENOMEM;
	}
	cd->output = rballoc(0, SOF_MEM_CAPS_RAM, buf_size);
	if (!cd->output) {
		comp_err(dev, "ctc_init(): Failed to allocate output buffer");
		ctc_free(mod);
		return -ENOMEM;
	}

	cd->tuning_handler = comp_data_blob_handler_new(dev);
	if (!cd->tuning_handler) {
		comp_err(dev, "ctc_init(): Failed to create tuning handler");
		ctc_free(mod);
		return -ENOMEM;
	}

	cd->enabled = true;

	comp_dbg(dev, "ctc_init(): Ready");

	return 0;
}

static int google_ctc_audio_processing_reconfigure(struct processing_module *mod)
{
	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	uint8_t *config;
	size_t size;
	int ret;

	comp_dbg(dev, "google_ctc_audio_processing_reconfigure()");

	config = comp_get_data_blob(cd->tuning_handler, &size, NULL);
	if (size == 0) {
		/* No data to be handled */
		return 0;
	}

	if (!config) {
		comp_err(dev, "google_ctc_audio_processing_reconfigure(): Tuning config not set");
		return -EINVAL;
	}

	comp_info(dev, "google_ctc_audio_processing_reconfigure(): New tuning config %p (%zu bytes)",
		  config, size);

	cd->reconfigure = false;
	comp_info(dev,
		  "google_ctc_audio_processing_reconfigure(): Applying config of size %zu bytes",
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

	comp_info(mod->dev, "ctc_prepare()");

	source = comp_dev_get_first_data_producer(dev);
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
		comp_err(mod->dev, "ctc_prepare(), invalid frame_fmt");
		return -EINVAL;
	}

	num_channels = audio_stream_get_channels(&source->stream);
	if (num_channels > kMaxChannels) {
		comp_err(mod->dev, "ctc_prepare(), invalid number of channels");
		return -EINVAL;
	}
	cd->next_avail_output_samples = cd->chunk_frames * num_channels;

	config = comp_get_data_blob(cd->tuning_handler, &config_size, NULL);

	if (config_size != CTC_BLOB_CONFIG_SIZE) {
		comp_info(mod->dev, "ctc_prepare(): config_size not expected: %d", config_size);
		config = NULL;
		config_size = 0;
	}
	cd->state = GoogleCtcAudioProcessingCreateWithConfig(cd->chunk_frames,
							     audio_stream_get_rate(&source->stream),
							     config,
							     config_size);
	if (!cd->state) {
		comp_err(mod->dev, "ctc_prepare(), failed to create CTC");
		return -ENOMEM;
	}

	return 0;
}

static int ctc_reset(struct processing_module *mod)
{
	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);
	size_t buf_size = cd->chunk_frames * sizeof(cd->input[0]) * kMaxChannels;

	comp_info(mod->dev, "ctc_reset()");

	GoogleCtcAudioProcessingFree(cd->state);
	cd->state = NULL;
	cd->ctc_func = NULL;
	cd->input_samples = 0;
	cd->next_avail_output_samples = 0;
	memset(cd->input, 0, buf_size);
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

	comp_dbg(mod->dev, "ctc_process()");

	if (cd->reconfigure) {
		ret = google_ctc_audio_processing_reconfigure(mod);
		if (ret)
			return ret;
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

DECLARE_MODULE_ADAPTER(google_ctc_audio_processing_interface,
		       google_ctc_audio_processing_uuid, google_ctc_audio_processing_tr);
SOF_MODULE_INIT(google_ctc_audio_processing,
		sys_comp_module_google_ctc_audio_processing_interface_init);

#if CONFIG_COMP_GOOGLE_CTC_AUDIO_PROCESSING_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

#define UUID_GOOGLE_CTC 0xBC, 0x1B, 0x0E, 0xBF, 0x6A, 0xDC, 0xFE, 0x45, 0x90, 0xBC, \
	 0x25, 0x54, 0xCB, 0x13, 0x7A, 0xB4

SOF_LLEXT_MOD_ENTRY(google_ctc_audio_processing, &google_ctc_audio_processing_interface);

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("CTC", google_ctc_audio_processing_llext_entry,
				  1, UUID_GOOGLE_CTC, 40);

SOF_LLEXT_BUILDINFO;

#endif
