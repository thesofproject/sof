// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Maxim Integrated All rights reserved.
//
// Author: Ryan Lee <ryans.lee@maximintegrated.com>
//
// Copyright(c) 2023 Google LLC.
//
// Author: Pin-chih Lin <johnylin@google.com>

#include <sys/types.h>

#include <rtos/init.h>
#include <sof/trace/trace.h>
#include <sof/ipc/msg.h>
#include <sof/ut.h>
#include <user/smart_amp.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/smart_amp/smart_amp.h>

static const struct comp_driver comp_smart_amp;

/* NOTE: this code builds itself with one of two distinct UUIDs
 * depending on configuration!
 */
#if CONFIG_MAXIM_DSM
SOF_DEFINE_REG_UUID(maxim_dsm);
#define UUID_SYM maxim_dsm_uuid

#else /* Passthrough */
SOF_DEFINE_REG_UUID(passthru_smart_amp);
#define UUID_SYM passthru_smart_amp

#endif
DECLARE_TR_CTX(smart_amp_comp_tr, SOF_UUID(UUID_SYM),
	       LOG_LEVEL_INFO);

/* Amp configuration & model calibration data for tuning/debug */
#define SOF_SMART_AMP_CONFIG 0
#define SOF_SMART_AMP_MODEL 1

struct smart_amp_data {
	struct sof_smart_amp_config config;
	struct comp_data_blob_handler *model_handler;
	struct comp_buffer *source_buf; /**< stream source buffer */
	struct comp_buffer *feedback_buf; /**< feedback source buffer */
	struct comp_buffer *sink_buf; /**< sink buffer */

	struct ipc_config_process ipc_config;

	smart_amp_src_func ff_get_frame; /**< function to get stream source */
	smart_amp_src_func fb_get_frame; /**< function to get feedback source */
	smart_amp_sink_func ff_set_frame; /**< function to set sink */
	struct smart_amp_mod_stream ff_mod; /**< feed-forward buffer for mod */
	struct smart_amp_mod_stream fb_mod; /**< feedback buffer for mod */
	struct smart_amp_mod_stream out_mod; /**< output buffer for mod */

	struct smart_amp_buf mod_mems[MOD_MEMBLK_MAX]; /**< memory blocks for mod */

	struct smart_amp_mod_data_base *mod_data; /**< inner model data */
};

/* smart_amp_free_mod_memories will be called promptly once getting error on
 * internal functions. That is, it may be called multiple times by the hierarchical
 * model when an error occurs in the lower function and propagates level-wise to
 * the function on top. Always set the pointer to NULL after freed to avoid the
 * double-freeing error.
 */
static inline void smart_amp_free_mod_memories(struct smart_amp_data *sad)
{
	/* sof -> mod feed-forward data re-mapping and format conversion */
	rfree(sad->ff_mod.buf.data);
	sad->ff_mod.buf.data = NULL;
	/* sof -> mod feedback data re-mapping and format conversion */
	rfree(sad->fb_mod.buf.data);
	sad->fb_mod.buf.data = NULL;
	/* mod -> sof processed data format conversion */
	rfree(sad->out_mod.buf.data);
	sad->out_mod.buf.data = NULL;

	/* mem block for mod private data usage */
	rfree(sad->mod_mems[MOD_MEMBLK_PRIVATE].data);
	sad->mod_mems[MOD_MEMBLK_PRIVATE].data = NULL;
	/* mem block for mod audio frame data usage */
	rfree(sad->mod_mems[MOD_MEMBLK_FRAME].data);
	sad->mod_mems[MOD_MEMBLK_FRAME].data = NULL;
	/* mem block for mod parameter blob usage */
	rfree(sad->mod_mems[MOD_MEMBLK_PARAM].data);
	sad->mod_mems[MOD_MEMBLK_PARAM].data = NULL;

	/* inner model data struct */
	rfree(sad->mod_data);
	sad->mod_data = NULL;
}

static inline int smart_amp_buf_alloc(struct smart_amp_buf *buf, size_t size)
{
	buf->data = rballoc(0, SOF_MEM_CAPS_RAM, size);
	if (!buf->data)
		return -ENOMEM;
	buf->size = size;
	return 0;
}

static ssize_t smart_amp_alloc_mod_memblk(struct smart_amp_data *sad,
					  enum smart_amp_mod_memblk blk)
{
	struct smart_amp_mod_data_base *mod = sad->mod_data;
	int ret;
	size_t size;

	/* query the required size from inner model. */
	ret = mod->mod_ops->query_memblk_size(mod, blk);
	if (ret < 0)
		goto error;

	/* no memory required */
	if (ret == 0)
		return 0;

	/* allocate the memory block when returned size > 0. */
	size = ret;
	ret = smart_amp_buf_alloc(&sad->mod_mems[blk], size);
	if (ret < 0)
		goto error;

	/* provide the memory block information to inner model. */
	ret = mod->mod_ops->set_memblk(mod, blk, &sad->mod_mems[blk]);
	if (ret < 0)
		goto error;

	return size;

error:
	smart_amp_free_mod_memories(sad);
	return ret;
}

static int smart_amp_alloc_data_buffers(struct comp_dev *dev,
					struct smart_amp_data *sad)
{
	size_t total_size;
	int ret;
	size_t size;

	/* sof -> mod feed-forward data re-mapping and format conversion */
	size = SMART_AMP_FF_BUF_DB_SZ * sizeof(int32_t);
	ret = smart_amp_buf_alloc(&sad->ff_mod.buf, size);
	if (ret < 0)
		goto error;
	total_size = size;

	/* sof -> mod feedback data re-mapping and format conversion */
	size = SMART_AMP_FB_BUF_DB_SZ * sizeof(int32_t);
	ret = smart_amp_buf_alloc(&sad->fb_mod.buf, size);
	if (ret < 0)
		goto error;
	total_size += size;

	/* mod -> sof processed data format conversion */
	size = SMART_AMP_FF_BUF_DB_SZ * sizeof(int32_t);
	ret = smart_amp_buf_alloc(&sad->out_mod.buf, size);
	if (ret < 0)
		goto error;
	total_size += size;

	comp_dbg(dev, "smart_amp_alloc(): used data buffer %zu bytes", total_size);
	return 0;

error:
	smart_amp_free_mod_memories(sad);
	return ret;
}

static struct comp_dev *smart_amp_new(const struct comp_driver *drv,
				      const struct comp_ipc_config *config,
				      const void *spec)
{
	struct comp_dev *dev;
	const struct ipc_config_process *ipc_sa = spec;
	struct smart_amp_data *sad;
	struct sof_smart_amp_config *cfg;
	size_t bs;
	int ret;

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	dev->ipc_config = *config;

	sad = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*sad));
	if (!sad) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, sad);
	sad->ipc_config = *ipc_sa;

	cfg = (struct sof_smart_amp_config *)ipc_sa->data;
	bs = ipc_sa->size;

	if (bs > 0 && bs < sizeof(struct sof_smart_amp_config)) {
		comp_err(dev, "smart_amp_new(): failed to apply config");
		goto error;
	}

	memcpy_s(&sad->config, sizeof(struct sof_smart_amp_config), cfg, bs);

	/* allocate inner model data struct */
	sad->mod_data = mod_data_create(dev);
	if (!sad->mod_data) {
		comp_err(dev, "smart_amp_new(): failed to allocate nner model data");
		goto error;
	}

	/* allocate stream buffers for mod */
	ret = smart_amp_alloc_data_buffers(dev, sad);
	if (ret) {
		comp_err(dev, "smart_amp_new(): failed to allocate data buffers, ret:%d", ret);
		goto error;
	}

	/* (before init) allocate mem block for mod private data usage */
	ret = smart_amp_alloc_mod_memblk(sad, MOD_MEMBLK_PRIVATE);
	if (ret < 0) {
		comp_err(dev, "smart_amp_new(): failed to allocate mod private, ret:%d", ret);
		goto error;
	}
	comp_dbg(dev, "smart_amp_new(): used mod private buffer %d bytes", ret);

	/* init model */
	ret = sad->mod_data->mod_ops->init(sad->mod_data);
	if (ret) {
		comp_err(dev, "smart_amp_new(): failed to init inner model, ret:%d", ret);
		goto error;
	}

	/* (after init) allocate mem block for mod audio frame data usage */
	ret = smart_amp_alloc_mod_memblk(sad, MOD_MEMBLK_FRAME);
	if (ret < 0) {
		comp_err(dev, "smart_amp_new(): failed to allocate mod buffer, ret:%d", ret);
		goto error;
	}
	comp_dbg(dev, "smart_amp_new(): used mod data buffer %d bytes", ret);

	/* (after init) allocate mem block for mod parameter blob usage */
	ret = smart_amp_alloc_mod_memblk(sad, MOD_MEMBLK_PARAM);
	if (ret < 0) {
		comp_err(dev, "smart_amp_new(): failed to allocate mod config, ret:%d", ret);
		goto error;
	}
	comp_dbg(dev, "smart_amp_new(): used mod config buffer %d bytes", ret);

	dev->state = COMP_STATE_READY;

	return dev;

error:
	smart_amp_free_mod_memories(sad);
	rfree(sad);
	rfree(dev);
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
	int ret = 0;

	/* Copy back to user space */
	bs = sad->config.size;

	comp_dbg(dev, "smart_amp_get_config(), actual blob size = %zu, expected blob size = %zu",
		 bs, sizeof(struct sof_smart_amp_config));

	if (bs == 0 || bs > size)
		return -EINVAL;

	ret = memcpy_s(cdata->data->data, size, &sad->config, bs);
	assert(!ret);

	cdata->data->abi = SOF_ABI_VERSION;
	cdata->data->size = bs;

	return ret;
}

#if CONFIG_IPC_MAJOR_3
static int smart_amp_ctrl_get_bin_data(struct comp_dev *dev,
				       struct sof_ipc_ctrl_data *cdata,
				       int size)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	struct smart_amp_mod_data_base *mod;
	int ret = 0;

	assert(sad);
	mod = sad->mod_data;

	switch (cdata->data->type) {
	case SOF_SMART_AMP_CONFIG:
		ret = smart_amp_get_config(dev, cdata, size);
		break;
	case SOF_SMART_AMP_MODEL:
		ret = mod->mod_ops->get_config(mod, cdata, size);
		if (ret < 0) {
			comp_err(dev, "smart_amp_ctrl_get_bin_data(): failed to read inner model!");
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
	struct smart_amp_mod_data_base *mod;
	int ret = 0;

	assert(sad);
	mod = sad->mod_data;

	if (dev->state < COMP_STATE_READY) {
		comp_err(dev, "smart_amp_ctrl_set_bin_data(): driver in init!");
		return -EBUSY;
	}

	switch (cdata->data->type) {
	case SOF_SMART_AMP_CONFIG:
		ret = smart_amp_set_config(dev, cdata);
		break;
	case SOF_SMART_AMP_MODEL:
		ret = mod->mod_ops->set_config(mod, cdata);
		if (ret < 0) {
			comp_err(dev, "smart_amp_ctrl_set_bin_data(): failed to write inner model!");
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
#endif

static void smart_amp_free(struct comp_dev *dev)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);

	comp_dbg(dev, "smart_amp_free()");

	smart_amp_free_mod_memories(sad);

	rfree(sad);
	sad = NULL;
	rfree(dev);
	dev = NULL;
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

static int smart_amp_ff_process(struct comp_dev *dev,
				const struct audio_stream *source,
				const struct audio_stream *sink,
				uint32_t frames, const int8_t *chan_map)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	struct smart_amp_mod_data_base *mod = sad->mod_data;
	int ret;

	sad->ff_mod.consumed = 0;
	sad->out_mod.produced = 0;

	if (frames == 0) {
		comp_warn(dev, "smart_amp_copy(): feed forward frame size zero warning.");
		return 0;
	}

	if (frames > SMART_AMP_FF_BUF_DB_SZ) {
		comp_err(dev, "smart_amp_copy(): feed forward frame size overflow: %u", frames);
		sad->ff_mod.consumed = frames;
		return -EINVAL;
	}

	sad->ff_get_frame(&sad->ff_mod, frames, source, chan_map);

	ret = mod->mod_ops->ff_proc(mod, frames, &sad->ff_mod, &sad->out_mod);
	if (ret) {
		comp_err(dev, "smart_amp_copy(): feed forward inner model process error");
		return ret;
	}

	sad->ff_set_frame(&sad->out_mod, sad->out_mod.produced, sink);

	return 0;
}

static int smart_amp_fb_process(struct comp_dev *dev,
				const struct audio_stream *source,
				uint32_t frames, const int8_t *chan_map)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	struct smart_amp_mod_data_base *mod = sad->mod_data;
	int ret;

	sad->fb_mod.consumed = 0;

	if (frames == 0) {
		comp_warn(dev, "smart_amp_copy(): feedback frame size zero warning.");
		return 0;
	}

	if (frames > SMART_AMP_FB_BUF_DB_SZ) {
		comp_err(dev, "smart_amp_copy(): feedback frame size overflow: %u", frames);
		sad->fb_mod.consumed = frames;
		return -EINVAL;
	}

	sad->fb_get_frame(&sad->fb_mod, frames, source, chan_map);

	ret = mod->mod_ops->fb_proc(mod, frames, &sad->fb_mod);
	if (ret) {
		comp_err(dev, "smart_amp_copy(): feedback inner model process error");
		return ret;
	}

	return 0;
}

static int smart_amp_copy(struct comp_dev *dev)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	struct comp_buffer *source_buf = sad->source_buf;
	struct comp_buffer *sink_buf = sad->sink_buf;
	uint32_t avail_passthrough_frames;
	uint32_t avail_feedback_frames;
	uint32_t avail_frames;
	uint32_t source_bytes;
	uint32_t sink_bytes;
	uint32_t feedback_bytes;

	comp_dbg(dev, "smart_amp_copy()");

	/* available bytes and samples calculation */
	avail_passthrough_frames = audio_stream_avail_frames(&source_buf->stream,
							     &sink_buf->stream);

	avail_frames = avail_passthrough_frames;

	if (sad->feedback_buf) {
		struct comp_buffer *feedback_buf = sad->feedback_buf;

		if (comp_get_state(feedback_buf->source) == dev->state) {
			/* feedback */
			avail_feedback_frames =
				audio_stream_get_avail_frames(&feedback_buf->stream);

			avail_feedback_frames = MIN(avail_passthrough_frames,
						    avail_feedback_frames);

			feedback_bytes = avail_feedback_frames *
				audio_stream_frame_bytes(&feedback_buf->stream);

			comp_dbg(dev, "smart_amp_copy(): processing %u feedback frames (avail_passthrough_frames: %u)",
				 avail_feedback_frames, avail_passthrough_frames);

			/* perform buffer writeback after source_buf process */
			buffer_stream_invalidate(feedback_buf, feedback_bytes);
			smart_amp_fb_process(dev, &feedback_buf->stream,
					     avail_feedback_frames,
					     sad->config.feedback_ch_map);

			comp_dbg(dev, "smart_amp_copy(): consumed %u feedback frames",
				 sad->fb_mod.consumed);
			if (sad->fb_mod.consumed < avail_feedback_frames) {
				feedback_bytes = sad->fb_mod.consumed *
					audio_stream_frame_bytes(&feedback_buf->stream);
			}

			comp_update_buffer_consume(feedback_buf, feedback_bytes);
		}
	}

	/* process data */
	source_bytes = avail_frames * audio_stream_frame_bytes(&source_buf->stream);

	buffer_stream_invalidate(source_buf, source_bytes);
	smart_amp_ff_process(dev, &source_buf->stream, &sink_buf->stream,
			     avail_frames, sad->config.source_ch_map);

	comp_dbg(dev, "smart_amp_copy(): processing %u feed forward frames (consumed: %u, produced: %u)",
		 avail_frames, sad->ff_mod.consumed, sad->out_mod.produced);
	if (sad->ff_mod.consumed < avail_frames)
		source_bytes = sad->ff_mod.consumed * audio_stream_frame_bytes(&source_buf->stream);

	sink_bytes = sad->out_mod.produced * audio_stream_frame_bytes(&sink_buf->stream);
	buffer_stream_writeback(sink_buf, sink_bytes);

	/* source/sink buffer pointers update */
	comp_update_buffer_consume(source_buf, source_bytes);
	comp_update_buffer_produce(sink_buf, sink_bytes);

	return 0;
}

static int smart_amp_reset(struct comp_dev *dev)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	struct smart_amp_mod_data_base *mod = sad->mod_data;
	int ret;

	comp_dbg(dev, "smart_amp_reset()");

	sad->ff_get_frame = NULL;
	sad->fb_get_frame = NULL;
	sad->ff_set_frame = NULL;

	/* reset inner model */
	ret = mod->mod_ops->reset(mod);
	if (ret)
		return ret;

	comp_set_state(dev, COMP_TRIGGER_RESET);

	return 0;
}

/* supported formats: {SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE}
 * which are enumerated as 0, 1, and 2. Simply check if supported while fmt <= 2.
 */
static inline bool is_supported_fmt(uint16_t fmt)
{
	return fmt <= SOF_IPC_FRAME_S32_LE;
}

static int smart_amp_resolve_mod_fmt(struct comp_dev *dev, uint32_t least_req_depth)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	struct smart_amp_mod_data_base *mod = sad->mod_data;
	const uint16_t *mod_fmts;
	int num_mod_fmts;
	int ret;
	int i;

	/* get supported formats from mod */
	ret = mod->mod_ops->get_supported_fmts(mod, &mod_fmts, &num_mod_fmts);
	if (ret) {
		comp_err(dev, "smart_amp_resolve_mod_fmt(): failed to get supported formats");
		return ret;
	}

	for (i = 0; i < num_mod_fmts; i++) {
		if (get_sample_bitdepth(mod_fmts[i]) >= least_req_depth &&
		    is_supported_fmt(mod_fmts[i])) {
			/* set frame format to inner model */
			comp_dbg(dev, "smart_amp_resolve_mod_fmt(): set mod format to %u",
				 mod_fmts[i]);
			ret = mod->mod_ops->set_fmt(mod, mod_fmts[i]);
			if (ret) {
				comp_err(dev,
					 "smart_amp_resolve_mod_fmt(): failed setting format %u",
					 mod_fmts[i]);
				return ret;
			}
			/* return the resolved format for later settings. */
			return mod_fmts[i];
		}
	}

	comp_err(dev, "smart_amp_resolve_mod_fmt(): failed to resolve the frame format for mod");
	return -EINVAL;
}

static int smart_amp_prepare(struct comp_dev *dev)
{
	struct smart_amp_data *sad = comp_get_drvdata(dev);
	uint16_t ff_src_fmt, fb_src_fmt, resolved_mod_fmt;
	uint32_t least_req_depth;
	uint32_t rate;
	int ret;

	comp_dbg(dev, "smart_amp_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	/* searching for stream and feedback source buffers */
	struct comp_buffer *source_buffer;

	comp_dev_for_each_producer(dev, source_buffer) {
		if (comp_buffer_get_source_component(source_buffer)->ipc_config.type
				== SOF_COMP_DEMUX)
			sad->feedback_buf = source_buffer;
		else
			sad->source_buf = source_buffer;
	}

	/* sink buffer */
	sad->sink_buf = comp_dev_get_first_data_consumer(dev);

	/* get frame format and channels param of stream and feedback source */
	ff_src_fmt = audio_stream_get_frm_fmt(&sad->source_buf->stream);
	sad->ff_mod.channels = MIN(SMART_AMP_FF_MAX_CH_NUM,
				   audio_stream_get_channels(&sad->source_buf->stream));
	sad->out_mod.channels = sad->ff_mod.channels;

	/* the least bitdepth required for inner model, which should not be lower than the bitdepth
	 * for input samples of feed-forward, and feedback if exists.
	 */
	least_req_depth = get_sample_bitdepth(ff_src_fmt);
	if (sad->feedback_buf) {
		/* forward set channels and rate param to feedback source */
		audio_stream_set_channels(&sad->feedback_buf->stream,
					  sad->config.feedback_channels);
		audio_stream_set_rate(&sad->feedback_buf->stream,
				      audio_stream_get_rate(&sad->source_buf->stream));
		fb_src_fmt = audio_stream_get_frm_fmt(&sad->feedback_buf->stream);
		sad->fb_mod.channels = MIN(SMART_AMP_FB_MAX_CH_NUM,
					   audio_stream_get_channels(&sad->feedback_buf->stream));

		least_req_depth = MAX(least_req_depth, get_sample_bitdepth(fb_src_fmt));
	}

	/* resolve the frame format for inner model. The return value will be the applied format
	 * or the negative error code.
	 */
	ret = smart_amp_resolve_mod_fmt(dev, least_req_depth);
	if (ret < 0)
		return ret;

	resolved_mod_fmt = ret;

	/* set format to mod buffers and get the corresponding src/sink function of channel
	 * remapping and format conversion.
	 */
	sad->ff_mod.frame_fmt = resolved_mod_fmt;
	sad->out_mod.frame_fmt = resolved_mod_fmt;
	sad->ff_get_frame = smart_amp_get_src_func(ff_src_fmt, sad->ff_mod.frame_fmt);
	sad->ff_set_frame = smart_amp_get_sink_func(ff_src_fmt, sad->out_mod.frame_fmt);
	comp_dbg(dev, "smart_amp_prepare(): ff mod buffer channels:%u fmt_conv:%u -> %u",
		 sad->ff_mod.channels, ff_src_fmt, sad->ff_mod.frame_fmt);
	comp_dbg(dev, "smart_amp_prepare(): output mod buffer channels:%u fmt_conv:%u -> %u",
		 sad->out_mod.channels, sad->out_mod.frame_fmt, ff_src_fmt);

	if (sad->feedback_buf) {
		sad->fb_mod.frame_fmt = resolved_mod_fmt;
		sad->fb_get_frame = smart_amp_get_src_func(fb_src_fmt, sad->fb_mod.frame_fmt);
		comp_dbg(dev, "smart_amp_prepare(): fb mod buffer channels:%u fmt_conv:%u -> %u",
			 sad->fb_mod.channels, fb_src_fmt, sad->fb_mod.frame_fmt);
	}
	return 0;
}

static const struct comp_driver comp_smart_amp = {
	.type = SOF_COMP_SMART_AMP,
	.uid = SOF_RT_UUID(UUID_SYM),
	.tctx = &smart_amp_comp_tr,
	.ops = {
		.create = smart_amp_new,
		.free = smart_amp_free,
		.params = smart_amp_params,
		.prepare = smart_amp_prepare,
#if CONFIG_IPC_MAJOR_3
		.cmd = smart_amp_cmd,
#endif
		.trigger = smart_amp_trigger,
		.copy = smart_amp_copy,
		.reset = smart_amp_reset,
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
