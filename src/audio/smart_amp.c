// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

#include <sof/audio/smart_amp.h>
#include <sof/audio/component.h>
#include <sof/trace/trace.h>
#include <sof/drivers/ipc.h>
#include <sof/ut.h>

static const struct comp_driver comp_smart_amp;

int source_channels_maps[SMART_AMP_MODE_AMOUNT][SMART_AMP_MAX_STREAM_CHAN] = {
	{1, 0, 1, 0, 1, 0, 1, 0},	// SMART_AMP_MODE_PASSTHROUGH
	{0, 0, 1, 1, -1, -1, -1, -1},	// SMART_AMP_MODE_PROC_FEEDBACK
	{0, 1, -1, -1, -1, -1, -1, -1}	// SMART_AMP_MODE_NOCODEC_FEEDBACK
};

int feedback_channels_maps[SMART_AMP_MODE_AMOUNT][SMART_AMP_MAX_STREAM_CHAN] = {
	{-1, -1, -1, -1, -1, -1, -1, -1},	// SMART_AMP_MODE_PASSTHROUGH
	{-1, -1, -1, -1, 0, 1, 2, 3},		// SMART_AMP_MODE_PROC_FEEDBACK
	{-1, -1, 0, 1, -1, -1, -1, -1}		// SMART_AMP_MODE_NOCODEC_FEEDBACK
};

struct smart_amp_data {
	struct comp_buffer *source_buf; /**< stream source buffer */
	struct comp_buffer *feedback_buf; /**< feedback source buffer */
	struct comp_buffer *sink_buf; /**< sink buffer */

	uint32_t mode;

	uint32_t in_channels;
	uint32_t out_channels;
	int *source_channel_map;
	int *feedback_channel_map;
};

static struct comp_dev *smart_amp_new(const struct comp_driver *drv,
				      struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_process *sa;
	struct sof_ipc_comp_process *ipc_sa =
		(struct sof_ipc_comp_process *)comp;
	struct smart_amp_data *sad;

	comp_cl_info(&comp_smart_amp, "smart_amp_new()");

	if (IPC_IS_SIZE_INVALID(ipc_sa->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_SMART_AMP, ipc_sa->config);
		return NULL;
	}

	dev = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;
	dev->drv = drv;

	sa = (struct sof_ipc_comp_process *)&dev->comp;

	assert(!memcpy_s(sa, sizeof(*sa), ipc_sa,
	       sizeof(struct sof_ipc_comp_process)));

	sad = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*sad));

	if (!sad) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, sad);

	//sad->mode = SMART_AMP_MODE_PASSTHROUGH;
	//sad->mode = SMART_AMP_MODE_PROC_FEEDBACK;
	sad->mode = SMART_AMP_MODE_NOCODEC_FEEDBACK;

	sad->source_channel_map = &source_channels_maps[sad->mode][0];
	sad->feedback_channel_map = &feedback_channels_maps[sad->mode][0];

	dev->state = COMP_STATE_READY;

	return dev;
}

static void smart_amp_free(struct comp_dev *dev)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);

	comp_info(dev, "smart_amp_free()");

	rfree(sad);
	rfree(dev);
}

static int smart_amp_verify_params(struct comp_dev *dev,
				   struct sof_ipc_stream_params *params)
{
	int ret;

	comp_info(dev, "smart_amp_verify_params()");

	ret = comp_verify_params(dev, BUFF_PARAMS_CHANNELS, params);
	if (ret < 0) {
		comp_err(dev, "volume_verify_params() error: comp_verify_params() failed.");
		return ret;
	}

	return 0;
}

static int smart_amp_params(struct comp_dev *dev,
			    struct sof_ipc_stream_params *params)
{
	int err;

	comp_info(dev, "smart_amp_params()");

	err = smart_amp_verify_params(dev, params);
	if (err < 0) {
		comp_err(dev, "smart_amp_params(): pcm params verification failed.");
		return -EINVAL;
	}

	return 0;
}

static int smart_amp_trigger(struct comp_dev *dev, int cmd)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	int ret = 0;

	comp_info(dev, "smart_amp_trigger(), command = %u", cmd);

	ret = comp_set_state(dev, cmd);

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		ret = PPL_STATUS_PATH_STOP;

	switch (cmd) {
	case COMP_TRIGGER_START:
	case COMP_TRIGGER_RELEASE:
		buffer_zero(sad->feedback_buf);
		break;
	case COMP_TRIGGER_PAUSE:
	case COMP_TRIGGER_STOP:
		break;
	default:
		break;
	}

	return ret;
}

static int smart_amp_process_s16(struct comp_dev *dev,
				 const struct audio_stream *source,
				 const struct audio_stream *sink,
				 uint32_t frames, int *chan_map)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	int16_t *src;
	int16_t *dest;
	uint32_t in_buff_frag = 0;
	uint32_t out_buff_frag = 0;
	int i;
	int j;

	comp_info(dev, "smart_amp_process_s16()");

	for (i = 0; i < frames; i++) {
		for (j = 0 ; j < sad->out_channels; j++) {
			if (chan_map[j] != -1) {
				src = audio_stream_read_frag_s16(source, in_buff_frag + chan_map[j]);	
				dest = audio_stream_write_frag_s16(sink, out_buff_frag);
				*dest = *src;
			}
			out_buff_frag++;
		}
		in_buff_frag += source->channels;
	}
	return 0;
}

static int smart_amp_process_s32(struct comp_dev *dev,
				 const struct audio_stream *source,
				 const struct audio_stream *sink,
				 uint32_t frames, int *chan_map)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	int32_t *src;
	int32_t *dest;
	uint32_t in_buff_frag = 0;
	uint32_t out_buff_frag = 0;
	int i;
	int j;

	comp_info(dev, "smart_amp_process_s32()");

	for (i = 0; i < frames; i++) {
		for (j = 0 ; j < sad->out_channels; j++) {
			if (chan_map[j] != -1) {
				src = audio_stream_read_frag_s32(source, in_buff_frag + chan_map[j]);	
				dest = audio_stream_write_frag_s32(sink, out_buff_frag);
				*dest = *src;
			}
			out_buff_frag++;
		}
		in_buff_frag += source->channels;
	}

	return 0;
}

static int smart_amp_process(struct comp_dev *dev, uint32_t frames,
			     struct comp_buffer *source,
			     struct comp_buffer *sink, int *chan_map)
{
	int ret = 0;

	switch (source->stream.frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		ret = smart_amp_process_s16(dev, &source->stream, &sink->stream,
					    frames, chan_map);
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		ret = smart_amp_process_s32(dev, &source->stream, &sink->stream,
					    frames, chan_map);

		break;
	default:
		comp_err(dev, "smart_amp_process() error: not supported frame format");
		return -EINVAL;
	}

	return ret;
}

static int smart_amp_copy(struct comp_dev *dev)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	uint32_t avail_frames;
	uint32_t source_bytes;
	uint32_t sink_bytes;
	uint32_t feedback_bytes;
	uint32_t feedback_frames;
	int ret = 0;

	comp_info(dev, "smart_amp_copy()");

	/* available bytes and samples calculation */
	avail_frames = audio_stream_avail_frames(&sad->source_buf->stream,
						 &sad->sink_buf->stream);
	source_bytes = avail_frames *
		audio_stream_frame_bytes(&sad->source_buf->stream);
	sink_bytes = avail_frames *
		audio_stream_frame_bytes(&sad->sink_buf->stream);

	/* process data */
	smart_amp_process(dev, avail_frames, sad->source_buf, sad->sink_buf,
			  sad->source_channel_map);

	/* source buffer pointers update */
	comp_update_buffer_consume(sad->source_buf, source_bytes);

	/* processing feedback */
	feedback_frames = sad->feedback_buf->stream.avail /
		audio_stream_frame_bytes(&sad->feedback_buf->stream);

	avail_frames = MIN(avail_frames, feedback_frames);

	/* available bytes and samples calculation */
	feedback_bytes = avail_frames *
		audio_stream_frame_bytes(&sad->feedback_buf->stream);

	comp_info(dev, "smart_amp_copy(): processing %d feedback bytes",
		  feedback_bytes);
	smart_amp_process(dev, avail_frames, sad->feedback_buf, sad->sink_buf,
			  sad->feedback_channel_map);
	comp_update_buffer_produce(sad->sink_buf, sink_bytes);
	comp_update_buffer_consume(sad->feedback_buf, feedback_bytes);

	return ret;
}

static int smart_amp_reset(struct comp_dev *dev)
{
	comp_info(dev, "smart_amp_reset()");

	comp_set_state(dev, COMP_TRIGGER_RESET);

	return 0;
}

static int smart_amp_prepare(struct comp_dev *dev)
{
	struct sof_ipc_comp_process *ipc_sa =
		(struct sof_ipc_comp_process *)&dev->comp;
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	struct comp_buffer *source_buffer;
	struct list_item *blist;
	int ret;

	(void)ipc_sa;

	comp_info(dev, "smart_amp_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* searching for stream and feedback source buffers */
	list_for_item(blist, &dev->bsource_list) {
		source_buffer = container_of(blist, struct comp_buffer,
					     sink_list);

		if (source_buffer->source->comp.type == SOF_COMP_DEMUX)
			sad->feedback_buf = source_buffer;
		else
			sad->source_buf = source_buffer;
	}

	sad->sink_buf = list_first_item(&dev->bsink_list, struct comp_buffer,
					source_list);

	sad->in_channels = sad->source_buf->stream.channels;
	sad->out_channels = sad->sink_buf->stream.channels;

	sad->feedback_buf->stream.channels = 8;
	sad->feedback_buf->stream.frame_fmt = 2;

	return 0;
}

static const struct comp_driver comp_smart_amp = {
	.type = SOF_COMP_SMART_AMP,
	.ops = {
		.create = smart_amp_new,
		.free = smart_amp_free,
		.params = smart_amp_params,
		.prepare = smart_amp_prepare,
		.trigger = smart_amp_trigger,
		.copy = smart_amp_copy,
		.reset = smart_amp_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_smart_amp_info = {
	.drv = &comp_smart_amp,
};

static void sys_comp_smart_amp_init(void)
{
	comp_register(platform_shared_get(&comp_smart_amp_info,
					  sizeof(comp_smart_amp_info)));
}

DECLARE_MODULE(sys_comp_smart_amp_init);
