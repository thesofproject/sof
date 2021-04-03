// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Maxim Integrated All rights reserved.
//
// Author: Ryan Lee <ryans.lee@maximintegrated.com>

#include <sof/trace/trace.h>
#include <sof/ipc/msg.h>
#include <sof/ut.h>
#include <user/smart_amp.h>
#include <sof/audio/smart_amp/smart_amp.h>

static const struct comp_driver comp_smart_amp;

/* 0cd84e80-ebd3-11ea-adc1-0242ac120002 */
DECLARE_SOF_RT_UUID("Maxim DSM", maxim_dsm_comp_uuid, 0x0cd84e80, 0xebd3,
		    0x11ea, 0xad, 0xc1, 0x02, 0x42, 0xac, 0x12, 0x00, 0x02);

DECLARE_TR_CTX(maxim_dsm_comp_tr, SOF_UUID(maxim_dsm_comp_uuid),
	       LOG_LEVEL_INFO);

/* Amp configuration & model calibration data for tuning/debug */
#define SOF_SMART_AMP_CONFIG 0
#define SOF_SMART_AMP_MODEL 1

typedef int(*smart_amp_proc)(struct comp_dev *dev,
			     const struct audio_stream *source,
			     const struct audio_stream *sink, uint32_t frames,
			     int8_t *chan_map, bool is_feedback);

struct smart_amp_data {
	struct sof_smart_amp_config config;
	struct comp_data_blob_handler *model_handler;
	struct comp_buffer *source_buf; /**< stream source buffer */
	struct comp_buffer *feedback_buf; /**< feedback source buffer */
	struct comp_buffer *sink_buf; /**< sink buffer */
	smart_amp_proc process;
	uint32_t in_channels;
	uint32_t out_channels;
	/* module handle for speaker protection algorithm */
	struct smart_amp_mod_struct_t *mod_handle;
};

static inline void smart_amp_free_memory(struct smart_amp_data *sad,
					 struct comp_dev *dev)
{
	struct smart_amp_mod_struct_t *hspk = sad->mod_handle;

	/* buffer : sof -> spk protection feed forward process */
	rfree(hspk->buf.frame_in);
	/* buffer : sof <- spk protection feed forward process */
	rfree(hspk->buf.frame_out);
	/* buffer : sof -> spk protection feedback process */
	rfree(hspk->buf.frame_iv);
	/* buffer : feed forward process input */
	rfree(hspk->buf.input);
	/* buffer : feed forward process output */
	rfree(hspk->buf.output);
	/* buffer : feedback voltage */
	rfree(hspk->buf.voltage);
	/* buffer : feedback current */
	rfree(hspk->buf.current);
	/* buffer : feed forward variable length -> fixed length */
	rfree(hspk->buf.ff.buf);
	/* buffer : feed forward variable length <- fixed length */
	rfree(hspk->buf.ff_out.buf);
	/* buffer : feedback variable length -> fixed length */
	rfree(hspk->buf.fb.buf);
	/* Module handle release */
	rfree(hspk);
}

static inline int smart_amp_alloc_memory(struct smart_amp_data *sad,
					 struct comp_dev *dev)
{
	struct smart_amp_mod_struct_t *hspk;
	int mem_sz;
	int size;

	/* memory allocation for module handle */
	mem_sz = sizeof(struct smart_amp_mod_struct_t);
	sad->mod_handle = rballoc(0, SOF_MEM_CAPS_RAM, mem_sz);
	if (!sad->mod_handle)
		goto err;
	memset(sad->mod_handle, 0, mem_sz);

	hspk = sad->mod_handle;

	/* buffer : sof -> spk protection feed forward process */
	size = SMART_AMP_FF_BUF_DB_SZ * sizeof(int32_t);
	hspk->buf.frame_in = rballoc(0, SOF_MEM_CAPS_RAM, size);
	if (!hspk->buf.frame_in)
		goto err;
	mem_sz += size;

	/* buffer : sof <- spk protection feed forward process */
	size = SMART_AMP_FF_BUF_DB_SZ * sizeof(int32_t);
	hspk->buf.frame_out = rballoc(0, SOF_MEM_CAPS_RAM, size);
	if (!hspk->buf.frame_out)
		goto err;
	mem_sz += size;

	/* buffer : sof -> spk protection feedback process */
	size = SMART_AMP_FB_BUF_DB_SZ * sizeof(int32_t);
	hspk->buf.frame_iv = rballoc(0, SOF_MEM_CAPS_RAM, size);
	if (!hspk->buf.frame_iv)
		goto err;
	mem_sz += size;

	/* buffer : feed forward process input */
	size = DSM_FF_BUF_SZ * sizeof(int32_t);
	hspk->buf.input = rballoc(0, SOF_MEM_CAPS_RAM, size);
	if (!hspk->buf.input)
		goto err;
	mem_sz += size;

	/* buffer : feed forward process output */
	size = DSM_FF_BUF_SZ * sizeof(int32_t);
	hspk->buf.output = rballoc(0, SOF_MEM_CAPS_RAM, size);
	if (!hspk->buf.output)
		goto err;
	mem_sz += size;

	/* buffer : feedback voltage */
	size = DSM_FF_BUF_SZ * sizeof(int32_t);
	hspk->buf.voltage = rballoc(0, SOF_MEM_CAPS_RAM, size);
	if (!hspk->buf.voltage)
		goto err;
	mem_sz += size;

	/* buffer : feedback current */
	size = DSM_FF_BUF_SZ * sizeof(int32_t);
	hspk->buf.current = rballoc(0, SOF_MEM_CAPS_RAM, size);
	if (!hspk->buf.current)
		goto err;
	mem_sz += size;

	/* buffer : feed forward variable length -> fixed length */
	size = DSM_FF_BUF_DB_SZ * sizeof(int32_t);
	hspk->buf.ff.buf = rballoc(0, SOF_MEM_CAPS_RAM, size);
	if (!hspk->buf.ff.buf)
		goto err;
	mem_sz += size;

	/* buffer : feed forward variable length <- fixed length */
	size = DSM_FF_BUF_DB_SZ * sizeof(int32_t);
	hspk->buf.ff_out.buf = rballoc(0, SOF_MEM_CAPS_RAM, size);
	if (!hspk->buf.ff_out.buf)
		goto err;
	mem_sz += size;

	/* buffer : feedback variable length -> fixed length */
	size = DSM_FB_BUF_DB_SZ * sizeof(int32_t);
	hspk->buf.fb.buf = rballoc(0, SOF_MEM_CAPS_RAM, size);
	if (!hspk->buf.fb.buf)
		goto err;
	mem_sz += size;

	/* memory allocation of DSM handle */
	size = smart_amp_get_memory_size(hspk, dev);
	hspk->dsmhandle = rballoc(0, SOF_MEM_CAPS_RAM, size);
	if (!hspk->dsmhandle)
		goto err;
	memset(hspk->dsmhandle, 0, size);
	mem_sz += size;

	comp_dbg(dev, "[DSM] module:%p (%d bytes used)",
		 hspk, mem_sz);

	return 0;
err:
	smart_amp_free_memory(sad, dev);
	return -ENOMEM;
}

static void smart_amp_free_caldata(struct comp_dev *dev,
				   struct smart_amp_caldata *caldata)
{
	if (!caldata->data)
		return;

	rfree(caldata->data);
	caldata->data = NULL;
	caldata->data_size = 0;
	caldata->data_pos = 0;
}

static inline int smart_amp_alloc_caldata(struct comp_dev *dev,
					  struct smart_amp_caldata *caldata,
					  uint32_t size)
{
	smart_amp_free_caldata(dev, caldata);

	if (!size)
		return 0;

	caldata->data = rballoc(0, SOF_MEM_CAPS_RAM, size);

	if (!caldata->data) {
		comp_err(dev, "smart_amp_alloc_caldata(): model->data rballoc failed");
		return -ENOMEM;
	}

	bzero(caldata->data, size);
	caldata->data_size = size;
	caldata->data_pos = 0;

	return 0;
}

static struct comp_dev *smart_amp_new(const struct comp_driver *drv,
				      struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_process *sa;
	struct sof_ipc_comp_process *ipc_sa =
		(struct sof_ipc_comp_process *)comp;
	struct smart_amp_data *sad;
	struct sof_smart_amp_config *cfg;
	size_t bs;
	int sz_caldata;
	int ret;

	dev = comp_alloc(drv, COMP_SIZE(struct sof_ipc_comp_process));
	if (!dev)
		return NULL;

	sad = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*sad));

	if (!sad) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, sad);

	sa = COMP_GET_IPC(dev, sof_ipc_comp_process);

	ret = memcpy_s(sa, sizeof(*sa), ipc_sa,
		       sizeof(struct sof_ipc_comp_process));
	assert(!ret);

	cfg = (struct sof_smart_amp_config *)ipc_sa->data;
	bs = ipc_sa->size;

	if (bs > 0 && bs < sizeof(struct sof_smart_amp_config)) {
		comp_err(dev, "smart_amp_new(): failed to apply config");

		if (sad)
			rfree(sad);
		rfree(sad);
		goto error;
	}

	memcpy_s(&sad->config, sizeof(struct sof_smart_amp_config), cfg, bs);

	if (smart_amp_alloc_memory(sad, dev) != 0)
		goto error;

	/* Bitwidth information is not available. Use 16bit as default.
	 * Re-initialize in the prepare function if ncessary
	 */
	sad->mod_handle->bitwidth = 16;
	if (smart_amp_init(sad->mod_handle, dev))
		goto error;

	/* Get the max. number of parameter to allocate memory for model data */
	sad->mod_handle->param.max_param =
		smart_amp_get_num_param(sad->mod_handle, dev);
	sz_caldata = sad->mod_handle->param.max_param * DSM_SINGLE_PARAM_SZ;

	if (sz_caldata > 0) {
		ret = smart_amp_alloc_caldata(dev, &sad->mod_handle->param.caldata,
					      sz_caldata * sizeof(int32_t));
		if (ret < 0) {
			comp_err(dev, "smart_amp_new(): caldata initial failed");
			goto error;
		}
	}

	/* update full parameter values */
	if (smart_amp_get_all_param(sad->mod_handle, dev) < 0)
		goto error;

	dev->state = COMP_STATE_READY;

	return dev;

error:
	return NULL;
}

static int smart_amp_set_config(struct comp_dev *dev,
				struct sof_ipc_ctrl_data *cdata)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	struct sof_smart_amp_config *cfg;
	size_t bs;

	/* Copy new config, find size from header */
	cfg = (struct sof_smart_amp_config *)
	       ASSUME_ALIGNED(cdata->data->data, sizeof(uint32_t));
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
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	int ret = 0;

	assert(sad);

	switch (cdata->data->type) {
	case SOF_SMART_AMP_CONFIG:
		ret = smart_amp_get_config(dev, cdata, size);
		break;
	case SOF_SMART_AMP_MODEL:
		ret = maxim_dsm_get_param(sad->mod_handle, dev, cdata, size);
		if (ret < 0) {
			comp_err(dev, "smart_amp_ctrl_get_bin_data(): parameter read error!");
			return ret;
		}
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

	comp_dbg(dev, "smart_amp_ctrl_get_data() size: %d", size);

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
		ret = maxim_dsm_set_param(sad->mod_handle, dev, cdata);
		if (ret < 0) {
			comp_err(dev, "smart_amp_ctrl_set_bin_data(): parameter write error!");
			return ret;
		}
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
	case SOF_CTRL_CMD_BINARY:
		comp_dbg(dev, "smart_amp_ctrl_set_data(), SOF_CTRL_CMD_BINARY");
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

	comp_dbg(dev, "smart_amp_cmd(): cmd: %d", cmd);

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

	comp_dbg(dev, "smart_amp_free()");

	smart_amp_free_caldata(dev, &sad->mod_handle->param.caldata);
	smart_amp_free_memory(sad, dev);

	rfree(sad);
	rfree(dev);
}

static int smart_amp_verify_params(struct comp_dev *dev,
				   struct sof_ipc_stream_params *params)
{
	int ret;

	comp_dbg(dev, "smart_amp_verify_params()");

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

	comp_dbg(dev, "smart_amp_params()");

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

	comp_dbg(dev, "smart_amp_trigger(), command = %u", cmd);

	ret = comp_set_state(dev, cmd);

	switch (cmd) {
	case COMP_TRIGGER_START:
	case COMP_TRIGGER_RELEASE:
		if (sad->feedback_buf)
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

static int smart_amp_process(struct comp_dev *dev,
			     const struct audio_stream *source,
			     const struct audio_stream *sink,
			     uint32_t frames, int8_t *chan_map,
			     bool is_feedback)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	int ret;

	if (!is_feedback)
		ret = smart_amp_ff_copy(dev, frames,
					sad->source_buf, sad->sink_buf,
					chan_map, sad->mod_handle,
					sad->in_channels, sad->out_channels);
	else
		ret = smart_amp_fb_copy(dev, frames,
					sad->feedback_buf, sad->sink_buf,
					chan_map, sad->mod_handle,
					sad->feedback_buf->stream.channels);
	return ret;
}

static smart_amp_proc get_smart_amp_process(struct comp_dev *dev)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);

	switch (sad->source_buf->stream.frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		return smart_amp_process;
	default:
		comp_err(dev, "smart_amp_process() error: not supported frame format");
		return NULL;
	}
}

static int smart_amp_copy(struct comp_dev *dev)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	uint32_t avail_passthrough_frames;
	uint32_t avail_feedback_frames;
	uint32_t avail_frames;
	uint32_t source_bytes;
	uint32_t sink_bytes;
	uint32_t feedback_bytes;
	uint32_t source_flags = 0;
	uint32_t sink_flags = 0;
	uint32_t feedback_flags = 0;
	int ret = 0;

	comp_dbg(dev, "smart_amp_copy()");

	buffer_lock(sad->source_buf, &source_flags);
	buffer_lock(sad->sink_buf, &sink_flags);

	/* available bytes and samples calculation */
	avail_passthrough_frames =
		audio_stream_avail_frames(&sad->source_buf->stream,
					  &sad->sink_buf->stream);

	buffer_unlock(sad->source_buf, source_flags);
	buffer_unlock(sad->sink_buf, sink_flags);

	avail_frames = avail_passthrough_frames;

	if (sad->feedback_buf) {
		buffer_lock(sad->feedback_buf, &feedback_flags);
		if (comp_get_state(dev, sad->feedback_buf->source) == dev->state) {
			/* feedback */
			avail_feedback_frames =
				audio_stream_get_avail_frames(&sad->feedback_buf->stream);

			avail_frames = MIN(avail_passthrough_frames,
					   avail_feedback_frames);

			feedback_bytes = avail_frames *
				audio_stream_frame_bytes(&sad->feedback_buf->stream);

			buffer_unlock(sad->feedback_buf, feedback_flags);

			comp_dbg(dev, "smart_amp_copy(): processing %d feedback frames (avail_passthrough_frames: %d)",
				 avail_frames, avail_passthrough_frames);

			/* perform buffer writeback after source_buf process */
			buffer_invalidate(sad->feedback_buf, feedback_bytes);
			sad->process(dev, &sad->feedback_buf->stream,
				     &sad->sink_buf->stream, avail_frames,
				     sad->config.feedback_ch_map, true);

			comp_update_buffer_consume(sad->feedback_buf, feedback_bytes);
		} else {
			buffer_unlock(sad->feedback_buf, feedback_flags);
		}
	}

	/* bytes calculation */
	buffer_lock(sad->source_buf, &source_flags);
	source_bytes = avail_frames *
		audio_stream_frame_bytes(&sad->source_buf->stream);
	buffer_unlock(sad->source_buf, source_flags);

	buffer_lock(sad->sink_buf, &sink_flags);
	sink_bytes = avail_frames *
		audio_stream_frame_bytes(&sad->sink_buf->stream);
	buffer_unlock(sad->sink_buf, sink_flags);

	/* process data */
	buffer_invalidate(sad->source_buf, source_bytes);
	sad->process(dev, &sad->source_buf->stream, &sad->sink_buf->stream,
		     avail_frames, sad->config.source_ch_map, false);
	buffer_writeback(sad->sink_buf, sink_bytes);

	/* source/sink buffer pointers update */
	comp_update_buffer_consume(sad->source_buf, source_bytes);
	comp_update_buffer_produce(sad->sink_buf, sink_bytes);

	return ret;
}

static int smart_amp_reset(struct comp_dev *dev)
{
	comp_dbg(dev, "smart_amp_reset()");

	comp_set_state(dev, COMP_TRIGGER_RESET);

	return 0;
}

static int smart_amp_prepare(struct comp_dev *dev)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	struct comp_buffer *source_buffer;
	struct list_item *blist;
	uint32_t flags = 0;
	int ret;
	int bitwidth;

	comp_dbg(dev, "smart_amp_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	/* searching for stream and feedback source buffers */
	list_for_item(blist, &dev->bsource_list) {
		source_buffer = container_of(blist, struct comp_buffer,
					     sink_list);
		buffer_lock(source_buffer, &flags);
		if (source_buffer->source->comp.type == SOF_COMP_DEMUX)
			sad->feedback_buf = source_buffer;
		else
			sad->source_buf = source_buffer;
		buffer_unlock(source_buffer, flags);
	}

	sad->sink_buf = list_first_item(&dev->bsink_list, struct comp_buffer,
					source_list);

	sad->in_channels = sad->source_buf->stream.channels;
	sad->out_channels = sad->sink_buf->stream.channels;

	if (sad->feedback_buf) {
		buffer_lock(sad->feedback_buf, &flags);
		sad->feedback_buf->stream.channels = sad->config.feedback_channels;
		sad->feedback_buf->stream.rate = sad->source_buf->stream.rate;

		ret = smart_amp_check_audio_fmt(sad->source_buf->stream.rate,
						sad->source_buf->stream.channels);
		if (ret) {
			comp_err(dev, "[DSM] Format not supported, sample rate: %d, ch: %d",
				 sad->source_buf->stream.rate,
				 sad->source_buf->stream.channels);
			goto error;
		}
		buffer_unlock(sad->feedback_buf, flags);
	}

	switch (sad->source_buf->stream.frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		bitwidth = 16;
		break;
	case SOF_IPC_FRAME_S24_4LE:
		bitwidth = 24;
		break;
	case SOF_IPC_FRAME_S32_LE:
		bitwidth = 32;
		break;
	default:
		comp_err(dev, "[DSM] smart_amp_process() error: not supported frame format %d",
			 sad->source_buf->stream.frame_fmt);
		goto error;
	}
	if (sad->mod_handle->bitwidth != bitwidth)	{
		sad->mod_handle->bitwidth = bitwidth;
		comp_info(dev, "[DSM] Re-initialized for %d bit processing", bitwidth);

		ret = smart_amp_init(sad->mod_handle, dev);
		if (ret) {
			comp_err(dev, "[DSM] Re-initialization error.");
			goto error;
		}
		ret = maxim_dsm_restore_param(sad->mod_handle, dev);
		if (ret) {
			comp_err(dev, "[DSM] Restoration error.");
			goto error;
		}
	}

	sad->process = get_smart_amp_process(dev);
	if (!sad->process) {
		comp_err(dev, "smart_amp_prepare(): get_smart_amp_process failed");
		ret = -EINVAL;
		goto error;
	}

error:
	smart_amp_flush(sad->mod_handle, dev);
	return ret;
}

static const struct comp_driver comp_smart_amp = {
	.type = SOF_COMP_SMART_AMP,
	.uid = SOF_RT_UUID(maxim_dsm_comp_uuid),
	.tctx = &maxim_dsm_comp_tr,
	.ops = {
		.create = smart_amp_new,
		.free = smart_amp_free,
		.params = smart_amp_params,
		.prepare = smart_amp_prepare,
		.cmd = smart_amp_cmd,
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
