// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Google LLC.
//
// Author: Lionel Koenig <lionelk@google.com>
#include <errno.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <ipc4/aec.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/format.h>
#include <sof/audio/kpb.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <rtos/wait.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <rtos/string.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <user/trace.h>

#include <google_rtc_audio_processing.h>
#include <google_rtc_audio_processing_platform.h>
#include <google_rtc_audio_processing_sof_message_reader.h>

#define GOOGLE_RTC_AUDIO_PROCESSING_FREQENCY_TO_PERIOD_FRAMES 100

LOG_MODULE_REGISTER(google_rtc_audio_processing, CONFIG_SOF_LOG_LEVEL);

/* b780a0a6-269f-466f-b477-23dfa05af758 */
DECLARE_SOF_RT_UUID("google-rtc-audio-processing", google_rtc_audio_processing_uuid,
					0xb780a0a6, 0x269f, 0x466f, 0xb4, 0x77, 0x23, 0xdf, 0xa0,
					0x5a, 0xf7, 0x58);

DECLARE_TR_CTX(google_rtc_audio_processing_tr, SOF_UUID(google_rtc_audio_processing_uuid),
			   LOG_LEVEL_INFO);

struct google_rtc_audio_processing_comp_data {
#if CONFIG_IPC_MAJOR_4
	struct sof_ipc4_aec_config config;
#endif
	struct comp_buffer *raw_microphone;
	struct comp_buffer *aec_reference;
	struct comp_buffer *output;
	uint32_t num_frames;
	int num_aec_reference_channels;
	int num_capture_channels;
	GoogleRtcAudioProcessingState *state;
	int16_t *aec_reference_buffer;
	int aec_reference_frame_index;
	int16_t *raw_mic_buffer;
	int raw_mic_buffer_frame_index;
	int16_t *output_buffer;
	int output_buffer_frame_index;
	uint8_t *memory_buffer;
	struct comp_data_blob_handler *tuning_handler;
	bool reconfigure;
};

void *GoogleRtcMalloc(size_t size)
{
	return rballoc(0, SOF_MEM_CAPS_RAM, size);
}

void GoogleRtcFree(void *ptr)
{
	return rfree(ptr);
}

static int google_rtc_audio_processing_params(
	struct comp_dev *dev,
	struct sof_ipc_stream_params *params)
{
	int ret;
#if CONFIG_IPC_MAJOR_4
	struct google_rtc_audio_processing_comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer __sparse_cache *sink_c;
	struct comp_buffer *sink;

	/* update sink buffer format */
	memset(params, 0, sizeof(*params));
	params->channels = cd->config.base_cfg.audio_fmt.channels_count;
	params->rate = cd->config.base_cfg.audio_fmt.sampling_frequency;
	params->sample_container_bytes = cd->config.base_cfg.audio_fmt.depth / 8;
	params->sample_valid_bytes =
		cd->config.base_cfg.audio_fmt.valid_bit_depth / 8;
	params->buffer_fmt = cd->config.base_cfg.audio_fmt.interleaving_style;
	params->buffer.size = cd->config.base_cfg.ibs;

	/* update sink format */
	if (!list_is_empty(&dev->bsink_list)) {
		struct ipc4_audio_format *out_fmt = &cd->config.output_fmt;
		enum sof_ipc_frame valid_fmt, frame_fmt;

		sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
		sink_c = buffer_acquire(sink);

		audio_stream_fmt_conversion(out_fmt->depth,
					    out_fmt->valid_bit_depth,
					    &frame_fmt, &valid_fmt,
					    out_fmt->s_type);

		audio_stream_set_frm_fmt(&sink_c->stream, frame_fmt);
		audio_stream_set_valid_fmt(&sink_c->stream, valid_fmt);
		audio_stream_set_channels(&sink_c->stream, cd->config.output_fmt.channels_count);
		audio_stream_set_rate(&sink_c->stream, cd->config.output_fmt.sampling_frequency);

		sink_c->buffer_fmt = out_fmt->interleaving_style;
		params->frame_fmt = valid_fmt;

		sink_c->hw_params_configured = true;

		buffer_release(sink_c);
	}
#endif

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev,
			 "google_rtc_audio_processing_params(): comp_verify_params failed.");
		return -EINVAL;
	}

	return 0;
}

static int google_rtc_audio_processing_reconfigure(struct comp_dev *dev)
{
	struct google_rtc_audio_processing_comp_data *cd = comp_get_drvdata(dev);
	uint8_t *config;
	size_t size;
	int ret;

	comp_dbg(dev, "google_rtc_audio_processing_reconfigure()");

	if (!comp_is_current_data_blob_valid(cd->tuning_handler) &&
	    !comp_is_new_data_blob_available(cd->tuning_handler)) {
		/*
		 * The data blob hasn't been available once so far.
		 *
		 * This looks redundant since the same check will be done in
		 * comp_get_data_blob() below. But without this early return,
		 * hundreds of warn message lines are produced per second by
		 * comp_get_data_blob() calls until the data blob is arrived.
		 */
		return 0;
	}

	config = comp_get_data_blob(cd->tuning_handler, &size, NULL);
	if (size == 0) {
		/* No data to be handled */
		return 0;
	}

	if (!config) {
		comp_err(dev, "google_rtc_audio_processing_reconfigure(): Tuning config not set");
		return -EINVAL;
	}

	comp_info(dev, "google_rtc_audio_processing_reconfigure(): New tuning config %p (%zu bytes)",
		  config, size);

	cd->reconfigure = false;

	uint8_t *google_rtc_audio_processing_config;
	size_t google_rtc_audio_processing_config_size;
	int num_capture_input_channels;
	int num_capture_output_channels;
	float aec_reference_delay;
	float mic_gain;
	bool google_rtc_audio_processing_config_present;
	bool num_capture_input_channels_present;
	bool num_capture_output_channels_present;
	bool aec_reference_delay_present;
	bool mic_gain_present;

	GoogleRtcAudioProcessingParseSofConfigMessage(config, size,
						      &google_rtc_audio_processing_config,
						      &google_rtc_audio_processing_config_size,
						      &num_capture_input_channels,
						      &num_capture_output_channels,
						      &aec_reference_delay,
						      &mic_gain,
						      &google_rtc_audio_processing_config_present,
						      &num_capture_input_channels_present,
						      &num_capture_output_channels_present,
						      &aec_reference_delay_present,
						      &mic_gain_present);

	if (google_rtc_audio_processing_config_present) {
		comp_info(dev,
			  "google_rtc_audio_processing_reconfigure(): Applying config of size %zu bytes",
			  google_rtc_audio_processing_config_size);

		ret = GoogleRtcAudioProcessingReconfigure(cd->state,
							  google_rtc_audio_processing_config,
							  google_rtc_audio_processing_config_size);
		if (ret) {
			comp_err(dev, "GoogleRtcAudioProcessingReconfigure failed: %d",
				 ret);
			return ret;
		}
	}

	if (num_capture_input_channels_present || num_capture_output_channels_present) {
		if (num_capture_input_channels_present && num_capture_output_channels_present) {
			if (num_capture_input_channels != num_capture_output_channels) {
				comp_err(dev, "GoogleRtcAudioProcessingReconfigure failed: unsupported channel counts");
				return -EINVAL;
			}
			cd->num_capture_channels = num_capture_input_channels;
		} else if (num_capture_input_channels_present) {
			cd->num_capture_channels = num_capture_output_channels;
		} else {
			cd->num_capture_channels = num_capture_output_channels;
		}
		comp_info(dev,
			  "google_rtc_audio_processing_reconfigure(): Applying num capture channels %d",
			  cd->num_capture_channels);


		ret = GoogleRtcAudioProcessingSetStreamFormats(cd->state,
							       CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ,
							       cd->num_capture_channels,
							       cd->num_capture_channels,
							       CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ,
							       cd->num_aec_reference_channels);

		if (ret) {
			comp_err(dev, "GoogleRtcAudioProcessingSetStreamFormats failed: %d",
				 ret);
			return ret;
		}
	}

	if (aec_reference_delay_present || mic_gain_present) {
		float *capture_headroom_linear_use = NULL;
		float *echo_path_delay_ms_use = NULL;

		if (mic_gain_present) {
			capture_headroom_linear_use = &mic_gain;

			/* Logging of linear headroom, using integer workaround to the broken printout of floats */
			comp_info(dev,
				  "google_rtc_audio_processing_reconfigure(): Applying capture linear headroom: %d.%d",
				  (int)mic_gain, (int)(100 * mic_gain) - 100 * ((int)mic_gain));
		}
		if (aec_reference_delay_present) {
			echo_path_delay_ms_use = &aec_reference_delay;

			/* Logging of delay, using integer workaround to the broken printout of floats */
			comp_info(dev,
				  "google_rtc_audio_processing_reconfigure(): Applying aec reference delay: %d.%d",
				  (int)aec_reference_delay,
				  (int)(100 * aec_reference_delay) -
				  100 * ((int)aec_reference_delay));
		}

		ret = GoogleRtcAudioProcessingParameters(cd->state,
							 capture_headroom_linear_use,
							 echo_path_delay_ms_use);

		if (ret) {
			comp_err(dev, "GoogleRtcAudioProcessingParameters failed: %d",
				 ret);
			return ret;
		}
	}

	return 0;
}

static int google_rtc_audio_processing_cmd_set_data(
	struct comp_dev *dev,
	struct sof_ipc_ctrl_data *cdata)
{
	struct google_rtc_audio_processing_comp_data *cd = comp_get_drvdata(dev);
	int ret;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		ret = comp_data_blob_set_cmd(cd->tuning_handler, cdata);
		if (ret)
			return ret;
		/* Accept the new blob immediately so that userspace can write
		 * the control in quick succession without error.
		 * This ensures the last successful control write from userspace
		 * before prepare/copy is applied.
		 * The config blob is not referenced after reconfigure() returns
		 * so it is safe to call comp_get_data_blob here which frees the
		 * old blob. This assumes cmd() and prepare()/copy() cannot run
		 * concurrently which is the case when there is no preemption.
		 */
		if (comp_is_new_data_blob_available(cd->tuning_handler)) {
			comp_get_data_blob(cd->tuning_handler, NULL, NULL);
			cd->reconfigure = true;
		}
		return 0;
	default:
		comp_err(dev,
				 "google_rtc_audio_processing_ctrl_set_data(): Only binary controls supported %d",
				 cdata->cmd);
		return -EINVAL;
	}
}

static int google_rtc_audio_processing_cmd_get_data(
	struct comp_dev *dev,
	struct sof_ipc_ctrl_data *cdata,
	int max_data_size)
{
	struct google_rtc_audio_processing_comp_data *cd = comp_get_drvdata(dev);

	comp_info(dev, "google_rtc_audio_processing_ctrl_get_data(): %u", cdata->cmd);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		return comp_data_blob_get_cmd(cd->tuning_handler, cdata, max_data_size);
	default:
		comp_err(dev,
				 "google_rtc_audio_processing_ctrl_get_data(): Only binary controls supported %d",
				 cdata->cmd);
		return -EINVAL;
	}
}

static int google_rtc_audio_processing_cmd(struct comp_dev *dev, int cmd, void *data,
									 int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;

	comp_dbg(dev, "google_rtc_audio_processing_cmd(): %d - data_cmd: %d", cmd, cdata->cmd);

	switch (cmd) {
	case COMP_CMD_SET_VALUE:
	case COMP_CMD_GET_VALUE:
		return 0;
	case COMP_CMD_SET_DATA:
		return google_rtc_audio_processing_cmd_set_data(dev, cdata);
	case COMP_CMD_GET_DATA:
		return google_rtc_audio_processing_cmd_get_data(dev, cdata, max_data_size);
	default:
		comp_err(dev, "google_rtc_audio_processing_cmd(): Unknown cmd %d", cmd);
		return -EINVAL;
	}
}

static struct comp_dev *google_rtc_audio_processing_create(
	const struct comp_driver *drv,
	const struct comp_ipc_config *config,
	const void *spec)
{
	struct comp_dev *dev;
	struct google_rtc_audio_processing_comp_data *cd;
	int ret;

	comp_cl_info(drv, "google_rtc_audio_processing_create()");

	/* Create component device with an effect processing component */
	dev = comp_alloc(drv, sizeof(*dev));

	if (!dev)
		return NULL;

	dev->ipc_config = *config;

	/* Create private component data */
	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		goto fail;

#if CONFIG_IPC_MAJOR_4
	cd->config = *(const struct sof_ipc4_aec_config *)spec;
#endif

	cd->tuning_handler = comp_data_blob_handler_new(dev);
	if (!cd->tuning_handler)
		goto fail;

	cd->num_aec_reference_channels = CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_NUM_AEC_REFERENCE_CHANNELS;
	cd->num_capture_channels = CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_NUM_CHANNELS;
	cd->num_frames = CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ /
		GOOGLE_RTC_AUDIO_PROCESSING_FREQENCY_TO_PERIOD_FRAMES;

	if (CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_MEMORY_BUFFER_SIZE_BYTES > 0) {
		cd->memory_buffer = rballoc(0, SOF_MEM_CAPS_RAM,
					    CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_MEMORY_BUFFER_SIZE_BYTES *
					    sizeof(cd->memory_buffer[0]));
		if (!cd->memory_buffer) {
			comp_err(dev, "google_rtc_audio_processing_create: failed to allocate memory buffer");

			goto fail;
		}

		GoogleRtcAudioProcessingAttachMemoryBuffer(cd->memory_buffer, CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_MEMORY_BUFFER_SIZE_BYTES);
	}

	cd->state = GoogleRtcAudioProcessingCreateWithConfig(CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ,
							     cd->num_capture_channels,
							     cd->num_capture_channels,
							     CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ,
							     cd->num_aec_reference_channels,
							     /*config=*/NULL, /*config_size=*/0);

	if (!cd->state) {
		comp_err(dev, "Failed to initialized GoogleRtcAudioProcessing");
		goto fail;
	}

	float capture_headroom_linear = CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_MIC_HEADROOM_LINEAR;
	float echo_path_delay_ms = CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_ECHO_PATH_DELAY_MS;
	ret = GoogleRtcAudioProcessingParameters(cd->state,
						 &capture_headroom_linear,
						 &echo_path_delay_ms);

	if (ret < 0) {
		comp_err(dev, "Failed to apply GoogleRtcAudioProcessingParameters");
		goto fail;
	}

	cd->raw_mic_buffer = rballoc(
		0, SOF_MEM_CAPS_RAM,
		cd->num_frames * cd->num_capture_channels * sizeof(cd->raw_mic_buffer[0]));
	if (!cd->raw_mic_buffer)
		goto fail;
	bzero(cd->raw_mic_buffer, cd->num_frames * cd->num_capture_channels * sizeof(cd->raw_mic_buffer[0]));
	cd->raw_mic_buffer_frame_index = 0;

	cd->aec_reference_buffer = rballoc(
		0, SOF_MEM_CAPS_RAM,
		cd->num_frames * sizeof(cd->aec_reference_buffer[0]) *
		cd->num_aec_reference_channels);
	if (!cd->aec_reference_buffer)
		goto fail;
	bzero(cd->aec_reference_buffer, cd->num_frames * cd->num_aec_reference_channels * sizeof(cd->aec_reference_buffer[0]));
	cd->aec_reference_frame_index = 0;

	cd->output_buffer = rballoc(
		0, SOF_MEM_CAPS_RAM,
		cd->num_frames * cd->num_capture_channels * sizeof(cd->output_buffer[0]));
	if (!cd->output_buffer)
		goto fail;
	bzero(cd->output_buffer, cd->num_frames * sizeof(cd->output_buffer[0]));
	cd->output_buffer_frame_index = 0;

	/* comp_is_new_data_blob_available always returns false for the first
	 * control write with non-empty config. The first non-empty write may
	 * happen after prepare (e.g. during copy). Default to true so that
	 * copy keeps checking until a non-empty config is applied.
	 */
	cd->reconfigure = true;

	comp_set_drvdata(dev, cd);
	dev->state = COMP_STATE_READY;
	comp_dbg(dev, "google_rtc_audio_processing_create(): Ready");
	return dev;
fail:
	comp_err(dev, "google_rtc_audio_processing_create(): Failed");
	if (cd) {
		rfree(cd->output_buffer);
		rfree(cd->aec_reference_buffer);
		if (cd->state) {
			GoogleRtcAudioProcessingFree(cd->state);
		}
		GoogleRtcAudioProcessingDetachMemoryBuffer();
		rfree(cd->memory_buffer);
		rfree(cd->raw_mic_buffer);
		comp_data_blob_handler_free(cd->tuning_handler);
		rfree(cd);
	}
	rfree(dev);
	return NULL;
}

static void google_rtc_audio_processing_free(struct comp_dev *dev)
{
	struct google_rtc_audio_processing_comp_data *cd = comp_get_drvdata(dev);

	comp_dbg(dev, "google_rtc_audio_processing_free()");

	GoogleRtcAudioProcessingFree(cd->state);
	cd->state = NULL;
	rfree(cd->output_buffer);
	rfree(cd->aec_reference_buffer);
	GoogleRtcAudioProcessingDetachMemoryBuffer();
	rfree(cd->memory_buffer);
	rfree(cd->raw_mic_buffer);
	comp_data_blob_handler_free(cd->tuning_handler);
	rfree(cd);
	rfree(dev);
}


static int google_rtc_audio_processing_trigger(struct comp_dev *dev, int cmd)
{
	comp_dbg(dev, "google_rtc_audio_processing_trigger(): %d", cmd);

	return comp_set_state(dev, cmd);
}

static int google_rtc_audio_processing_prepare(struct comp_dev *dev)
{
	struct google_rtc_audio_processing_comp_data *cd = comp_get_drvdata(dev);
	struct list_item *source_buffer_list_item;
	struct comp_buffer __sparse_cache *output_c;
	unsigned int aec_channels = 0, frame_fmt, rate;
	int ret;

	comp_dbg(dev, "google_rtc_audio_processing_prepare()");

	/* searching for stream and feedback source buffers */
	list_for_item(source_buffer_list_item, &dev->bsource_list) {
		struct comp_buffer *source = container_of(source_buffer_list_item,
							  struct comp_buffer, sink_list);
		struct comp_buffer __sparse_cache *source_c = buffer_acquire(source);

#if CONFIG_IPC_MAJOR_4
		if (IPC4_SINK_QUEUE_ID(source_c->id) == SOF_AEC_FEEDBACK_QUEUE_ID) {
#else
		if (source_c->source->pipeline->pipeline_id != dev->pipeline->pipeline_id) {
#endif
			cd->aec_reference = source;
			aec_channels = audio_stream_get_channels(&source_c->stream);
		} else {
			cd->raw_microphone = source;
		}
		buffer_release(source_c);
	}

	cd->output = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);

	/* On some platform the playback output is left right left right due to a crossover
	 * later on the signal processing chain. That makes the aec_reference be 4 channels
	 * and the AEC should only use the 2 first.
	 */
	if (cd->num_aec_reference_channels > aec_channels) {
		comp_err(dev, "unsupported number of AEC reference channels: %d",
			 aec_channels);
		return -EINVAL;
	}

	output_c = buffer_acquire(cd->output);
	frame_fmt = audio_stream_get_frm_fmt(&output_c->stream);
	rate = audio_stream_get_rate(&output_c->stream);
	buffer_release(output_c);


	if (cd->num_capture_channels > audio_stream_get_channels(&cd->raw_microphone->stream)) {
		comp_err(dev, "unsupported number of microphone channels: %d",
			 audio_stream_get_channels(&cd->raw_microphone->stream));
		return -EINVAL;
	}

	if (cd->num_capture_channels > audio_stream_get_channels(&cd->output->stream)) {
		comp_err(dev, "unsupported number of output channels: %d",
			 audio_stream_get_channels(&cd->output->stream));
		return -EINVAL;
	}

	switch (frame_fmt) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		break;
#endif /* CONFIG_FORMAT_S16LE */
	default:
		comp_err(dev, "unsupported data format: %d", frame_fmt);
		return -EINVAL;
	}

	if (rate != CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ) {
		comp_err(dev, "unsupported samplerate: %d", rate);
		return -EINVAL;
	}

	/* Blobs sent during COMP_STATE_READY is assigned to blob_handler->data
	 * directly, so comp_is_new_data_blob_available always returns false.
	 */
	ret = google_rtc_audio_processing_reconfigure(dev);
	if (ret)
		return ret;

	return comp_set_state(dev, COMP_TRIGGER_PREPARE);
}

static int google_rtc_audio_processing_reset(struct comp_dev *dev)
{
	comp_dbg(dev, "google_rtc_audio_processing_reset()");

	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

static int google_rtc_audio_processing_copy(struct comp_dev *dev)
{
	struct google_rtc_audio_processing_comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer __sparse_cache *buffer_c, *mic_buf, *output_buf;
	struct comp_copy_limits cl;
	int16_t *src, *dst, *ref;
	uint32_t num_aec_reference_frames;
	uint32_t num_aec_reference_bytes;
	int num_samples_remaining;
	int num_frames_remaining;
	int channel;
	int nmax;
	int ret;
	int i, j, n;

	if (cd->reconfigure) {
		ret = google_rtc_audio_processing_reconfigure(dev);
		if (ret)
			return ret;
	}

	buffer_c = buffer_acquire(cd->aec_reference);

	ref = audio_stream_get_rptr(&buffer_c->stream);

	num_aec_reference_frames = audio_stream_get_avail_frames(&buffer_c->stream);
	num_aec_reference_bytes = audio_stream_get_avail_bytes(&buffer_c->stream);

	buffer_stream_invalidate(buffer_c, num_aec_reference_bytes);

	num_samples_remaining = num_aec_reference_frames *
		audio_stream_get_channels(&buffer_c->stream);
	while (num_samples_remaining) {
		nmax = audio_stream_samples_without_wrap_s16(&buffer_c->stream, ref);
		n = MIN(num_samples_remaining, nmax);
		for (i = 0; i < n; i += cd->num_aec_reference_channels) {
			j = cd->num_aec_reference_channels * cd->aec_reference_frame_index;
			for (channel = 0; channel < cd->num_aec_reference_channels; ++channel)
				cd->aec_reference_buffer[j++] = ref[channel];

			ref += audio_stream_get_channels(&buffer_c->stream);
			++cd->aec_reference_frame_index;

			if (cd->aec_reference_frame_index == cd->num_frames) {
				GoogleRtcAudioProcessingAnalyzeRender_int16(cd->state,
									    cd->aec_reference_buffer);
				cd->aec_reference_frame_index = 0;
			}
		}
		num_samples_remaining -= n;
		ref = audio_stream_wrap(&buffer_c->stream, ref);
	}
	comp_update_buffer_consume(buffer_c, num_aec_reference_bytes);

	buffer_release(buffer_c);

	mic_buf = buffer_acquire(cd->raw_microphone);
	output_buf = buffer_acquire(cd->output);

	src = audio_stream_get_rptr(&mic_buf->stream);
	dst = audio_stream_get_wptr(&output_buf->stream);

	comp_get_copy_limits(mic_buf, output_buf, &cl);
	buffer_stream_invalidate(mic_buf, cl.source_bytes);

	num_frames_remaining = cl.frames;
	while (num_frames_remaining) {
		nmax = audio_stream_frames_without_wrap(&mic_buf->stream, src);
		n = MIN(num_frames_remaining, nmax);
		nmax = audio_stream_frames_without_wrap(&output_buf->stream, dst);
		n = MIN(n, nmax);
		for (i = 0; i < n; i++) {
			memcpy_s(&(cd->raw_mic_buffer[cd->raw_mic_buffer_frame_index *
						      cd->num_capture_channels]),
				 cd->num_frames * cd->num_capture_channels *
				 sizeof(cd->raw_mic_buffer[0]), src,
				 sizeof(int16_t) * cd->num_capture_channels);
			++cd->raw_mic_buffer_frame_index;


			memcpy_s(dst, cd->num_frames * cd->num_capture_channels *
				 sizeof(cd->output_buffer[0]),
				 &(cd->output_buffer[cd->output_buffer_frame_index *
						     cd->num_capture_channels]),
				 sizeof(int16_t) * cd->num_capture_channels);
			++cd->output_buffer_frame_index;

			if (cd->raw_mic_buffer_frame_index == cd->num_frames) {
				GoogleRtcAudioProcessingProcessCapture_int16(cd->state,
									     cd->raw_mic_buffer,
									     cd->output_buffer);
				cd->output_buffer_frame_index = 0;
				cd->raw_mic_buffer_frame_index = 0;
			}

			src += audio_stream_get_channels(&mic_buf->stream);
			dst += audio_stream_get_channels(&output_buf->stream);
		}
		num_frames_remaining -= n;
		src = audio_stream_wrap(&mic_buf->stream, src);
		dst = audio_stream_wrap(&output_buf->stream, dst);
	}

	buffer_stream_writeback(output_buf, cl.sink_bytes);

	comp_update_buffer_produce(output_buf, cl.sink_bytes);
	comp_update_buffer_consume(mic_buf, cl.source_bytes);

	buffer_release(output_buf);
	buffer_release(mic_buf);

	return 0;
}

#if CONFIG_IPC_MAJOR_4
static int google_rtc_audio_processing_set_large_config(struct comp_dev *dev,
							uint32_t param_id,
							bool first_block,
							bool last_block,
							uint32_t data_offset,
							const char *data)
{
	struct google_rtc_audio_processing_comp_data *cd = comp_get_drvdata(dev);
	size_t bcfg_size, ext_size;

	switch (param_id) {
	case IPC4_AEC_SET_EXT_FMT:
		bcfg_size = sizeof(struct ipc4_base_module_cfg);
		ext_size = sizeof(cd->config) - bcfg_size;
		memcpy_s((void *)&cd->config + bcfg_size, ext_size, data, ext_size);
		return 0;
	default:
		comp_err(dev, "Invalid param %d", param_id);
		return -EINVAL;
	}

	return 0;
}

static int google_rtc_audio_processing_get_attribute(struct comp_dev *dev,
						     uint32_t type, void *value)
{
	struct google_rtc_audio_processing_comp_data *cd = comp_get_drvdata(dev);

	switch (type) {
	case COMP_ATTR_BASE_CONFIG:
		*(struct ipc4_base_module_cfg *)value = cd->config.base_cfg;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
#endif

static const struct comp_driver google_rtc_audio_processing = {
	.uid = SOF_RT_UUID(google_rtc_audio_processing_uuid),
	.tctx = &google_rtc_audio_processing_tr,
	.ops = {
		.create = google_rtc_audio_processing_create,
		.free = google_rtc_audio_processing_free,
		.params = google_rtc_audio_processing_params,
		.cmd = google_rtc_audio_processing_cmd,
		.trigger = google_rtc_audio_processing_trigger,
		.copy = google_rtc_audio_processing_copy,
		.prepare = google_rtc_audio_processing_prepare,
		.reset = google_rtc_audio_processing_reset,
#if CONFIG_IPC_MAJOR_4
		.get_attribute = google_rtc_audio_processing_get_attribute,
		.set_large_config = google_rtc_audio_processing_set_large_config,
#endif
	},
};

static SHARED_DATA struct comp_driver_info google_rtc_audio_processing_info = {
	.drv = &google_rtc_audio_processing,
};

UT_STATIC void sys_comp_google_rtc_audio_processing_init(void)
{
	comp_register(
		platform_shared_get(
			&google_rtc_audio_processing_info,
			sizeof(google_rtc_audio_processing_info)));
}

DECLARE_MODULE(sys_comp_google_rtc_audio_processing_init);
SOF_MODULE_INIT(google_rtc_audio_processing, sys_comp_google_rtc_audio_processing_init);
