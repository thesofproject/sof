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
#include <sof/audio/sink_source_utils.h>
#include <rtos/init.h>

#include <google_ctc_audio_processing.h>

#include "google_ctc_audio_processing.h"

LOG_MODULE_REGISTER(google_ctc_audio_processing, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(google_ctc_audio_processing);

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

static int ctc_passthrough(struct sof_source *source, struct sof_sink *sink, size_t frames)
{
	return source_to_sink_copy(source, sink, true, frames * source_get_frame_bytes(source));
}

#if CONFIG_FORMAT_S16LE
static int ctc_s16_default(struct google_ctc_audio_processing_comp_data *cd,
			   struct sof_source *source,
			   struct sof_sink *sink,
			   size_t frames)
{
	unsigned int n_ch = source_get_channels(source);
	size_t samples = frames * n_ch;
	const int16_t *src, *src_start;
	int16_t *dest, *dest_start;
	size_t src_samples, dest_samples;
	size_t samples_to_process, samples_to_written;
	size_t written_samples = 0;
	int ret;

	if (!cd->enabled)
		return ctc_passthrough(source, sink, frames);

	ret = source_get_data_s16(source, frames * source_get_frame_bytes(source),
				  &src, &src_start, &src_samples);
	if (ret)
		return ret;

	ret = sink_get_buffer_s16(sink, frames * sink_get_frame_bytes(sink),
				  &dest, &dest_start, &dest_samples);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}

	samples_to_process = MIN(samples, cir_buf_samples_without_wrap_s16(src,
						src_start + src_samples));
	samples_to_written = MIN(samples, cir_buf_samples_without_wrap_s16(dest,
						dest_start + dest_samples));

	// writes previous processed samples to the output.
	while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
	       written_samples < samples_to_written) {
		dest[written_samples++] =
			convert_float_to_int16(cd->output[cd->next_avail_output_samples]);
		cd->next_avail_output_samples++;
	}
	for (size_t i = 0; i < samples_to_process; ++i) {
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

	ret = source_release_data(source, samples_to_process * sizeof(int16_t));
	if (ret) {
		sink_commit_buffer(sink, 0);
		return ret;
	}
	return sink_commit_buffer(sink, written_samples * sizeof(int16_t));
}
#endif

#if CONFIG_FORMAT_S24LE
static int ctc_s24_default(struct google_ctc_audio_processing_comp_data *cd,
			   struct sof_source *source,
			   struct sof_sink *sink,
			   size_t frames)
{
	unsigned int n_ch = source_get_channels(source);
	size_t samples = frames * n_ch;
	const int32_t *src, *src_start;
	int32_t *dest, *dest_start;
	size_t src_samples, dest_samples;
	size_t samples_to_process, samples_to_written;
	size_t written_samples = 0;
	int ret;

	if (!cd->enabled)
		return ctc_passthrough(source, sink, frames);

	ret = source_get_data_s32(source, frames * source_get_frame_bytes(source),
				  &src, &src_start, &src_samples);
	if (ret)
		return ret;

	ret = sink_get_buffer_s32(sink, frames * sink_get_frame_bytes(sink),
				  &dest, &dest_start, &dest_samples);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}

	samples_to_process = MIN(samples, cir_buf_samples_without_wrap_s32(src,
						src_start + src_samples));
	samples_to_written = MIN(samples, cir_buf_samples_without_wrap_s32(dest,
						dest_start + dest_samples));

	// writes previous processed samples to the output.
	while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
	       written_samples < samples_to_written) {
		dest[written_samples++] =
			convert_float_to_int32(cd->output[cd->next_avail_output_samples]);
		cd->next_avail_output_samples++;
	}
	for (size_t i = 0; i < samples_to_process; ++i) {
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

	ret = source_release_data(source, samples_to_process * sizeof(int32_t));
	if (ret) {
		sink_commit_buffer(sink, written_samples * sizeof(int32_t));
		return ret;
	}
	return sink_commit_buffer(sink, written_samples * sizeof(int32_t));
}
#endif

#if CONFIG_FORMAT_S32LE
static int ctc_s32_default(struct google_ctc_audio_processing_comp_data *cd,
			   struct sof_source *source,
			   struct sof_sink *sink,
			   size_t frames)
{
	unsigned int n_ch = source_get_channels(source);
	size_t samples = frames * n_ch;
	const int32_t *src, *src_start;
	int32_t *dest, *dest_start;
	size_t src_samples, dest_samples;
	size_t samples_to_process, samples_to_written;
	size_t written_samples = 0;
	int ret;

	if (!cd->enabled)
		return ctc_passthrough(source, sink, frames);

	ret = source_get_data_s32(source, frames * source_get_frame_bytes(source),
				  &src, &src_start, &src_samples);
	if (ret)
		return ret;

	ret = sink_get_buffer_s32(sink, frames * sink_get_frame_bytes(sink),
				  &dest, &dest_start, &dest_samples);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}

	samples_to_process = MIN(samples, cir_buf_samples_without_wrap_s32(src,
						src_start + src_samples));
	samples_to_written = MIN(samples, cir_buf_samples_without_wrap_s32(dest,
						dest_start + dest_samples));

	// writes previous processed samples to the output.
	while (cd->next_avail_output_samples < cd->chunk_frames * n_ch &&
	       written_samples < samples_to_written) {
		dest[written_samples++] =
			convert_float_to_int32(cd->output[cd->next_avail_output_samples]);
		cd->next_avail_output_samples++;
	}
	for (size_t i = 0; i < samples_to_process; ++i) {
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

	ret = source_release_data(source, samples_to_process * sizeof(int32_t));
	if (ret) {
		sink_commit_buffer(sink, written_samples * sizeof(int32_t));
		return ret;
	}
	return sink_commit_buffer(sink, written_samples * sizeof(int32_t));
}
#endif

static int ctc_free(struct processing_module *mod)
{
	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "entry");

	if (cd) {
		mod_free(mod, cd->input);
		mod_free(mod, cd->output);
		GoogleCtcAudioProcessingFree(cd->state);
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
	struct sof_source *source;
	unsigned int num_channels;
	uint8_t *config;
	int config_size;

	comp_info(mod->dev, "entry");

	if (!num_of_sources || !num_of_sinks) {
		comp_err(dev, "no source or sink buffer");
		return -ENOTCONN;
	}

	source = sources[0];

	switch (source_get_frm_fmt(source)) {
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

	num_channels = source_get_channels(source);
	if (num_channels > (unsigned int)kMaxChannels) {
		comp_err(mod->dev, "invalid number of channels");
		return -EINVAL;
	}
	cd->next_avail_output_samples = cd->chunk_frames * num_channels;

	config = comp_get_data_blob(cd->tuning_handler, &config_size, NULL);

	if (config_size != CTC_BLOB_CONFIG_SIZE) {
		comp_info(mod->dev, "config_size not expected: %d", config_size);
		config = NULL;
		config_size = 0;
	}
	cd->state = GoogleCtcAudioProcessingCreateWithConfig(cd->chunk_frames,
							     source_get_rate(source),
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
	size_t buf_size = cd->chunk_frames * sizeof(cd->input[0]) * kMaxChannels;

	comp_info(mod->dev, "entry");

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
		       struct sof_source **sources,
		       int num_of_sources,
		       struct sof_sink **sinks,
		       int num_of_sinks)
{
	struct google_ctc_audio_processing_comp_data *cd = module_get_private_data(mod);
	struct sof_source *source = sources[0];
	struct sof_sink *sink = sinks[0];
	size_t frames = MIN(source_get_data_frames_available(source),
			    sink_get_free_frames(sink));
	int ret;

	comp_dbg(mod->dev, "entry");

	if (cd->reconfigure) {
		ret = google_ctc_audio_processing_reconfigure(mod);
		if (ret)
			return ret;
	}

	return cd->ctc_func(cd, source, sink, frames);
}

static const struct module_interface google_ctc_audio_processing_interface = {
	.init  = ctc_init,
	.free = ctc_free,
	.process = ctc_process,
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

/* unused with Zephyr, generates no output */
DECLARE_TR_CTX(google_ctc_audio_processing_tr, SOF_UUID(google_ctc_audio_processing_uuid),
	       LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(google_ctc_audio_processing_interface,
		       google_ctc_audio_processing_uuid, google_ctc_audio_processing_tr);
SOF_MODULE_INIT(google_ctc_audio_processing,
		sys_comp_module_google_ctc_audio_processing_interface_init);

#endif
