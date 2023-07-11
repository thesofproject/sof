// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 awinic Integrated All rights reserved.
//
// Author: Jimmy Zhang <zhangjianming@awinic.com>

#include <rtos/init.h>
#include <sof/trace/trace.h>
#include <sof/ipc/msg.h>
#include <sof/ut.h>
#include <user/smart_amp.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/smart_amp/aw_smart_amp.h>

static const struct comp_driver comp_smart_amp;

/* 0cd84e80-ebd3-11ea-adc1-0242ac120002 */
DECLARE_SOF_RT_UUID("Awinic SKTune", awinic_sktune_comp_uuid, 0x0cd84e80, 0xebd3,
		    0x11ea, 0xad, 0xc1, 0x02, 0x42, 0xac, 0x12, 0x00, 0x02);

DECLARE_TR_CTX(awinic_sktune_comp_tr, SOF_UUID(awinic_sktune_comp_uuid),
	       LOG_LEVEL_INFO);

/* Amp configuration & model calibration data for tuning/debug */
#define SOF_SMART_AMP_CONFIG 0
#define SOF_SMART_AMP_MODEL 1


struct smart_amp_data {
	struct sof_smart_amp_config config;
	struct comp_buffer *source_buf; /**< stream source buffer */
	struct comp_buffer *feedback_buf; /**< feedback source buffer */
	struct comp_buffer *sink_buf; /**< sink buffer */
	uint32_t in_channels;
	uint32_t out_channels;
	struct ipc_config_process ipc_config;
	/* module handle for speaker protection algorithm */
	sktune_t *algo_handle;
};

static struct comp_dev *smart_amp_comp_new(const struct comp_driver *drv,
				      const struct comp_ipc_config *config,
				      const void *spec)
{
	struct comp_dev *dev;
	const struct ipc_config_process *ipc_cfg = spec;
	struct smart_amp_data *amp_data;
	struct sof_smart_amp_config *cfg;
	size_t block_size;	
	int ret;

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	dev->ipc_config = *config;

	amp_data = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*amp_data));
	if (!amp_data) {
		comp_err(dev, "amp_data alloc failed");
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, amp_data);
	amp_data->ipc_config = *ipc_cfg;

	cfg = (struct sof_smart_amp_config *)ipc_cfg->data;  
	block_size = ipc_cfg->size;

	if (block_size > 0 && block_size < sizeof(struct sof_smart_amp_config)) {
		comp_err(dev, "smart_amp_new(): failed to apply config");
		goto error;
	}

	memcpy_s(&amp_data->config, sizeof(struct sof_smart_amp_config), cfg, block_size);

	/*memory allocation for sktune*/
	amp_data->algo_handle = smart_amp_sktune_alloc(dev);
	if (!amp_data->algo_handle) {
		comp_err(dev, "[Awinic] SKTune alloc failed!");
		goto error;
	}


	/* Bitwidth information is not available. Use 16bit as default.*/
	amp_data->algo_handle->media_info.bit_per_sample = 16;
	amp_data->algo_handle->media_info.bit_qactor_sample = 15;
	amp_data->algo_handle->media_info.num_channel = 2;
	amp_data->algo_handle->media_info.sample_rate = 48000;
	amp_data->algo_handle->media_info.data_is_signed = 1;

	ret = smart_amp_init(amp_data->algo_handle, dev);
	if (ret) {
		comp_err(dev, "[Awinic] samrt amp init failed!");
		goto error;
	}

	dev->state = COMP_STATE_READY;

	return dev;

error:
	rfree(amp_data->algo_handle);
	rfree(amp_data);
	rfree(dev);
	return NULL;
}

static int smart_amp_set_config(struct comp_dev *dev,
				struct sof_ipc_ctrl_data *cdata)
{
	struct smart_amp_data *smartpa_data = comp_get_drvdata(dev);
	struct sof_smart_amp_config *cfg;
	size_t bs;

	/* Copy new config, find size from header */
	cfg = (struct sof_smart_amp_config *)
	       ASSUME_ALIGNED(&cdata->data->data, sizeof(uint32_t));
	bs = cfg->size;

	comp_dbg(dev, "[Awinic] smart_amp_set_config(), actual blob size = %u, expected blob size = %u",
		 bs, sizeof(struct sof_smart_amp_config));

	if (bs != sizeof(struct sof_smart_amp_config)) {
		comp_err(dev, "[Awinic] smart_amp_set_config(): invalid blob size, actual blob size = %u, expected blob size = %u",
			 bs, sizeof(struct sof_smart_amp_config));
		return -EINVAL;
	}

	memcpy_s(&smartpa_data->config, sizeof(struct sof_smart_amp_config), cfg,
		 sizeof(struct sof_smart_amp_config));

	return 0;
}

static int smart_amp_get_config(struct comp_dev *dev,
				struct sof_ipc_ctrl_data *cdata, int size)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	size_t bs;
	int ret = 0;

	/* Copy back to user space */
	bs = sad->config.size;

	comp_dbg(dev, "smart_amp_get_config(), actual blob size = %u, expected blob size = %u",
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
	struct smart_amp_data *smartpa_data = comp_get_drvdata(dev);
	int ret = 0;

	assert(smartpa_data);

	switch (cdata->data->type) {
	case SOF_SMART_AMP_CONFIG:
		ret = smart_amp_get_config(dev, cdata, size);
		break;
	case SOF_SMART_AMP_MODEL:
		comp_dbg(dev, "[Awinic] smart_amp_ctrl_get_data() type: %d", cdata->data->type);
		break;
	default:
		ret = smart_amp_get_param(smartpa_data->algo_handle, dev, cdata, size, cdata->data->type);
		if (ret < 0) {
			comp_err(dev, "smart_amp_get_param(): get failed");
		}
		break;
	}

	return ret;
}

static int smart_amp_ctrl_get_data(struct comp_dev *dev,
				   struct sof_ipc_ctrl_data *cdata, int size)
{
	int ret = 0;

	comp_dbg(dev, "[Awinic] smart_amp_ctrl_get_data() size: %d", size);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		ret = smart_amp_ctrl_get_bin_data(dev, cdata, size);
		break;
	default:
		comp_err(dev, "[Awinic] smart_amp_ctrl_get_data(): invalid cdata->cmd");
		return -EINVAL;
	}

	return ret;
}

static int smart_amp_ctrl_set_bin_data(struct comp_dev *dev,
				       struct sof_ipc_ctrl_data *cdata)
{
	struct smart_amp_data *smartpa_data = comp_get_drvdata(dev);
	int ret = 0;

	assert(smartpa_data);

	if (dev->state < COMP_STATE_READY) {
		comp_err(dev, "[Awinic] smart_amp_ctrl_set_bin_data(): driver in init!");
		return -EBUSY;
	}

	switch (cdata->data->type) {
	case SOF_SMART_AMP_CONFIG:
		ret = smart_amp_set_config(dev, cdata);
		break;
	case SOF_SMART_AMP_MODEL:
		comp_err(dev, "smart_amp_ctrl_set_bin_data(): parameter type %d!", cdata->data->type);
		break;
	default:
		ret = smart_amp_set_param(smartpa_data->algo_handle, dev, cdata, cdata->data->type);
		if (ret < 0) {
			comp_err(dev, "smart_amp_ctrl_set_bin_data(): set_param data failed!");
		}
		break;
	}

	return ret;
}

static int smart_amp_ctrl_set_data(struct comp_dev *dev,
				   struct sof_ipc_ctrl_data *cdata)
{
	int ret = 0;

	/* Check version from ABI header */
	if (SOF_ABI_VERSION_INCOMPATIBLE(SOF_ABI_VERSION, cdata->data->abi)) {
		comp_err(dev, "[Awinic] smart_amp_ctrl_set_data(): invalid version");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_dbg(dev, "[Awinic] smart_amp_ctrl_set_data(), SOF_CTRL_CMD_BINARY");
		ret = smart_amp_ctrl_set_bin_data(dev, cdata);
		break;
	default:
		comp_err(dev, "[Awinic] smart_amp_ctrl_set_data(): invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* used to pass standard and bespoke commands (with data) to component */
static int smart_amp_comp_cmd(struct comp_dev *dev, int cmd, void *data,
			 int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = ASSUME_ALIGNED(data, 4);

	comp_dbg(dev, "[Awinic] smart_amp_cmd(): cmd: %d", cmd);

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		return smart_amp_ctrl_set_data(dev, cdata);
	case COMP_CMD_GET_DATA:
		return smart_amp_ctrl_get_data(dev, cdata, max_data_size);
	default:
		return -EINVAL;
	}
}

static void smart_amp_comp_free(struct comp_dev *dev)
{
	struct smart_amp_data *smartpa_data = comp_get_drvdata(dev);

	comp_dbg(dev, "[Awinic] smart_amp_free()");

	smart_amp_deinit(smartpa_data->algo_handle, dev);

	smartpa_data->algo_handle = NULL;
	rfree(smartpa_data);
	rfree(dev);
}

static int smart_amp_comp_params(struct comp_dev *dev,
			    struct sof_ipc_stream_params *params)
{
	int err;

	comp_dbg(dev, "[Awinic] smart_amp_params()");

	err = comp_verify_params(dev, BUFF_PARAMS_CHANNELS, params); 
	if (err < 0) {
		comp_err(dev, "Awinic volume_verify_params() error: comp_verify_params() failed.");
		return -EINVAL;
	}

	return 0;
}

static int smart_amp_comp_trigger(struct comp_dev *dev, int cmd)
{
	struct smart_amp_data *smartpa_data = comp_get_drvdata(dev);
	int ret = 0;

	comp_dbg(dev, "[Awinic] smart_amp_trigger(), command = %u", cmd);

	ret = comp_set_state(dev, cmd);

	switch (cmd) {
	case COMP_TRIGGER_START:
	case COMP_TRIGGER_RELEASE:
		if (smartpa_data->feedback_buf) {
			struct comp_buffer __sparse_cache *buf = buffer_acquire(smartpa_data->feedback_buf);
			buffer_zero(buf);
			buffer_release(buf);
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



static int smart_amp_comp_copy(struct comp_dev *dev)
{
	struct smart_amp_data *smartpa_data = comp_get_drvdata(dev);
	struct comp_buffer __sparse_cache *source_buf = buffer_acquire(smartpa_data->source_buf);
	struct comp_buffer __sparse_cache *sink_buf = buffer_acquire(smartpa_data->sink_buf);
	uint32_t avail_passthrough_frames;
	uint32_t avail_feedback_frames;
	uint32_t avail_frames;
	uint32_t source_bytes;
	uint32_t sink_bytes;
	uint32_t feedback_bytes;

	comp_dbg(dev, "[Awinic] smart_amp_copy()");

	/* available bytes and samples calculation */
	avail_passthrough_frames = audio_stream_avail_frames(&source_buf->stream,
							     &sink_buf->stream);

	avail_frames = avail_passthrough_frames;

	if (smartpa_data->feedback_buf) {
		struct comp_buffer __sparse_cache *feedback_buf = buffer_acquire(smartpa_data->feedback_buf);

		if (comp_get_state(dev, feedback_buf->source) == dev->state) {
			/* feedback frame calculation */
			avail_feedback_frames =
				audio_stream_get_avail_frames(&feedback_buf->stream);

			/* RX and TX data frame alignment*/
			avail_feedback_frames = MIN(avail_passthrough_frames,
						    avail_feedback_frames);
			/* calculation TX data bytes */
			feedback_bytes = avail_feedback_frames *
				audio_stream_frame_bytes(&feedback_buf->stream);

			comp_dbg(dev, "[Awinic] smart_amp_copy(): processing %d feedback frames (avail_passthrough_frames: %d)",
				 avail_feedback_frames, avail_passthrough_frames);

			/* perform buffer writeback after source_buf process */
			buffer_stream_invalidate(feedback_buf, feedback_bytes); 

			/* copy IV data frome stream to IV buf */
			smart_amp_fb_data_prepare(smartpa_data->algo_handle, dev, &feedback_buf->stream,
				     avail_feedback_frames);

			comp_update_buffer_consume(feedback_buf, feedback_bytes);
		}

		buffer_release(feedback_buf);
	} else {
		/*IV buffer clear*/

	}

	/* bytes calculation */
	source_bytes = avail_frames * audio_stream_frame_bytes(&source_buf->stream);
	sink_bytes = avail_frames * audio_stream_frame_bytes(&sink_buf->stream);

	/* process data */
	buffer_stream_invalidate(source_buf, source_bytes);

    //ff data perpare
    smart_amp_ff_data_prepare(smartpa_data->algo_handle, dev, &source_buf->stream, avail_frames);
	
	/*algorithm process*/
	smart_amp_process(smartpa_data->algo_handle, dev,
				&source_buf->stream, &sink_buf->stream, avail_frames, smartpa_data->out_channels);

	buffer_stream_writeback(sink_buf, sink_bytes);

	/* source/sink buffer pointers update */
	comp_update_buffer_consume(source_buf, source_bytes);
	comp_update_buffer_produce(sink_buf, sink_bytes);

	buffer_release(sink_buf);
	buffer_release(source_buf);

	return 0;
}

static int smart_amp_comp_reset(struct comp_dev *dev)
{
	struct smart_amp_data *smartpa_data = comp_get_drvdata(dev);

	comp_dbg(dev, "[Awinic] smart_amp_reset()");

	smartpa_data->in_channels = 0;
	smartpa_data->out_channels = 0;

	comp_set_state(dev, COMP_TRIGGER_RESET);

	return 0;
}

static int smart_amp_comp_prepare(struct comp_dev *dev)
{
	struct smart_amp_data *smartpa_data = comp_get_drvdata(dev);
	struct comp_buffer __sparse_cache *source_c, *buf_c;
	struct list_item *blist;
	int ret;
	media_info_t media_info;

	comp_dbg(dev, "[Awinic] smart_amp_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	/* searching for stream and feedback source buffers */
	list_for_item(blist, &dev->bsource_list) {
		struct comp_buffer *source_buffer = container_of(blist, struct comp_buffer,
								 sink_list);
		source_c = buffer_acquire(source_buffer);

		if (source_c->source->ipc_config.type == SOF_COMP_DEMUX) {
			smartpa_data->feedback_buf = source_buffer;
		} else {
			smartpa_data->source_buf = source_buffer;
			smartpa_data->in_channels = audio_stream_get_channels(&source_c->stream);
		}

		buffer_release(source_c);
	}

	smartpa_data->sink_buf = list_first_item(&dev->bsink_list, struct comp_buffer,
					source_list);

	buf_c = buffer_acquire(smartpa_data->sink_buf); 
	smartpa_data->out_channels = audio_stream_get_channels(&buf_c->stream);
	buffer_release(buf_c);

	source_c = buffer_acquire(smartpa_data->source_buf);

	if (smartpa_data->feedback_buf) {
		buf_c = buffer_acquire(smartpa_data->feedback_buf);

		audio_stream_set_channels(&buf_c->stream, smartpa_data->config.feedback_channels);
		audio_stream_set_rate(&buf_c->stream, audio_stream_get_rate(&source_c->stream));
		buffer_release(buf_c);

		ret = smart_amp_check_audio_fmt(audio_stream_get_rate(&source_c->stream),
						audio_stream_get_channels(&source_c->stream));
		if (ret) {
			comp_err(dev, "[Awinic] Format not supported, sample rate: %d, ch: %d",
				 audio_stream_get_rate(&source_c->stream),
				 audio_stream_get_channels(&source_c->stream));
			goto error;
		}
	}

	switch (audio_stream_get_frm_fmt(&source_c->stream)) {
	case SOF_IPC_FRAME_S16_LE:
		media_info.bit_per_sample = 16;
		media_info.bit_qactor_sample = 16 -1;
		break;
	case SOF_IPC_FRAME_S24_4LE:
		media_info.bit_per_sample = 24;
		media_info.bit_qactor_sample = 24 -1;
		break;
	case SOF_IPC_FRAME_S32_LE:
		media_info.bit_per_sample = 32;
		media_info.bit_qactor_sample = 32 -1;
		break;
	default:
		comp_err(dev, "[Awinic] smart_amp_process() error: not supported frame format %d",
			 audio_stream_get_frm_fmt(&source_c->stream));
		goto error;
	}


	media_info.num_channel = audio_stream_get_channels(&source_c->stream);
	media_info.sample_rate =  audio_stream_get_rate(&source_c->stream);

	smartpa_data->algo_handle->media_info = media_info;
	comp_info(dev, "[Awinic] Re-initialized for %d bit processing", media_info.bit_per_sample);

	ret = smart_amp_init(smartpa_data->algo_handle, dev);
	if (ret) {
		comp_err(dev, "[Awinic] Re-initialization error.");
		goto error;
	}

error:
	buffer_release(source_c);
	smart_amp_flush(smartpa_data->algo_handle, dev);

	return ret;
}

static const struct comp_driver comp_smart_amp = {
	.type = SOF_COMP_SMART_AMP,
	.uid = SOF_RT_UUID(awinic_sktune_comp_uuid),
	.tctx = &awinic_sktune_comp_tr,
	.ops = {
		.create = smart_amp_comp_new,
		.free = smart_amp_comp_free,
		.params = smart_amp_comp_params,
		.prepare = smart_amp_comp_prepare,
		.cmd = smart_amp_comp_cmd,
		.trigger = smart_amp_comp_trigger,
		.copy = smart_amp_comp_copy,
		.reset = smart_amp_comp_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_smart_amp_info = {
	.drv = &comp_smart_amp,
};

UT_STATIC void sys_comp_smart_amp_init(void)
{
	comp_register(platform_shared_get(&comp_smart_amp_info,
					  sizeof(comp_smart_amp_info)));
}

DECLARE_MODULE(sys_comp_smart_amp_init);
SOF_MODULE_INIT(smart_amp, sys_comp_smart_amp_init);
