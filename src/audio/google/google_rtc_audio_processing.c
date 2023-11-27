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
#include <sof/audio/module_adapter/module/generic.h>
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
#define GOOGLE_RTC_NUM_INPUT_PINS 2

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
	int aec_reference_source;
	int raw_microphone_source;
};

void *GoogleRtcMalloc(size_t size)
{
	return rballoc(0, SOF_MEM_CAPS_RAM, size);
}

void GoogleRtcFree(void *ptr)
{
	return rfree(ptr);
}

#if CONFIG_IPC_MAJOR_4
static void google_rtc_audio_processing_params(struct processing_module *mod)
{
	struct google_rtc_audio_processing_comp_data *cd = module_get_private_data(mod);
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_buffer *sinkb, *sourceb;
	struct list_item *source_list;
	struct comp_dev *dev = mod->dev;

	ipc4_base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, params);
	component_set_nearest_period_frames(dev, params->rate);

	list_for_item(source_list, &dev->bsource_list) {
		sourceb = container_of(source_list, struct comp_buffer, sink_list);
		if (IPC4_SINK_QUEUE_ID(buf_get_id(sourceb)) == SOF_AEC_FEEDBACK_QUEUE_ID)
			ipc4_update_buffer_format(sourceb, &cd->config.reference_fmt);
		else
			ipc4_update_buffer_format(sourceb, &mod->priv.cfg.base_cfg.audio_fmt);
	}

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	ipc4_update_buffer_format(sinkb, &mod->priv.cfg.base_cfg.audio_fmt);
}
#endif

static int google_rtc_audio_processing_reconfigure(struct processing_module *mod)
{
	struct google_rtc_audio_processing_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
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

#if CONFIG_IPC_MAJOR_3
static int google_rtc_audio_processing_cmd_set_data(struct processing_module *mod,
						    struct sof_ipc_ctrl_data *cdata)
{
	struct google_rtc_audio_processing_comp_data *cd = module_get_private_data(mod);
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
		comp_err(mod->dev,
			 "google_rtc_audio_processing_ctrl_set_data(): Only binary controls supported %d",
			 cdata->cmd);
		return -EINVAL;
	}
}

static int google_rtc_audio_processing_cmd_get_data(struct processing_module *mod,
						    struct sof_ipc_ctrl_data *cdata,
						    size_t max_data_size)
{
	struct google_rtc_audio_processing_comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "google_rtc_audio_processing_ctrl_get_data(): %u", cdata->cmd);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		return comp_data_blob_get_cmd(cd->tuning_handler, cdata, max_data_size);
	default:
		comp_err(mod->dev,
			 "google_rtc_audio_processing_ctrl_get_data(): Only binary controls supported %d",
			 cdata->cmd);
		return -EINVAL;
	}
}
#endif

static int google_rtc_audio_processing_set_config(struct processing_module *mod, uint32_t param_id,
						  enum module_cfg_fragment_position pos,
						  uint32_t data_offset_size,
						  const uint8_t *fragment,
						  size_t fragment_size, uint8_t *response,
						  size_t response_size)
{
#if CONFIG_IPC_MAJOR_4
	struct google_rtc_audio_processing_comp_data *cd = module_get_private_data(mod);
	int ret;

	switch (param_id) {
	case SOF_IPC4_SWITCH_CONTROL_PARAM_ID:
	case SOF_IPC4_ENUM_CONTROL_PARAM_ID:
		comp_err(mod->dev, "google_rtc_audio_processing_ctrl_set_data(): Only binary controls supported");
		return -EINVAL;
	}

	ret = comp_data_blob_set(cd->tuning_handler, pos, data_offset_size,
				 fragment, fragment_size);
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
	 *
	 * Note from review: A race condition is possible and should be
	 * further investigated and fixed.
	 */
	if (comp_is_new_data_blob_available(cd->tuning_handler)) {
		comp_get_data_blob(cd->tuning_handler, NULL, NULL);
		cd->reconfigure = true;
	}

	return 0;
#elif CONFIG_IPC_MAJOR_3
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;

	return google_rtc_audio_processing_cmd_set_data(mod, cdata);
#endif
}

static int google_rtc_audio_processing_get_config(struct processing_module *mod,
						  uint32_t param_id, uint32_t *data_offset_size,
						  uint8_t *fragment, size_t fragment_size)
{
#if CONFIG_IPC_MAJOR_4
	comp_err(mod->dev, "google_rtc_audio_processing_ctrl_get_config(): Not supported");
	return -EINVAL;
#elif CONFIG_IPC_MAJOR_3
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;

	return google_rtc_audio_processing_cmd_get_data(mod, cdata, fragment_size);
#endif
}

static int google_rtc_audio_processing_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct google_rtc_audio_processing_comp_data *cd;
	int ret;

	comp_info(dev, "google_rtc_audio_processing_init()");

	/* Create private component data */
	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		ret = -ENOMEM;
		goto fail;
	}

	md->private = cd;

#if CONFIG_IPC_MAJOR_4
	const struct ipc4_base_module_extended_cfg *base_cfg = md->cfg.init_data;
	struct ipc4_input_pin_format reference_fmt, output_fmt;
	const size_t size = sizeof(struct ipc4_input_pin_format);

	cd->config.base_cfg = base_cfg->base_cfg;

	/* Copy the reference format from input pin 1 format */
	memcpy_s(&reference_fmt, size,
		 &base_cfg->base_cfg_ext.pin_formats[size], size);
	memcpy_s(&output_fmt, size,
		 &base_cfg->base_cfg_ext.pin_formats[size * GOOGLE_RTC_NUM_INPUT_PINS], size);

	cd->config.reference_fmt = reference_fmt.audio_fmt;
	cd->config.output_fmt = output_fmt.audio_fmt;
#endif

	cd->tuning_handler = comp_data_blob_handler_new(dev);
	if (!cd->tuning_handler) {
		ret = -ENOMEM;
		goto fail;
	}

	cd->num_aec_reference_channels = CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_NUM_AEC_REFERENCE_CHANNELS;
	cd->num_capture_channels = CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_NUM_CHANNELS;
	cd->num_frames = CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ /
		GOOGLE_RTC_AUDIO_PROCESSING_FREQENCY_TO_PERIOD_FRAMES;

	if (CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_MEMORY_BUFFER_SIZE_BYTES > 0) {
		cd->memory_buffer = rballoc(0, SOF_MEM_CAPS_RAM,
					    CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_MEMORY_BUFFER_SIZE_BYTES *
					    sizeof(cd->memory_buffer[0]));
		if (!cd->memory_buffer) {
			comp_err(dev, "google_rtc_audio_processing_init: failed to allocate memory buffer");
			ret = -ENOMEM;
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
		ret = -EINVAL;
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
	if (!cd->raw_mic_buffer) {
		ret = -EINVAL;
		goto fail;
	}
	bzero(cd->raw_mic_buffer, cd->num_frames * cd->num_capture_channels * sizeof(cd->raw_mic_buffer[0]));
	cd->raw_mic_buffer_frame_index = 0;

	cd->aec_reference_buffer = rballoc(
		0, SOF_MEM_CAPS_RAM,
		cd->num_frames * sizeof(cd->aec_reference_buffer[0]) *
		cd->num_aec_reference_channels);
	if (!cd->aec_reference_buffer) {
		ret = -ENOMEM;
		goto fail;
	}
	bzero(cd->aec_reference_buffer, cd->num_frames * cd->num_aec_reference_channels * sizeof(cd->aec_reference_buffer[0]));
	cd->aec_reference_frame_index = 0;

	cd->output_buffer = rballoc(
		0, SOF_MEM_CAPS_RAM,
		cd->num_frames * cd->num_capture_channels * sizeof(cd->output_buffer[0]));
	if (!cd->output_buffer) {
		ret = -ENOMEM;
		goto fail;
	}
	bzero(cd->output_buffer, cd->num_frames * sizeof(cd->output_buffer[0]));
	cd->output_buffer_frame_index = 0;

	/* comp_is_new_data_blob_available always returns false for the first
	 * control write with non-empty config. The first non-empty write may
	 * happen after prepare (e.g. during copy). Default to true so that
	 * copy keeps checking until a non-empty config is applied.
	 */
	cd->reconfigure = true;

	/* Mic and reference, needed for audio stream type copy module client */
	mod->max_sources = 2;

	comp_dbg(dev, "google_rtc_audio_processing_init(): Ready");
	return 0;

fail:
	comp_err(dev, "google_rtc_audio_processing_init(): Failed");
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

	return ret;
}

static int google_rtc_audio_processing_free(struct processing_module *mod)
{
	struct google_rtc_audio_processing_comp_data *cd = module_get_private_data(mod);

	comp_dbg(mod->dev, "google_rtc_audio_processing_free()");

	GoogleRtcAudioProcessingFree(cd->state);
	cd->state = NULL;
	rfree(cd->output_buffer);
	rfree(cd->aec_reference_buffer);
	GoogleRtcAudioProcessingDetachMemoryBuffer();
	rfree(cd->memory_buffer);
	rfree(cd->raw_mic_buffer);
	comp_data_blob_handler_free(cd->tuning_handler);
	rfree(cd);
	return 0;
}

static int google_rtc_audio_processing_prepare(struct processing_module *mod,
					       struct sof_source **sources,
					       int num_of_sources,
					       struct sof_sink **sinks,
					       int num_of_sinks)
{
	struct comp_dev *dev = mod->dev;
	struct google_rtc_audio_processing_comp_data *cd = module_get_private_data(mod);
	struct list_item *source_buffer_list_item;
	struct comp_buffer *output;
	unsigned int aec_channels = 0, frame_fmt, rate;
	int microphone_stream_channels = 0;
	int output_stream_channels;
	int ret;
	int i = 0;

	comp_info(dev, "google_rtc_audio_processing_prepare()");

#if CONFIG_IPC_MAJOR_4
	google_rtc_audio_processing_params(mod);
#endif

	/* searching for stream and feedback source buffers */
	list_for_item(source_buffer_list_item, &dev->bsource_list) {
		struct comp_buffer *source = container_of(source_buffer_list_item,
							  struct comp_buffer, sink_list);
#if CONFIG_IPC_MAJOR_4
		if (IPC4_SINK_QUEUE_ID(buf_get_id(source)) ==
			SOF_AEC_FEEDBACK_QUEUE_ID) {
#else
		if (source->source->pipeline->pipeline_id != dev->pipeline->pipeline_id) {
#endif
			cd->aec_reference_source = i;
			aec_channels = audio_stream_get_channels(&source->stream);
			comp_dbg(dev, "reference index = %d, channels = %d", i, aec_channels);
		} else {
			cd->raw_microphone_source = i;
			microphone_stream_channels = audio_stream_get_channels(&source->stream);
			comp_dbg(dev, "microphone index = %d, channels = %d", i,
				 microphone_stream_channels);
		}

		audio_stream_init_alignment_constants(1, 1, &source->stream);
		i++;
	}

	output = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);

	/* On some platform the playback output is left right left right due to a crossover
	 * later on the signal processing chain. That makes the aec_reference be 4 channels
	 * and the AEC should only use the 2 first.
	 */
	if (cd->num_aec_reference_channels > aec_channels) {
		comp_err(dev, "unsupported number of AEC reference channels: %d",
			 aec_channels);
		return -EINVAL;
	}

	audio_stream_init_alignment_constants(1, 1, &output->stream);
	frame_fmt = audio_stream_get_frm_fmt(&output->stream);
	rate = audio_stream_get_rate(&output->stream);
	output_stream_channels = audio_stream_get_channels(&output->stream);

	if (cd->num_capture_channels > microphone_stream_channels) {
		comp_err(dev, "unsupported number of microphone channels: %d",
			 microphone_stream_channels);
		return -EINVAL;
	}

	if (cd->num_capture_channels > output_stream_channels) {
		comp_err(dev, "unsupported number of output channels: %d",
			 output_stream_channels);
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
	ret = google_rtc_audio_processing_reconfigure(mod);
	if (ret)
		return ret;

	return 0;
}

static int google_rtc_audio_processing_reset(struct processing_module *mod)
{
	comp_dbg(mod->dev, "google_rtc_audio_processing_reset()");

	return 0;
}

static int google_rtc_audio_processing_process(struct processing_module *mod,
					       struct input_stream_buffer *input_buffers,
					       int num_input_buffers,
					       struct output_stream_buffer *output_buffers,
					       int num_output_buffers)
{
	struct google_rtc_audio_processing_comp_data *cd = module_get_private_data(mod);
	int16_t *src, *dst, *ref;
	uint32_t num_aec_reference_frames;
	uint32_t num_aec_reference_bytes;
	int num_samples_remaining;
	int num_frames_remaining;
	int channel;
	int frames;
	int nmax;
	int ret;
	int i, j, n;

	struct input_stream_buffer *ref_streamb, *mic_streamb;
	struct output_stream_buffer *out_streamb;
	struct audio_stream *ref_stream, *mic_stream, *out_stream;

	if (cd->reconfigure) {
		ret = google_rtc_audio_processing_reconfigure(mod);
		if (ret)
			return ret;
	}

	ref_streamb = &input_buffers[cd->aec_reference_source];
	ref_stream = ref_streamb->data;
	ref = audio_stream_get_rptr(ref_stream);

	num_aec_reference_frames = input_buffers[cd->aec_reference_source].size;
	num_aec_reference_bytes = audio_stream_frame_bytes(ref_stream) * num_aec_reference_frames;

	num_samples_remaining = num_aec_reference_frames * audio_stream_get_channels(ref_stream);
	while (num_samples_remaining) {
		nmax = audio_stream_samples_without_wrap_s16(ref_stream, ref);
		n = MIN(num_samples_remaining, nmax);
		for (i = 0; i < n; i += cd->num_aec_reference_channels) {
			j = cd->num_aec_reference_channels * cd->aec_reference_frame_index;
			for (channel = 0; channel < cd->num_aec_reference_channels; ++channel)
				cd->aec_reference_buffer[j++] = ref[channel];

			ref += audio_stream_get_channels(ref_stream);
			++cd->aec_reference_frame_index;

			if (cd->aec_reference_frame_index == cd->num_frames) {
				GoogleRtcAudioProcessingAnalyzeRender_int16(cd->state,
									    cd->aec_reference_buffer);
				cd->aec_reference_frame_index = 0;
			}
		}
		num_samples_remaining -= n;
		ref = audio_stream_wrap(ref_stream, ref);
	}
	input_buffers[cd->aec_reference_source].consumed = num_aec_reference_bytes;

	mic_streamb = &input_buffers[cd->raw_microphone_source];
	mic_stream = mic_streamb->data;
	out_streamb = &output_buffers[0];
	out_stream = out_streamb->data;

	src = audio_stream_get_rptr(mic_stream);
	dst = audio_stream_get_wptr(out_stream);

	frames = input_buffers[cd->raw_microphone_source].size;
	num_frames_remaining = frames;

	while (num_frames_remaining) {
		nmax = audio_stream_frames_without_wrap(mic_stream, src);
		n = MIN(num_frames_remaining, nmax);
		nmax = audio_stream_frames_without_wrap(out_stream, dst);
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

			src += audio_stream_get_channels(mic_stream);
			dst += audio_stream_get_channels(out_stream);
		}
		num_frames_remaining -= n;
		src = audio_stream_wrap(mic_stream, src);
		dst = audio_stream_wrap(out_stream, dst);
	}

	module_update_buffer_position(&input_buffers[cd->raw_microphone_source],
				      &output_buffers[0], frames);

	return 0;
}

static struct module_interface google_rtc_audio_processing_interface = {
	.init  = google_rtc_audio_processing_init,
	.free = google_rtc_audio_processing_free,
	.process_audio_stream = google_rtc_audio_processing_process,
	.prepare = google_rtc_audio_processing_prepare,
	.set_configuration = google_rtc_audio_processing_set_config,
	.get_configuration = google_rtc_audio_processing_get_config,
	.reset = google_rtc_audio_processing_reset,
};

DECLARE_MODULE_ADAPTER(google_rtc_audio_processing_interface,
		       google_rtc_audio_processing_uuid, google_rtc_audio_processing_tr);
SOF_MODULE_INIT(google_rtc_audio_processing,
		sys_comp_module_google_rtc_audio_processing_interface_init);
