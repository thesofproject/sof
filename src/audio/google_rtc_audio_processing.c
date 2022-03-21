// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Google LLC.
//
// Author: Lionel Koenig <lionelk@google.com>
#include <errno.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/kpb.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
#include <sof/lib/alloc.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/lib/wait.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/string.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <user/trace.h>

#include <google_rtc_audio_processing.h>
#include <google_rtc_audio_processing_platform.h>

#define GOOGLE_RTC_AUDIO_PROCESSING_SAMPLERATE 48000

/* b780a0a6-269f-466f-b477-23dfa05af758 */
DECLARE_SOF_RT_UUID("google-rtc-audio-processing", google_rtc_audio_processing_uuid,
					0xb780a0a6, 0x269f, 0x466f, 0xb4, 0x77, 0x23, 0xdf, 0xa0,
					0x5a, 0xf7, 0x58);
DECLARE_TR_CTX(google_rtc_audio_processing_tr, SOF_UUID(google_rtc_audio_processing_uuid),
			   LOG_LEVEL_INFO);

struct google_rtc_audio_processing_comp_data {
	struct comp_buffer *raw_microphone;
	struct comp_buffer *aec_reference;
	struct comp_buffer *output;
	uint32_t num_frames;
	int num_aec_reference_channels;
	GoogleRtcAudioProcessingState *state;
	int16_t *aec_reference_buffer;
	int16_t aec_reference_frame_index;
	int16_t *raw_mic_buffer;
	int16_t raw_mic_buffer_index;
	int16_t *output_buffer;
	int16_t output_buffer_index;
	struct comp_data_blob_handler *tuning_handler;
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

	ret = GoogleRtcAudioProcessingReconfigure(cd->state, config, size);
	if (ret) {
		comp_err(dev, "GoogleRtcAudioProcessingReconfigure failed: %d",
			 ret);
		return -EINVAL;
	}

	return 0;
}

static int google_rtc_audio_processing_cmd_set_data(
	struct comp_dev *dev,
	struct sof_ipc_ctrl_data *cdata)
{
	struct google_rtc_audio_processing_comp_data *cd = comp_get_drvdata(dev);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		return comp_data_blob_set_cmd(cd->tuning_handler, cdata);
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
	struct comp_ipc_config *config,
	void *spec)
{
	struct comp_dev *dev;
	struct google_rtc_audio_processing_comp_data *cd;

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

	cd->tuning_handler = comp_data_blob_handler_new(dev);
	if (!cd->tuning_handler)
		goto fail;

	cd->state = GoogleRtcAudioProcessingCreate();
	if (!cd->state) {
		comp_err(dev, "Failed to initialized GoogleRtcAudioProcessing");
		goto fail;
	}
	cd->num_aec_reference_channels = 2;
	cd->num_frames = GOOGLE_RTC_AUDIO_PROCESSING_SAMPLERATE *
					 GoogleRtcAudioProcessingGetFramesizeInMs(cd->state) / 1000;

	cd->raw_mic_buffer = rballoc(
		0, SOF_MEM_CAPS_RAM,
		cd->num_frames * sizeof(cd->raw_mic_buffer[0]));
	if (!cd->raw_mic_buffer)
		goto fail;
	bzero(cd->raw_mic_buffer, cd->num_frames * sizeof(cd->raw_mic_buffer[0]));
	cd->raw_mic_buffer_index = 0;

	cd->aec_reference_buffer = rballoc(
		0, SOF_MEM_CAPS_RAM,
		cd->num_frames * sizeof(cd->aec_reference_buffer[0]) *
		cd->num_aec_reference_channels);
	if (!cd->aec_reference_buffer)
		goto fail;
	bzero(cd->aec_reference_buffer, cd->num_frames * sizeof(cd->aec_reference_buffer[0]));
	cd->aec_reference_frame_index = 0;

	cd->output_buffer = rballoc(
		0, SOF_MEM_CAPS_RAM,
		cd->num_frames * sizeof(cd->output_buffer[0]));
	if (!cd->output_buffer)
		goto fail;
	bzero(cd->output_buffer, cd->num_frames * sizeof(cd->output_buffer[0]));
	cd->output_buffer_index = 0;

	comp_set_drvdata(dev, cd);
	dev->state = COMP_STATE_READY;
	comp_dbg(dev, "google_rtc_audio_processing_create(): Ready");
	return dev;
fail:
	comp_err(dev, "google_rtc_audio_processing_create(): Failed");
	if (cd) {
		rfree(cd->output_buffer);
		rfree(cd->aec_reference_buffer);
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
	struct comp_buffer *source_buffer;
	struct list_item *source_buffer_list_item;
	int ret;

	comp_dbg(dev, "google_rtc_audio_processing_prepare()");

	/* searching for stream and feedback source buffers */
	list_for_item(source_buffer_list_item, &dev->bsource_list) {
		source_buffer = container_of(source_buffer_list_item, struct comp_buffer,
						 sink_list);
		source_buffer = buffer_acquire(source_buffer);
		if (source_buffer->source->ipc_config.type == SOF_COMP_DEMUX)
			cd->aec_reference = source_buffer;
		else if (source_buffer->source->ipc_config.type == SOF_COMP_DAI)
			cd->raw_microphone = source_buffer;
		source_buffer = buffer_release(source_buffer);
	}

	cd->output = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);

	/* On some platform the playback output is left right left right due to a crossover
	 * later on on the signal processing chain. That makes the aec_reference be 4 channels
	 * and the AEC should only use the 2 first.
	 */
	if (cd->num_aec_reference_channels > cd->aec_reference->stream.channels) {
		comp_err(dev, "unsupported number of AEC reference channels: %d",
			 cd->aec_reference->stream.channels);
		return -EINVAL;
	}

	switch (cd->output->stream.frame_fmt) {
#if CONFIG_FORMAT_S16LE
	case SOF_IPC_FRAME_S16_LE:
		break;
#endif /* CONFIG_FORMAT_S16LE */
	default:
		comp_err(dev, "unsupported data format: %d", cd->output->stream.frame_fmt);
		return -EINVAL;
	}

	if (cd->output->stream.rate != GOOGLE_RTC_AUDIO_PROCESSING_SAMPLERATE) {
		comp_err(dev, "unsupported samplerate: %d", cd->output->stream.rate);
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

	struct comp_copy_limits cl;
	int frame;
	uint32_t aec_reference_buff_frag;
	uint32_t raw_microphone_buff_frag;
	uint32_t output_buff_frag;
	int16_t *src;
	int16_t *dst;
	uint32_t num_aec_reference_frames;
	uint32_t num_aec_reference_bytes;
	int channel;
	int ret;

	if (comp_is_new_data_blob_available(cd->tuning_handler)) {
		ret = google_rtc_audio_processing_reconfigure(dev);
		if (ret)
			return ret;
	}

	cd->aec_reference = buffer_acquire(cd->aec_reference);
	num_aec_reference_frames = audio_stream_get_avail_frames(&cd->aec_reference->stream);
	num_aec_reference_bytes = audio_stream_get_avail_bytes(&cd->aec_reference->stream);
	cd->aec_reference = buffer_release(cd->aec_reference);

	buffer_stream_invalidate(cd->aec_reference, num_aec_reference_bytes);

	aec_reference_buff_frag = 0;
	for (frame = 0; frame < num_aec_reference_frames; frame++) {
		for (channel = 0; channel < cd->num_aec_reference_channels; ++channel) {
			src = audio_stream_read_frag_s16(&cd->aec_reference->stream,
							 aec_reference_buff_frag + channel);
			cd->aec_reference_buffer[cd->num_aec_reference_channels *
				cd->aec_reference_frame_index + channel] = *src;
		}
		++cd->aec_reference_frame_index;

		if (cd->aec_reference_frame_index == cd->num_frames) {
			GoogleRtcAudioProcessingAnalyzeRender_int16(cd->state,
								    cd->aec_reference_buffer);
			cd->aec_reference_frame_index = 0;
		}
		aec_reference_buff_frag += cd->aec_reference->stream.channels;
	}
	comp_update_buffer_consume(cd->aec_reference, num_aec_reference_bytes);

	comp_get_copy_limits_with_lock(cd->raw_microphone, cd->output, &cl);
	buffer_stream_invalidate(cd->raw_microphone, cl.source_bytes);

	raw_microphone_buff_frag = 0;
	output_buff_frag = 0;
	for (frame = 0; frame < cl.frames; frame++) {
		src = audio_stream_read_frag_s16(&cd->raw_microphone->stream,
										 raw_microphone_buff_frag);
		cd->raw_mic_buffer[cd->raw_mic_buffer_index] = *src;
		++cd->raw_mic_buffer_index;

		dst = audio_stream_write_frag_s16(&cd->output->stream, output_buff_frag);
		*dst = cd->output_buffer[cd->output_buffer_index];
		++cd->output_buffer_index;

		if (cd->raw_mic_buffer_index == cd->num_frames) {
			GoogleRtcAudioProcessingProcessCapture_int16(
				cd->state, cd->raw_mic_buffer,
				cd->output_buffer);
			cd->output_buffer_index = 0;
			cd->raw_mic_buffer_index = 0;
		}

		raw_microphone_buff_frag += cd->raw_microphone->stream.channels;
		output_buff_frag += cd->output->stream.channels;
	}

	buffer_stream_writeback(cd->output, cl.sink_bytes);

	comp_update_buffer_produce(cd->output, cl.sink_bytes);
	comp_update_buffer_consume(cd->raw_microphone, cl.source_bytes);
	return 0;
}

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
