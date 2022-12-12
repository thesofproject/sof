// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Google LLC.
//
// Author: Kehuang Li <kehuangli@google.com>

#include <sof/audio/audio_stream.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/compiler_attributes.h>
#include <rtos/alloc.h>
#include <stdint.h>
#include <stdlib.h>

#include <google_audio_post_processing.h>

#define GOOGLE_AUDIO_POST_PROCESSING_SAMPLERATE 48000

/* fd48f000-c316-4ec2-9ff8-edb4efa3f52c */
DECLARE_SOF_RT_UUID("google-audio-post-processing", gapp_uuid, 0xfd48f000, 0xc316, 0x4ec2, 0x9f,
		    0xf8, 0xed, 0xb4, 0xef, 0xa3, 0xf5, 0x2c);
DECLARE_TR_CTX(gapp_tr, SOF_UUID(gapp_uuid), LOG_LEVEL_INFO);

/* GAPP configuration & model calibration data for tuning/debug */
#define SOF_GAPP_CONFIG 0
#define SOF_GAPP_MODEL 1
#define SOF_GAPP_DATA 2

struct comp_data {
	GoogleAudioPostProcessingState *state;
	struct comp_data_blob_handler *tuning_handler;
	uint32_t config[2];
	int channel_volume[SOF_IPC_MAX_CHANNELS];
	int num_channels;
	bool has_new_volume;
	struct GoogleAudioPostProcessingBuffer buf_in;
	struct GoogleAudioPostProcessingBuffer buf_out;
};

static int gapp_setup(struct comp_dev *dev, struct comp_data *cd)
{
	uint8_t *config;
	size_t config_size;

	config = comp_get_data_blob(cd->tuning_handler, &config_size, NULL);
	return GoogleAudioPostProcessingSetup(cd->state, cd->num_channels, dev->frames,
					      cd->channel_volume[0], config, config_size);
}

static int gapp_set_volume(struct comp_data *cd)
{
	int ret = 0;

	if (cd->has_new_volume) {
		ret = GoogleAudioPostProcessingSetVol(cd->state, cd->channel_volume,
						      cd->num_channels);
		if (ret >= 0)
			cd->has_new_volume = false;
	}

	return ret;
}

static int gapp_ctrl_set_val(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int j;
	int ch;
	uint32_t val;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_VOLUME:
		comp_dbg(dev, "gapp_ctrl_set_val(): SOF_CTRL_CMD_VOLUME, comp_id = %u",
			 cdata->comp_id);
		/* validate */
		if (cdata->num_elems > SOF_IPC_MAX_CHANNELS) {
			comp_err(dev, "gapp_ctrl_set_val(): invalid cdata->num_elems %u",
				 cdata->num_elems);
			return -EINVAL;
		}
		for (j = 0; j < cdata->num_elems; j++) {
			ch = cdata->chanv[j].channel;
			val = cdata->chanv[j].value;
			comp_dbg(dev, "gapp_ctrl_set_val(), channel = %d, value = %u", ch, val);
			if (cd->channel_volume[ch] != val) {
				cd->channel_volume[ch] = val;
				cd->has_new_volume = true;
			}
		}
		return 0;
	default:
		comp_err(dev, "gapp_ctrl_set_val(): Only volume control supported %d", cdata->cmd);
		return -EINVAL;
	}
}

static int gapp_ctrl_get_val(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata,
			     int max_data_size)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int j;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_VOLUME:
		comp_dbg(dev, "gapp_ctrl_get_val(): SOF_CTRL_CMD_VOLUME, comp_id = %u",
			 cdata->comp_id);
		/* validate */
		if (cdata->num_elems == 0 || cdata->num_elems > SOF_IPC_MAX_CHANNELS) {
			comp_err(dev, "gapp_ctrl_get_val(): invalid cdata->num_elems %u",
				 cdata->num_elems);
			return -EINVAL;
		}
		for (j = 0; j < cdata->num_elems; j++) {
			cdata->chanv[j].channel = j;
			cdata->chanv[j].value = cd->channel_volume[j];
		}
		return 0;
	default:
		comp_err(dev, "gapp_ctrl_get_val(): Only volume control supported %d", cdata->cmd);
		return -EINVAL;
	}
}

static int gapp_set_comp_config(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret;

	if (cdata->data->size != sizeof(cd->config)) {
		comp_err(dev, "gapp_set_comp_config(): invalid data size %d", cdata->data->size);
		return -EINVAL;
	}

	ret = memcpy_s(cd->config, sizeof(cd->config), cdata->data->data, cdata->data->size);
	comp_dbg(dev, "GAPP new settings c[0] %u c[1] %u", cd->config[0], cd->config[1]);

	return ret;
}

static int gapp_ctrl_set_bin_data(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	assert(cd);
	comp_dbg(dev,
		 "gapp_ctrl_set_bin_data(): type = %u, comp_id = %u, index = %u, blob_size = %d",
		 cdata->data->type, cdata->comp_id, cdata->msg_index, cdata->num_elems);

	if (dev->state < COMP_STATE_READY) {
		comp_err(dev, "gapp_ctrl_set_bin_data(): driver in init!");
		return -EBUSY;
	}

	switch (cdata->data->type) {
	case SOF_GAPP_CONFIG:
		return gapp_set_comp_config(dev, cdata);
	case SOF_GAPP_MODEL:
		return comp_data_blob_set_cmd(cd->tuning_handler, cdata);
	case SOF_GAPP_DATA:
		return 0;
	default:
		comp_err(dev, "gapp_ctrl_set_bin_data(): unknown binary data type");
		return -EINVAL;
	}
}

static int gapp_ctrl_set_data(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata)
{
	/* Check version from ABI header */
	if (SOF_ABI_VERSION_INCOMPATIBLE(SOF_ABI_VERSION, cdata->data->abi)) {
		comp_err(dev, "gapp_ctrl_set_data(): invalid version");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		return gapp_ctrl_set_bin_data(dev, cdata);
	default:
		comp_err(dev, "gapp_ctrl_set_data(): invalid cdata->cmd");
		return -EINVAL;
	}
}

static int gapp_get_comp_config(struct comp_data *cd, struct sof_ipc_ctrl_data *cdata,
				int max_data_size)
{
	int ret;

	if (sizeof(cd->config) > max_data_size)
		return -EINVAL;

	ret = memcpy_s(cdata->data->data, max_data_size, cd->config, sizeof(cd->config));
	if (ret)
		return ret;

	cdata->data->abi = SOF_ABI_VERSION;
	cdata->data->size = sizeof(cd->config);
	return 0;
}

static int gapp_get_internal_config(struct comp_data *cd, struct sof_ipc_ctrl_data *cdata,
				    int max_data_size)
{
	int data_size;
	int blob_size = cdata->num_elems;

	if (blob_size > max_data_size)
		return -EINVAL;

	data_size =
		GoogleAudioPostProcessingGetConfig(cd->state, cdata->data->type, cdata->msg_index,
						   (uint8_t *)cdata->data->data, blob_size);

	if (data_size < 0)
		return -EINVAL;

	cdata->data->abi = SOF_ABI_VERSION;
	cdata->data->size = data_size;

	return 0;
}

static int gapp_ctrl_get_bin_data(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata,
				  int max_data_size)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	if (!cd)
		return -ENODEV;

	comp_dbg(dev, "gapp_ctrl_get_bin_data(): type = %u, index = %u, size = %d",
		 cdata->data->type, cdata->msg_index, cdata->num_elems);

	switch (cdata->data->type) {
	case SOF_GAPP_CONFIG:
		return gapp_get_comp_config(cd, cdata, max_data_size);
	case SOF_GAPP_MODEL:
	case SOF_GAPP_DATA:
		return gapp_get_internal_config(cd, cdata, max_data_size);
	default:
		comp_err(dev, "gapp_ctrl_get_bin_data(): unknown binary data type");
		return -EINVAL;
	}
}

static int gapp_ctrl_get_data(struct comp_dev *dev, struct sof_ipc_ctrl_data *cdata,
			      int max_data_size)
{
	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		return gapp_ctrl_get_bin_data(dev, cdata, max_data_size);
	default:
		comp_err(dev, "smart_amp_ctrl_get_data(): invalid cdata->cmd");
		return -EINVAL;
	}
}

static int gapp_cmd(struct comp_dev *dev, int cmd, void *data, int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;

	switch (cmd) {
	case COMP_CMD_SET_VALUE:
		return gapp_ctrl_set_val(dev, cdata);
	case COMP_CMD_GET_VALUE:
		return gapp_ctrl_get_val(dev, cdata, max_data_size);
	case COMP_CMD_SET_DATA:
		return gapp_ctrl_set_data(dev, cdata);
	case COMP_CMD_GET_DATA:
		return gapp_ctrl_get_data(dev, cdata, max_data_size);
	default:
		comp_err(dev, "gapp_cmd(): unhandled command %d", cmd);
		return -EINVAL;
	}
}

static struct comp_dev *gapp_new(const struct comp_driver *drv, struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_process *gapp;
	struct sof_ipc_comp_process *ipc_gapp =
		container_of(comp, struct sof_ipc_comp_process, comp);
	struct comp_data *cd;
	int ret;

	comp_cl_info(drv, "gapp_new()");

	dev = comp_alloc(drv, COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd)
		goto fail;

	gapp = COMP_GET_IPC(dev, sof_ipc_comp_process);
	ret = memcpy_s(gapp, sizeof(*gapp), ipc_gapp, sizeof(struct sof_ipc_comp_process));
	if (ret)
		goto fail;

	if (ipc_gapp->size == sizeof(cd->config)) {
		ret = memcpy_s(cd->config, sizeof(cd->config), ipc_gapp->data, ipc_gapp->size);
		if (ret)
			goto fail;
	}

	cd->tuning_handler = comp_data_blob_handler_new(dev);
	if (!cd->tuning_handler)
		goto fail;

	cd->state = GoogleAudioPostProcessingCreate();
	if (!cd->state)
		goto fail_internal;

	comp_set_drvdata(dev, cd);

	dev->state = COMP_STATE_READY;

	comp_dbg(dev, "GAPP created c[0] %d c[1] %d", cd->config[0], cd->config[1]);

	return dev;
fail_internal:
	comp_data_blob_handler_free(cd->tuning_handler);
fail:
	rfree(cd);
	rfree(dev);
	return NULL;
}

static void gapp_delete(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	comp_dbg(dev, "gapp_delete()");

	GoogleAudioPostProcessingDelete(cd->state);
	comp_data_blob_handler_free(cd->tuning_handler);
	rfree(cd);
	rfree(dev);
}

static int gapp_params(struct comp_dev *dev, struct sof_ipc_stream_params *params)
{
	return comp_verify_params(dev, 0, params);
}

static int gapp_trigger(struct comp_dev *dev, int cmd)
{
	comp_dbg(dev, "gapp_trigger(): cmd = %d", cmd);
	return comp_set_state(dev, cmd);
}

static int gapp_prepare(struct comp_dev *dev)
{
	struct comp_buffer *sink_buf;
	struct sof_ipc_comp_config *config = dev_comp_config(dev);
	struct comp_data *cd = comp_get_drvdata(dev);
	uint32_t sink_per_bytes;
	int ret;

	comp_dbg(dev, "gapp_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	sink_buf = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);

	sink_per_bytes = audio_stream_period_bytes(&sink_buf->stream, dev->frames);

	if (sink_buf->stream.size < config->periods_sink * sink_per_bytes) {
		comp_err(dev, "gapp_prepare(): sink buffer size insufficient");
		return -ENOBUFS;
	}

	switch (sink_buf->stream.frame_fmt) {
	case SOF_IPC_FRAME_S32_LE:
	case SOF_IPC_FRAME_S16_LE:
		break;
	default:
		comp_err(dev, "unsupported data format: %d", sink_buf->stream.frame_fmt);
		return -EINVAL;
	}

	if (sink_buf->stream.rate != GOOGLE_AUDIO_POST_PROCESSING_SAMPLERATE) {
		comp_err(dev, "unsupported samplerate: %d", sink_buf->stream.rate);
		return -EINVAL;
	}

	cd->num_channels = sink_buf->stream.channels;

	ret = gapp_setup(dev, cd);
	if (ret < 0)
		return ret;

	ret = gapp_set_volume(cd);
	if (ret < 0)
		return ret;

	comp_dbg(dev, "GAPP prepared");
	return 0;
}

static int gapp_reset(struct comp_dev *dev)
{
	comp_dbg(dev, "gapp_reset()");
	return comp_set_state(dev, COMP_TRIGGER_RESET);
}

static int gapp_set_buffers(struct comp_data *cd, struct audio_stream *source,
			    struct audio_stream *sink, int frames)
{
	cd->buf_in.sample_size = audio_stream_sample_bytes(source);
	cd->buf_in.channels = source->channels;
	cd->buf_in.frames = frames;
	cd->buf_in.base_addr = source->addr;
	cd->buf_in.head_ptr = source->r_ptr;
	cd->buf_in.end_addr = source->end_addr;

	cd->buf_out.sample_size = audio_stream_sample_bytes(sink);
	cd->buf_out.channels = sink->channels;
	cd->buf_out.frames = frames;
	cd->buf_out.base_addr = sink->addr;
	cd->buf_out.head_ptr = sink->w_ptr;
	cd->buf_out.end_addr = sink->end_addr;
	return 0;
}

static int gapp_copy(struct comp_dev *dev)
{
	int ret;
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_copy_limits cl;
	struct comp_buffer *source;
	struct comp_buffer *sink;

	/* Check for changed configuration */
	if (comp_is_new_data_blob_available(cd->tuning_handler)) {
		ret = gapp_setup(dev, cd);
		if (ret < 0) {
			comp_err(dev, "gapp_copy(), failed reconfiguration");
			return ret;
		}
	}

	ret = gapp_set_volume(cd);
	if (ret < 0) {
		comp_err(dev, "gapp_copy(), failed setting volume");
		return ret;
	}

	source = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);

	comp_get_copy_limits_with_lock(source, sink, &cl);
	buffer_invalidate(source, cl.source_bytes);

	gapp_set_buffers(cd, &source->stream, &sink->stream, cl.frames);
	GoogleAudioPostProcessingProcess(cd->state, &cd->buf_in, &cd->buf_out);
	buffer_writeback(sink, cl.sink_bytes);

	comp_update_buffer_produce(sink, cl.sink_bytes);
	comp_update_buffer_consume(source, cl.source_bytes);

	return 0;
}

struct comp_driver comp_gapp = {
	.uid = SOF_RT_UUID(gapp_uuid),
	.tctx = &gapp_tr,
	.ops = {
		.create = gapp_new,
		.free = gapp_delete,
		.params = gapp_params,
		.cmd = gapp_cmd,
		.trigger = gapp_trigger,
		.prepare = gapp_prepare,
		.reset = gapp_reset,
		.copy = gapp_copy,
	},
};

static SHARED_DATA struct comp_driver_info comp_gapp_info = {
	.drv = &comp_gapp,
};

static void sys_comp_gapp_init(void)
{
	comp_register(platform_shared_get(&comp_gapp_info, sizeof(comp_gapp_info)));
}

DECLARE_MODULE(sys_comp_gapp_init);
