// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

#include <sof/compiler_attributes.h>
#include <sof/samples/audio/smart_amp_test.h>
#include <sof/audio/ipc-config.h>
#include <sof/trace/trace.h>
#include <sof/ipc/msg.h>
#include <rtos/init.h>
#include <sof/ut.h>

#ifndef __ZEPHYR__
#include <rtos/mutex.h>
#endif

static const struct comp_driver comp_smart_amp;

LOG_MODULE_REGISTER(smart_amp_test, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(smart_amp_test);

DECLARE_TR_CTX(smart_amp_test_comp_tr, SOF_UUID(smart_amp_test_uuid),
	       LOG_LEVEL_INFO);

typedef int(*smart_amp_proc)(struct comp_dev *dev,
			     const struct audio_stream *source,
			     const struct audio_stream *sink, uint32_t frames,
			     int8_t *chan_map);

struct smart_amp_data {
	struct sof_smart_amp_config config;
	struct comp_data_blob_handler *model_handler;
	void *data_blob;
	size_t data_blob_size;

	struct comp_buffer *source_buf; /**< stream source buffer */
	struct comp_buffer *feedback_buf; /**< feedback source buffer */
	struct comp_buffer *sink_buf; /**< sink buffer */

	smart_amp_proc process;

	uint32_t in_channels;
	uint32_t out_channels;
};

static struct comp_dev *smart_amp_new(const struct comp_driver *drv,
				      const struct comp_ipc_config *config,
				      const void *spec)
{
	const struct ipc_config_process *ipc_sa = spec;
	const struct sof_smart_amp_config *cfg;
	struct smart_amp_data *sad;
	struct comp_dev *dev;
	size_t bs;

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;
	dev->ipc_config = *config;

	sad = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*sad));
	if (!sad)
		goto fail;

	comp_set_drvdata(dev, sad);

	/* component model data handler */
	sad->model_handler = comp_data_blob_handler_new(dev);
	if (!sad->model_handler)
		goto sad_fail;


	cfg = (struct sof_smart_amp_config *)ipc_sa->data;
	bs = ipc_sa->size;
	if (bs > 0 && bs < sizeof(struct sof_smart_amp_config)) {
		comp_err(dev, "smart_amp_new(): failed to apply config");
		goto sad_fail;
	}
	memcpy_s(&sad->config, sizeof(struct sof_smart_amp_config), cfg, bs);
	dev->state = COMP_STATE_READY;

	return dev;

sad_fail:
	comp_data_blob_handler_free(sad->model_handler);
	rfree(sad);
fail:
	rfree(dev);
	return NULL;
}

static void smart_amp_set_params(struct comp_dev *dev,
				 struct sof_ipc_stream_params *params)
{
	/* nothing to do */
}

static int smart_amp_set_config(struct comp_dev *dev,
				struct sof_ipc_ctrl_data *cdata)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	struct sof_smart_amp_config *cfg;
	size_t bs;

	/* Copy new config, find size from header */
	cfg = (struct sof_smart_amp_config *)
	       ASSUME_ALIGNED(&cdata->data->data, sizeof(uint32_t));
	bs = cfg->size;

	comp_dbg(dev, "smart_amp_set_config(), actual blob size = %zu, expected blob size = %zu",
		 bs, sizeof(struct sof_smart_amp_config));

	if (bs != sizeof(struct sof_smart_amp_config)) {
		comp_err(dev, "smart_amp_set_config(): invalid blob size, actual blob size = %zu, expected blob size = %zu",
			 bs, sizeof(struct sof_smart_amp_config));
		return -EINVAL;
	}

	memcpy_s(&sad->config, sizeof(struct sof_smart_amp_config), cfg,
		 sizeof(struct sof_smart_amp_config));

	return 0;
}

static int smart_amp_get_config(struct comp_dev *dev,
				struct sof_ipc_ctrl_data *cdata, int size)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	size_t bs;
	int ret;

	/* Copy back to user space */
	bs = sad->config.size;

	comp_dbg(dev, "smart_amp_set_config(), actual blob size = %zu, expected blob size = %zu",
		 bs, sizeof(struct sof_smart_amp_config));

	if (bs == 0 || bs > size)
		return -EINVAL;

	ret = memcpy_s(cdata->data->data, size, &sad->config, bs);
	assert(!ret);

	cdata->data->abi = SOF_ABI_VERSION;
	cdata->data->size = bs;

	return ret;
}

static int smart_amp_ctrl_get_bin_data(struct comp_dev *dev,
				       struct sof_ipc_ctrl_data *cdata,
				       int size)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);

	assert(sad);

	switch (cdata->data->type) {
	case SOF_SMART_AMP_CONFIG:
		return smart_amp_get_config(dev, cdata, size);
	case SOF_SMART_AMP_MODEL:
		return comp_data_blob_get_cmd(sad->model_handler, cdata, size);
	default:
		comp_warn(dev, "smart_amp_ctrl_get_bin_data(): unknown binary data type");
		break;
	}

	return 0;
}

static int smart_amp_ctrl_get_data(struct comp_dev *dev,
				   struct sof_ipc_ctrl_data *cdata, int size)
{
	comp_info(dev, "smart_amp_ctrl_get_data() size: %d", size);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		return smart_amp_ctrl_get_bin_data(dev, cdata, size);
	default:
		comp_err(dev, "smart_amp_ctrl_get_data(): invalid cdata->cmd");
		return -EINVAL;
	}
}

static int smart_amp_ctrl_set_bin_data(struct comp_dev *dev,
				       struct sof_ipc_ctrl_data *cdata)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);

	assert(sad);

	if (dev->state < COMP_STATE_READY) {
		comp_err(dev, "smart_amp_ctrl_set_bin_data(): driver in init!");
		return -EBUSY;
	}

	switch (cdata->data->type) {
	case SOF_SMART_AMP_CONFIG:
		return smart_amp_set_config(dev, cdata);
	case SOF_SMART_AMP_MODEL:
		return comp_data_blob_set_cmd(sad->model_handler, cdata);
	default:
		comp_warn(dev, "smart_amp_ctrl_set_bin_data(): unknown binary data type");
		break;
	}

	return 0;
}

static int smart_amp_ctrl_set_data(struct comp_dev *dev,
				   struct sof_ipc_ctrl_data *cdata)
{
	/* Check version from ABI header */
	if (SOF_ABI_VERSION_INCOMPATIBLE(SOF_ABI_VERSION, cdata->data->abi)) {
		comp_err(dev, "smart_amp_ctrl_set_data(): invalid version");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		comp_info(dev, "smart_amp_ctrl_set_data(), SOF_CTRL_CMD_ENUM");
		break;
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "smart_amp_ctrl_set_data(), SOF_CTRL_CMD_BINARY");
		return smart_amp_ctrl_set_bin_data(dev, cdata);
	default:
		comp_err(dev, "smart_amp_ctrl_set_data(): invalid cdata->cmd");
		return -EINVAL;
	}

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int smart_amp_cmd(struct comp_dev *dev, int cmd, void *data,
			 int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);

	comp_info(dev, "smart_amp_cmd(): cmd: %d", cmd);

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		return smart_amp_ctrl_set_data(dev, cdata);
	case COMP_CMD_GET_DATA:
		return smart_amp_ctrl_get_data(dev, cdata, max_data_size);
	default:
		return -EINVAL;
	}
}

static void smart_amp_free(struct comp_dev *dev)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);

	comp_info(dev, "smart_amp_free()");

	comp_data_blob_handler_free(sad->model_handler);

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
		comp_err(dev, "smart_amp_verify_params() error: comp_verify_params() failed.");
		return ret;
	}

	return 0;
}

static int smart_amp_params(struct comp_dev *dev,
			    struct sof_ipc_stream_params *params)
{
	int err;

	comp_info(dev, "smart_amp_params()");

	smart_amp_set_params(dev, params);

	err = smart_amp_verify_params(dev, params);
	if (err < 0) {
		comp_err(dev, "smart_amp_params(): pcm params verification failed.");
		return err;
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
		return PPL_STATUS_PATH_STOP;

	switch (cmd) {
	case COMP_TRIGGER_START:
	case COMP_TRIGGER_RELEASE:
		if (sad->feedback_buf) {
			buffer_zero(sad->feedback_buf);
		}
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
				 uint32_t frames, int8_t *chan_map)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	int16_t *src;
	int16_t *dest;
	uint32_t in_frag = 0;
	uint32_t out_frag = 0;
	int i;
	int j;

	comp_dbg(dev, "smart_amp_process_s16()");

	for (i = 0; i < frames; i++) {
		for (j = 0 ; j < sad->out_channels; j++) {
			if (chan_map[j] != -1) {
				src = audio_stream_read_frag_s16(source,
								 in_frag +
								 chan_map[j]);
				dest = audio_stream_write_frag_s16(sink,
								   out_frag);
				*dest = *src;
			}
			out_frag++;
		}
		in_frag += audio_stream_get_channels(source);
	}
	return 0;
}

static int smart_amp_process_s32(struct comp_dev *dev,
				 const struct audio_stream *source,
				 const struct audio_stream *sink,
				 uint32_t frames, int8_t *chan_map)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	int32_t *src;
	int32_t *dest;
	uint32_t in_frag = 0;
	uint32_t out_frag = 0;
	int i;
	int j;

	comp_dbg(dev, "smart_amp_process_s32()");

	for (i = 0; i < frames; i++) {
		for (j = 0 ; j < sad->out_channels; j++) {
			if (chan_map[j] != -1) {
				src = audio_stream_read_frag_s32(source,
								 in_frag +
								 chan_map[j]);
				dest = audio_stream_write_frag_s32(sink,
								   out_frag);
				*dest = *src;
			}
			out_frag++;
		}
		in_frag += audio_stream_get_channels(source);
	}

	return 0;
}

static smart_amp_proc get_smart_amp_process(struct comp_dev *dev,
					    struct comp_buffer *buf)
{
	switch (audio_stream_get_frm_fmt(&buf->stream)) {
	case SOF_IPC_FRAME_S16_LE:
		return smart_amp_process_s16;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		return smart_amp_process_s32;
	default:
		comp_err(dev, "smart_amp_process() error: not supported frame format");
		return NULL;
	}
}

static int smart_amp_copy(struct comp_dev *dev)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	struct comp_buffer *source_buf = sad->source_buf;
	struct comp_buffer *sink_buf = sad->sink_buf;
	uint32_t avail_passthrough_frames;
	uint32_t avail_feedback_frames;
	uint32_t avail_frames = 0;
	uint32_t source_bytes;
	uint32_t sink_bytes;
	uint32_t feedback_bytes;

	comp_dbg(dev, "smart_amp_copy()");

	/* available bytes and samples calculation */
	avail_passthrough_frames =
		audio_stream_avail_frames(&source_buf->stream,
					  &sink_buf->stream);

	if (sad->feedback_buf) {
		struct comp_buffer *buf = sad->feedback_buf;

		if (comp_buffer_get_source_state(buf) == dev->state) {
			/* feedback */
			avail_feedback_frames =
				audio_stream_get_avail_frames(&buf->stream);

			avail_frames = MIN(avail_passthrough_frames,
					   avail_feedback_frames);

			feedback_bytes = avail_frames *
				audio_stream_frame_bytes(&buf->stream);

			comp_dbg(dev, "smart_amp_copy(): processing %d feedback frames (avail_passthrough_frames: %d)",
				 avail_frames, avail_passthrough_frames);

			/* perform buffer writeback after source_buf process */
			buffer_stream_invalidate(buf, feedback_bytes);
			sad->process(dev, &buf->stream, &sink_buf->stream,
				     avail_frames, sad->config.feedback_ch_map);

			comp_update_buffer_consume(buf, feedback_bytes);
		}
	}

	if (!avail_frames)
		avail_frames = avail_passthrough_frames;
	/* bytes calculation */
	source_bytes = avail_frames *
		audio_stream_frame_bytes(&source_buf->stream);

	sink_bytes = avail_frames *
		audio_stream_frame_bytes(&sink_buf->stream);

	/* process data */
	buffer_stream_invalidate(source_buf, source_bytes);
	sad->process(dev, &source_buf->stream, &sink_buf->stream,
		     avail_frames, sad->config.source_ch_map);
	buffer_stream_writeback(sink_buf, sink_bytes);

	/* source/sink buffer pointers update */
	comp_update_buffer_consume(source_buf, source_bytes);
	comp_update_buffer_produce(sink_buf, sink_bytes);

	return 0;
}

static int smart_amp_reset(struct comp_dev *dev)
{
	comp_info(dev, "smart_amp_reset()");

	comp_set_state(dev, COMP_TRIGGER_RESET);

	return 0;
}

static int smart_amp_prepare(struct comp_dev *dev)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	struct comp_buffer *source_buffer;
	int ret;

	comp_info(dev, "smart_amp_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* searching for stream and feedback source buffers */
	comp_dev_for_each_producer(dev, source_buffer) {
		/* FIXME: how often can this loop be run? */
		if (comp_buffer_get_source_component(source_buffer)->ipc_config.type ==
				SOF_COMP_DEMUX)
			sad->feedback_buf = source_buffer;
		else
			sad->source_buf = source_buffer;
	}

	sad->sink_buf = comp_dev_get_first_data_consumer(dev);
	if (!sad->sink_buf) {
		comp_err(dev, "no sink buffer");
		return -ENOTCONN;
	}

	sad->out_channels = audio_stream_get_channels(&sad->sink_buf->stream);

	sad->in_channels = audio_stream_get_channels(&sad->source_buf->stream);

	if (sad->feedback_buf) {
		audio_stream_set_channels(&sad->feedback_buf->stream,
					  sad->config.feedback_channels);
		audio_stream_set_rate(&sad->feedback_buf->stream,
				      audio_stream_get_rate(&sad->source_buf->stream));
	}

	sad->process = get_smart_amp_process(dev, sad->source_buf);
	if (!sad->process) {
		comp_err(dev, "smart_amp_prepare(): get_smart_amp_process failed");
		ret = -EINVAL;
	}
	return ret;
}

static const struct comp_driver comp_smart_amp = {
	.type = SOF_COMP_SMART_AMP,
	.uid = SOF_RT_UUID(smart_amp_test_uuid),
	.tctx = &smart_amp_test_comp_tr,
	.ops = {
		.create			= smart_amp_new,
		.free			= smart_amp_free,
		.params			= smart_amp_params,
		.prepare		= smart_amp_prepare,
		.cmd			= smart_amp_cmd,
		.trigger		= smart_amp_trigger,
		.copy			= smart_amp_copy,
		.reset			= smart_amp_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_smart_amp_info = {
	.drv = &comp_smart_amp,
};

UT_STATIC void sys_comp_smart_amp_test_init(void)
{
	comp_register(platform_shared_get(&comp_smart_amp_info,
					  sizeof(comp_smart_amp_info)));
}

DECLARE_MODULE(sys_comp_smart_amp_test_init);
SOF_MODULE_INIT(smart_amp_test, sys_comp_smart_amp_test_init);
