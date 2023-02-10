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

#if CONFIG_IPC_MAJOR_4
#include <ipc4/module.h>
#endif

static const struct comp_driver comp_smart_amp;

LOG_MODULE_REGISTER(smart_amp_test, CONFIG_SOF_LOG_LEVEL);

/* 167a961e-8ae4-11ea-89f1-000c29ce1635 */
DECLARE_SOF_RT_UUID("smart_amp-test", smart_amp_comp_uuid, 0x167a961e, 0x8ae4,
		    0x11ea, 0x89, 0xf1, 0x00, 0x0c, 0x29, 0xce, 0x16, 0x35);

DECLARE_TR_CTX(smart_amp_comp_tr, SOF_UUID(smart_amp_comp_uuid),
	       LOG_LEVEL_INFO);

struct smart_amp_data {
#if CONFIG_IPC_MAJOR_4
	struct sof_smart_amp_ipc4_config ipc4_cfg;
#endif
	struct sof_smart_amp_config config;
	struct comp_data_blob_handler *model_handler;
	void *data_blob;
	size_t data_blob_size;

	struct comp_buffer *source_buf; /**< stream source buffer */
	struct comp_buffer *feedback_buf; /**< feedback source buffer */
	struct comp_buffer *sink_buf; /**< sink buffer */

	struct k_mutex lock; /**< protect feedback_buf updated */

	smart_amp_proc process;

	uint32_t in_channels;
	uint32_t out_channels;
};

static struct comp_dev *smart_amp_new(const struct comp_driver *drv,
				      const struct comp_ipc_config *config,
				      const void *spec)
{
#if CONFIG_IPC_MAJOR_3
	const struct ipc_config_process *ipc_sa = spec;
	const struct sof_smart_amp_config *cfg;
#else
	const struct ipc4_base_module_extended_cfg *base_cfg = spec;
#endif
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

	k_mutex_init(&sad->lock);

#if CONFIG_IPC_MAJOR_4
	if (base_cfg->base_cfg_ext.nb_input_pins != SMART_AMP_NUM_IN_PINS ||
	    base_cfg->base_cfg_ext.nb_output_pins != SMART_AMP_NUM_OUT_PINS) {
		comp_err(dev, "smart_amp_new(): Invalid pin configuration");
		goto sad_fail;
	}

	/* Copy the base_cfg */
	memcpy_s(&sad->ipc4_cfg.base, sizeof(sad->ipc4_cfg.base),
		 &base_cfg->base_cfg, sizeof(base_cfg->base_cfg));

	/* Copy the pin formats */
	bs = sizeof(sad->ipc4_cfg.input_pins) + sizeof(sad->ipc4_cfg.output_pin);
	memcpy_s(sad->ipc4_cfg.input_pins, bs,
		 base_cfg->base_cfg_ext.pin_formats, bs);
#else
	cfg = (struct sof_smart_amp_config *)ipc_sa->data;
	bs = ipc_sa->size;

	if ((bs > 0) && (bs < sizeof(struct sof_smart_amp_config))) {
		comp_err(dev, "smart_amp_new(): failed to apply config");
		goto sad_fail;
	}

	memcpy_s(&sad->config, sizeof(struct sof_smart_amp_config), cfg, bs);
#endif

	dev->state = COMP_STATE_READY;

	return dev;

sad_fail:
	comp_data_blob_handler_free(sad->model_handler);
	rfree(sad);
fail:
	rfree(dev);
	return NULL;
}

#if CONFIG_IPC_MAJOR_4
static void smart_amp_set_params(struct comp_dev *dev,
				 struct sof_ipc_stream_params *params)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	struct comp_buffer *sink;
	struct comp_buffer __sparse_cache *sink_c;

	comp_dbg(dev, "smart_amp_set_params()");

	memset(params, 0, sizeof(*params));
	params->channels = sad->ipc4_cfg.base.audio_fmt.channels_count;
	params->rate = sad->ipc4_cfg.base.audio_fmt.sampling_frequency;
	params->sample_container_bytes = sad->ipc4_cfg.base.audio_fmt.depth / 8;
	params->sample_valid_bytes =
		sad->ipc4_cfg.base.audio_fmt.valid_bit_depth / 8;
	params->buffer_fmt = sad->ipc4_cfg.base.audio_fmt.interleaving_style;
	params->buffer.size = sad->ipc4_cfg.base.ibs;

	/* update sink format */
	if (!list_is_empty(&dev->bsink_list)) {
		struct ipc4_output_pin_format *sink_fmt = &sad->ipc4_cfg.output_pin;
		struct ipc4_audio_format out_fmt = sink_fmt->audio_fmt;

		sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
		sink_c = buffer_acquire(sink);
		sink_c->stream.channels = out_fmt.channels_count;
		sink_c->stream.rate = out_fmt.sampling_frequency;

		audio_stream_fmt_conversion(out_fmt.depth,
					    out_fmt.valid_bit_depth,
					    &sink_c->stream.frame_fmt,
					    &sink_c->stream.valid_sample_fmt,
					    out_fmt.s_type);

		sink_c->buffer_fmt = out_fmt.interleaving_style;
		params->frame_fmt = sink_c->stream.frame_fmt;

		sink_c->hw_params_configured = true;

		buffer_release(sink_c);
	}
}

static inline int smart_amp_set_config(struct comp_dev *dev, const char *data,
				       uint32_t data_size)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	struct sof_smart_amp_config *cfg;
	uint32_t cfg_size;

	cfg = (struct sof_smart_amp_config *)data;
	cfg_size = data_size;

	if (cfg_size != sizeof(struct sof_smart_amp_config)) {
		comp_err(dev, "smart_amp_set_config(): invalid config size %u, expect %u",
			 cfg_size, sizeof(struct sof_smart_amp_config));
		return -EINVAL;
	}

	comp_dbg(dev, "smart_amp_set_config(): config size = %u", cfg_size);

	memcpy_s(&sad->config, sizeof(struct sof_smart_amp_config), cfg,
		 sizeof(struct sof_smart_amp_config));

	return 0;
}

static inline int smart_amp_get_config(struct comp_dev *dev, char *data,
				       uint32_t *data_size)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	uint32_t cfg_size;

	cfg_size = sizeof(struct sof_smart_amp_config);

	if (cfg_size > *data_size) {
		comp_err(dev, "smart_amp_get_config(): wrong config size %d",
			 *data_size);
		return -EINVAL;
	}

	*data_size = cfg_size;

	return memcpy_s(data, cfg_size, &sad->config, cfg_size);
}

static int smart_amp_set_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
				      bool last_block, uint32_t data_offset, const char *data)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);

	comp_dbg(dev, "smart_amp_set_large_config()");

	switch (param_id) {
	case SMART_AMP_SET_MODEL:
		return ipc4_comp_data_blob_set(sad->model_handler,
					       first_block,
					       last_block,
					       data_offset,
					       data);
	case SMART_AMP_SET_CONFIG:
		return smart_amp_set_config(dev, data, data_offset);
	default:
		return -EINVAL;
	}
}

static int smart_amp_get_large_config(struct comp_dev *dev, uint32_t param_id, bool first_block,
				      bool last_block, uint32_t *data_offset, char *data)
{
	comp_dbg(dev, "smart_amp_get_large_config()");

	switch (param_id) {
	case SMART_AMP_GET_CONFIG:
		return smart_amp_get_config(dev, data, data_offset);
	default:
		return -EINVAL;
	}
}

static int smart_amp_get_attribute(struct comp_dev *dev, uint32_t type,
				   void *value)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);

	comp_dbg(dev, "smart_amp_get_attribute()");

	switch (type) {
	case COMP_ATTR_BASE_CONFIG:
		*(struct ipc4_base_module_cfg *)value = sad->ipc4_cfg.base;
		return 0;
	default:
		return -EINVAL;
	}
}

static int smart_amp_bind(struct comp_dev *dev, void *data)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	struct comp_buffer __sparse_cache *buffer_c;
	struct comp_buffer *source_buffer;
	struct list_item *blist;

	comp_dbg(dev, "smart_amp_bind()");

	/* searching for feedback source buffers */
	list_for_item(blist, &dev->bsource_list) {
		source_buffer = container_of(blist, struct comp_buffer, sink_list);

		k_mutex_lock(&sad->lock, K_FOREVER);
		buffer_c = buffer_acquire(source_buffer);
		if (IPC4_SINK_QUEUE_ID(buffer_c->id) == SOF_SMART_AMP_FEEDBACK_QUEUE_ID) {
			sad->feedback_buf = source_buffer;
			buffer_c->stream.channels = sad->config.feedback_channels;
			buffer_c->stream.rate = sad->ipc4_cfg.base.audio_fmt.sampling_frequency;

			buffer_release(buffer_c);
			k_mutex_unlock(&sad->lock);
			break;
		}

		buffer_release(buffer_c);
		k_mutex_unlock(&sad->lock);
	}

	return 0;
}

static int smart_amp_unbind(struct comp_dev *dev, void *data)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	struct ipc4_module_bind_unbind *bu = data;

	comp_dbg(dev, "smart_amp_unbind()");

	if (bu->extension.r.dst_instance_id == SOF_SMART_AMP_FEEDBACK_QUEUE_ID) {
		k_mutex_lock(&sad->lock, K_FOREVER);
		sad->feedback_buf = NULL;
		k_mutex_unlock(&sad->lock);
	}

	return 0;
}

#else

static void smart_amp_set_params(struct comp_dev *dev,
				 struct sof_ipc_stream_params *params)
{
	//nothing to do
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

	comp_dbg(dev, "smart_amp_set_config(), actual blob size = %u, expected blob size = %u",
		 bs, sizeof(struct sof_smart_amp_config));

	if (bs != sizeof(struct sof_smart_amp_config)) {
		comp_err(dev, "smart_amp_set_config(): invalid blob size, actual blob size = %u, expected blob size = %u",
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
	int ret = 0;

	// Copy back to user space
	bs = sad->config.size;

	comp_dbg(dev, "smart_amp_set_config(), actual blob size = %u, expected blob size = %u",
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
	int ret = 0;

	assert(sad);

	switch (cdata->data->type) {
	case SOF_SMART_AMP_CONFIG:
		ret = smart_amp_get_config(dev, cdata, size);
		break;
	case SOF_SMART_AMP_MODEL:
		ret = comp_data_blob_get_cmd(sad->model_handler, cdata, size);
		break;
	default:
		comp_err(dev, "smart_amp_ctrl_get_bin_data(): unknown binary data type");
		break;
	}

	return ret;
}

static int smart_amp_ctrl_get_data(struct comp_dev *dev,
				   struct sof_ipc_ctrl_data *cdata, int size)
{
	int ret = 0;

	comp_info(dev, "smart_amp_ctrl_get_data() size: %d", size);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		ret = smart_amp_ctrl_get_bin_data(dev, cdata, size);
		break;
	default:
		comp_err(dev, "smart_amp_ctrl_get_data(): invalid cdata->cmd");
		return -EINVAL;
	}

	return ret;
}

static int smart_amp_ctrl_set_bin_data(struct comp_dev *dev,
				       struct sof_ipc_ctrl_data *cdata)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	int ret = 0;

	assert(sad);

	if (dev->state < COMP_STATE_READY) {
		comp_err(dev, "smart_amp_ctrl_set_bin_data(): driver in init!");
		return -EBUSY;
	}

	switch (cdata->data->type) {
	case SOF_SMART_AMP_CONFIG:
		ret = smart_amp_set_config(dev, cdata);
		break;
	case SOF_SMART_AMP_MODEL:
		ret = comp_data_blob_set_cmd(sad->model_handler, cdata);
		break;
	default:
		comp_err(dev, "smart_amp_ctrl_set_bin_data(): unknown binary data type");
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
		comp_err(dev, "smart_amp_ctrl_set_data(): invalid version");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_ENUM:
		comp_info(dev, "smart_amp_ctrl_set_data(), SOF_CTRL_CMD_ENUM");
		break;
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "smart_amp_ctrl_set_data(), SOF_CTRL_CMD_BINARY");
		ret = smart_amp_ctrl_set_bin_data(dev, cdata);
		break;
	default:
		comp_err(dev, "smart_amp_ctrl_set_data(): invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}

	return ret;
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

#endif /* CONFIG_IPC_MAJOR_4 */

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
		k_mutex_lock(&sad->lock, K_FOREVER);
		if (sad->feedback_buf) {
			struct comp_buffer __sparse_cache *buf = buffer_acquire(sad->feedback_buf);
			buffer_zero(buf);
			buffer_release(buf);
		}
		k_mutex_unlock(&sad->lock);
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
				 const struct audio_stream __sparse_cache *source,
				 const struct audio_stream __sparse_cache *sink,
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
		in_frag += source->channels;
	}
	return 0;
}

static int smart_amp_process_s32(struct comp_dev *dev,
				 const struct audio_stream __sparse_cache *source,
				 const struct audio_stream __sparse_cache *sink,
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
		in_frag += source->channels;
	}

	return 0;
}

static smart_amp_proc get_smart_amp_process(struct comp_dev *dev,
					    struct comp_buffer __sparse_cache *buf)
{
	switch (buf->stream.frame_fmt) {
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
	struct comp_buffer __sparse_cache *source_buf = buffer_acquire(sad->source_buf);
	struct comp_buffer __sparse_cache *sink_buf = buffer_acquire(sad->sink_buf);
	uint32_t avail_passthrough_frames;
	uint32_t avail_feedback_frames;
	uint32_t avail_frames;
	uint32_t source_bytes;
	uint32_t sink_bytes;
	uint32_t feedback_bytes;

	comp_dbg(dev, "smart_amp_copy()");

	/* available bytes and samples calculation */
	avail_passthrough_frames =
		audio_stream_avail_frames(&source_buf->stream,
					  &sink_buf->stream);

	k_mutex_lock(&sad->lock, K_FOREVER);
	if (sad->feedback_buf) {
		struct comp_buffer __sparse_cache *buf = buffer_acquire(sad->feedback_buf);

		if (buf->source && comp_get_state(dev, buf->source) == dev->state) {
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

		buffer_release(buf);
	}
	k_mutex_unlock(&sad->lock);

	/* bytes calculation */
	source_bytes = avail_passthrough_frames *
		audio_stream_frame_bytes(&source_buf->stream);

	sink_bytes = avail_passthrough_frames *
		audio_stream_frame_bytes(&sink_buf->stream);

	/* process data */
	buffer_stream_invalidate(source_buf, source_bytes);
	sad->process(dev, &source_buf->stream, &sink_buf->stream,
		     avail_passthrough_frames, sad->config.source_ch_map);
	buffer_stream_writeback(sink_buf, sink_bytes);

	/* source/sink buffer pointers update */
	comp_update_buffer_consume(source_buf, source_bytes);
	comp_update_buffer_produce(sink_buf, sink_bytes);

	buffer_release(sink_buf);
	buffer_release(source_buf);

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
	struct comp_buffer __sparse_cache *buffer_c;
	struct list_item *blist;
	int ret;

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
		buffer_c = buffer_acquire(source_buffer);

#if CONFIG_IPC_MAJOR_3
		/* FIXME: how often can this loop be run? */
		if (buffer_c->source->ipc_config.type == SOF_COMP_DEMUX)
			sad->feedback_buf = source_buffer;
		else
#endif
			sad->source_buf = source_buffer;

		buffer_release(buffer_c);
	}

	sad->sink_buf = list_first_item(&dev->bsink_list, struct comp_buffer,
					source_list);

	buffer_c = buffer_acquire(sad->sink_buf);
	sad->out_channels = buffer_c->stream.channels;
	buffer_release(buffer_c);

	buffer_c = buffer_acquire(sad->source_buf);
	sad->in_channels = buffer_c->stream.channels;

	k_mutex_lock(&sad->lock, K_FOREVER);
	if (sad->feedback_buf) {
		struct comp_buffer __sparse_cache *buf = buffer_acquire(sad->feedback_buf);

		buf->stream.channels = sad->config.feedback_channels;
		buf->stream.rate = buffer_c->stream.rate;
		buffer_release(buf);
	}
	k_mutex_unlock(&sad->lock);

	sad->process = get_smart_amp_process(dev, buffer_c);
	if (!sad->process) {
		comp_err(dev, "smart_amp_prepare(): get_smart_amp_process failed");
		ret = -EINVAL;
	}

	buffer_release(buffer_c);

	return ret;
}

static const struct comp_driver comp_smart_amp = {
	.type = SOF_COMP_SMART_AMP,
	.uid = SOF_RT_UUID(smart_amp_comp_uuid),
	.tctx = &smart_amp_comp_tr,
	.ops = {
		.create			= smart_amp_new,
		.free			= smart_amp_free,
		.params			= smart_amp_params,
		.prepare		= smart_amp_prepare,
#if CONFIG_IPC_MAJOR_4
		.set_large_config	= smart_amp_set_large_config,
		.get_large_config	= smart_amp_get_large_config,
		.get_attribute		= smart_amp_get_attribute,
		.bind			= smart_amp_bind,
		.unbind			= smart_amp_unbind,
#else
		.cmd			= smart_amp_cmd,
#endif /* CONFIG_IPC_MAJOR_4 */
		.trigger		= smart_amp_trigger,
		.copy			= smart_amp_copy,
		.reset			= smart_amp_reset,
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
SOF_MODULE_INIT(smart_amp_test, sys_comp_smart_amp_init);
