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
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/trace/trace.h>
#include <sof/ipc/msg.h>
#include <sof/ut.h>
#include <user/smart_amp.h>
#include <sof/audio/ipc-config.h>
#include <sof/audio/smart_amp/smart_amp.h>

/* NOTE: this code builds itself with one of two distinct UUIDs
 * depending on configuration!
 */
#if CONFIG_MAXIM_DSM
SOF_DEFINE_REG_UUID(maxim_dsm);
#define UUID_SYM maxim_dsm_uuid

#else /* Passthrough */
SOF_DEFINE_REG_UUID(passthru_smart_amp);
#define UUID_SYM passthru_smart_amp_uuid

#endif

DECLARE_TR_CTX(smart_amp_comp_tr, SOF_UUID(UUID_SYM),
	       LOG_LEVEL_INFO);

LOG_MODULE_REGISTER(smart_amp, CONFIG_SOF_LOG_LEVEL);

/* Amp configuration & model calibration data for tuning/debug */
#define SOF_SMART_AMP_CONFIG 0
#define SOF_SMART_AMP_MODEL 1

struct smart_amp_data {
	struct processing_module *mod;
	struct sof_smart_amp_config config;
	struct comp_buffer *source_buf; /**< stream source buffer */
	struct comp_buffer *feedback_buf; /**< feedback source buffer */
	struct comp_buffer *sink_buf; /**< sink buffer */

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
	mod_free(sad->mod, sad->ff_mod.buf.data);
	sad->ff_mod.buf.data = NULL;
	/* sof -> mod feedback data re-mapping and format conversion */
	mod_free(sad->mod, sad->fb_mod.buf.data);
	sad->fb_mod.buf.data = NULL;
	/* mod -> sof processed data format conversion */
	mod_free(sad->mod, sad->out_mod.buf.data);
	sad->out_mod.buf.data = NULL;

	/* mem block for mod private data usage */
	mod_free(sad->mod, sad->mod_mems[MOD_MEMBLK_PRIVATE].data);
	sad->mod_mems[MOD_MEMBLK_PRIVATE].data = NULL;
	/* mem block for mod audio frame data usage */
	mod_free(sad->mod, sad->mod_mems[MOD_MEMBLK_FRAME].data);
	sad->mod_mems[MOD_MEMBLK_FRAME].data = NULL;
	/* mem block for mod parameter blob usage */
	mod_free(sad->mod, sad->mod_mems[MOD_MEMBLK_PARAM].data);
	sad->mod_mems[MOD_MEMBLK_PARAM].data = NULL;

	/* inner model data struct */
	mod_free(sad->mod, sad->mod_data);
	sad->mod_data = NULL;
}

static inline int smart_amp_buf_alloc(struct processing_module *mod,
				      struct smart_amp_buf *buf, size_t size)
{
	buf->data = mod_alloc(mod, size);
	if (!buf->data)
		return -ENOMEM;
	buf->size = size;
	return 0;
}

static ssize_t smart_amp_alloc_mod_memblk(struct smart_amp_data *sad,
					  enum smart_amp_mod_memblk blk)
{
	struct smart_amp_mod_data_base *smod = sad->mod_data;
	int ret;
	size_t size;

	/* query the required size from inner model. */
	ret = smod->mod_ops->query_memblk_size(smod, blk);
	if (ret < 0)
		goto error;

	/* no memory required */
	if (ret == 0)
		return 0;

	/* allocate the memory block when returned size > 0. */
	size = ret;
	ret = smart_amp_buf_alloc(sad->mod, &sad->mod_mems[blk], size);
	if (ret < 0)
		goto error;

	/* provide the memory block information to inner model. */
	ret = smod->mod_ops->set_memblk(smod, blk, &sad->mod_mems[blk]);
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
	ret = smart_amp_buf_alloc(sad->mod, &sad->ff_mod.buf, size);
	if (ret < 0)
		goto error;
	total_size = size;

	/* sof -> mod feedback data re-mapping and format conversion */
	size = SMART_AMP_FB_BUF_DB_SZ * sizeof(int32_t);
	ret = smart_amp_buf_alloc(sad->mod, &sad->fb_mod.buf, size);
	if (ret < 0)
		goto error;
	total_size += size;

	/* mod -> sof processed data format conversion */
	size = SMART_AMP_FF_BUF_DB_SZ * sizeof(int32_t);
	ret = smart_amp_buf_alloc(sad->mod, &sad->out_mod.buf, size);
	if (ret < 0)
		goto error;
	total_size += size;

	comp_dbg(dev, "used data buffer %zu bytes", total_size);
	return 0;

error:
	smart_amp_free_mod_memories(sad);
	return ret;
}

static int smart_amp_init(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct module_config *mcfg = &mod->priv.cfg;
	struct smart_amp_data *sad;
	struct sof_smart_amp_config *cfg;
	size_t bs;
	int ret;

	/* Allocate smart_amp_data using module API */
	sad = mod_zalloc(mod, sizeof(*sad));
	if (!sad)
		return -ENOMEM;

	mod->priv.private = sad;
	sad->mod = mod;

	/* Copy config blob if present */
	if (mcfg->avail && mcfg->init_data && mcfg->size >= sizeof(struct sof_smart_amp_config)) {
		cfg = (struct sof_smart_amp_config *)mcfg->init_data;
		bs = mcfg->size;
		memcpy_s(&sad->config, sizeof(struct sof_smart_amp_config), cfg, bs);
	}

	/* allocate inner model data struct */
	sad->mod_data = mod_data_create(dev);
	if (!sad->mod_data) {
		comp_err(dev, "failed to allocate nner model data");
		ret = -ENOMEM;
		goto error;
	}

	/* allocate stream buffers for mod */
	ret = smart_amp_alloc_data_buffers(dev, sad);
	if (ret) {
		comp_err(dev, "failed to allocate data buffers, ret:%d", ret);
		goto error;
	}

	/* (before init) allocate mem block for mod private data usage */
	ret = smart_amp_alloc_mod_memblk(sad, MOD_MEMBLK_PRIVATE);
	if (ret < 0) {
		comp_err(dev, "failed to allocate mod private, ret:%d", ret);
		goto error;
	}
	comp_dbg(dev, "used mod private buffer %d bytes", ret);

	/* init model */
	ret = sad->mod_data->mod_ops->init(sad->mod_data);
	if (ret) {
		comp_err(dev, "failed to init inner model, ret:%d", ret);
		goto error;
	}

	/* (after init) allocate mem block for mod audio frame data usage */
	ret = smart_amp_alloc_mod_memblk(sad, MOD_MEMBLK_FRAME);
	if (ret < 0) {
		comp_err(dev, "failed to allocate mod buffer, ret:%d", ret);
		goto error;
	}
	comp_dbg(dev, "used mod data buffer %d bytes", ret);

	/* (after init) allocate mem block for mod parameter blob usage */
	ret = smart_amp_alloc_mod_memblk(sad, MOD_MEMBLK_PARAM);
	if (ret < 0) {
		comp_err(dev, "failed to allocate mod config, ret:%d", ret);
		goto error;
	}
	comp_dbg(dev, "used mod config buffer %d bytes", ret);

	return 0;
error:
	smart_amp_free_mod_memories(sad);
	mod_free(mod, sad);
	return ret;
}

static int smart_amp_set_config(struct processing_module *mod,
				struct sof_ipc_ctrl_data *cdata)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct sof_smart_amp_config *cfg;
	struct comp_dev *dev = mod->dev;
	size_t bs;

	/* Copy new config, find size from header */
	cfg = (struct sof_smart_amp_config *)
	      ASSUME_ALIGNED(&cdata->data->data, sizeof(uint32_t));
	bs = cfg->size;

	comp_dbg(dev, "smart_amp_set_config(), actual blob size = %zu, expected blob size = %zu",
		 bs, sizeof(struct sof_smart_amp_config));

	if (bs != sizeof(struct sof_smart_amp_config)) {
		comp_err(dev, "invalid blob size, actual blob size = %zu, expected blob size = %zu",
			 bs, sizeof(struct sof_smart_amp_config));
		return -EINVAL;
	}

	memcpy_s(&sad->config, sizeof(struct sof_smart_amp_config), cfg,
		 sizeof(struct sof_smart_amp_config));

	return 0;
}

static int smart_amp_get_config(struct processing_module *mod,
				struct sof_ipc_ctrl_data *cdata, int size)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
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

static int smart_amp_ctrl_get_bin_data(struct processing_module *mod,
				       struct sof_ipc_ctrl_data *cdata,
				       int size)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct smart_amp_mod_data_base *smod;
	struct comp_dev *dev = mod->dev;
	int ret = 0;

	assert(sad);
	smod = sad->mod_data;

	switch (cdata->data->type) {
	case SOF_SMART_AMP_CONFIG:
		ret = smart_amp_get_config(mod, cdata, size);
		break;
	case SOF_SMART_AMP_MODEL:
		ret = smod->mod_ops->get_config(smod, cdata, size);
		if (ret < 0) {
			comp_err(dev, "failed to read inner model!");
			return ret;
		}
		break;
	default:
		comp_err(dev, "unknown binary data type");
		break;
	}

	return ret;
}

static int smart_amp_get_configuration(struct processing_module *mod,
				       uint32_t config_id, uint32_t *data_offset_size,
				       uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "config_id %u size: %zu", config_id, fragment_size);

#if CONFIG_IPC_MAJOR_3
	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		return smart_amp_ctrl_get_bin_data(mod, cdata, fragment_size);
	default:
		comp_err(dev, "invalid cdata->cmd");
	}
#elif CONFIG_IPC_MAJOR_4
	return smart_amp_ctrl_get_bin_data(mod, cdata, fragment_size);
#endif
	return -EINVAL;
}

static int smart_amp_ctrl_set_bin_data(struct processing_module *mod,
				       struct sof_ipc_ctrl_data *cdata)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct smart_amp_mod_data_base *smod;
	struct comp_dev *dev = mod->dev;
	int ret = 0;

	assert(sad);
	smod = sad->mod_data;

	if (dev->state < COMP_STATE_READY) {
		comp_err(dev, "driver in init!");
		return -EBUSY;
	}

	switch (cdata->data->type) {
	case SOF_SMART_AMP_CONFIG:
		ret = smart_amp_set_config(mod, cdata);
		break;
	case SOF_SMART_AMP_MODEL:
		ret = smod->mod_ops->set_config(smod, cdata);
		if (ret < 0) {
			comp_err(dev, "failed to write inner model!");
			return ret;
		}
		break;
	default:
		comp_err(dev, "unknown binary data type");
		break;
	}

	return ret;
}

static int smart_amp_ctrl_set_data(struct processing_module *mod,
				   struct sof_ipc_ctrl_data *cdata)
{
	struct comp_dev *dev = mod->dev;
	int ret = 0;

	/* Check version from ABI header */
	if (SOF_ABI_VERSION_INCOMPATIBLE(SOF_ABI_VERSION, cdata->data->abi)) {
		comp_err(dev, "invalid version");
		return -EINVAL;
	}

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_dbg(dev, "SOF_CTRL_CMD_BINARY");
		ret = smart_amp_ctrl_set_bin_data(mod, cdata);
		break;
	default:
		comp_err(dev, "invalid cdata->cmd");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int smart_amp_set_configuration(struct processing_module *mod,
				       uint32_t config_id,
				       enum module_cfg_fragment_position pos,
				       uint32_t data_offset_size,
				       const uint8_t *fragment, size_t fragment_size,
				       uint8_t *response, size_t response_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct comp_dev *dev = mod->dev;

	comp_info(dev, "config_id %u size %zu", config_id, response_size);

#if CONFIG_IPC_MAJOR_3
	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		comp_info(dev, "SOF_CTRL_CMD_BINARY");
		return smart_amp_ctrl_set_data(mod, cdata);
	default:
		comp_err(dev, "unknown cmd %d", cdata->cmd);
	}
#elif CONFIG_IPC_MAJOR_4
	return smart_amp_ctrl_set_data(mod, cdata);
#endif
	return -EINVAL;
}

static int smart_amp_free(struct processing_module *mod)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "smart_amp_free()");
	if (!sad)
		return 0;

	smart_amp_free_mod_memories(sad);

	mod_free(mod, sad);
	mod->priv.private = NULL;

	return 0;
}

#if CONFIG_IPC_MAJOR_4
static void smart_amp_ipc4_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_buffer *sinkb, *sourceb;
	struct comp_dev *dev = mod->dev;

	ipc4_base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, params);
	component_set_nearest_period_frames(dev, params->rate);

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
	ipc4_update_buffer_format(sourceb, &mod->priv.cfg.base_cfg.audio_fmt);

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	ipc4_update_buffer_format(sinkb, &mod->priv.cfg.base_cfg.audio_fmt);
}
#endif /* CONFIG_IPC_MAJOR_4 */

static int smart_amp_trigger(struct processing_module *mod, int cmd)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
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

static int smart_amp_ff_process(struct processing_module *mod,
				const struct audio_stream *source,
				const struct audio_stream *sink,
				uint32_t frames, const int8_t *chan_map)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct smart_amp_mod_data_base *smod = sad->mod_data;
	struct comp_dev *dev = mod->dev;
	int ret;

	sad->ff_mod.consumed = 0;
	sad->out_mod.produced = 0;

	if (frames == 0) {
		comp_warn(dev, "feed forward frame size zero warning.");
		return 0;
	}

	if (frames > SMART_AMP_FF_BUF_DB_SZ) {
		comp_err(dev, "feed forward frame size overflow: %u", frames);
		sad->ff_mod.consumed = frames;
		return -EINVAL;
	}

	sad->ff_get_frame(&sad->ff_mod, frames, source, chan_map);

	ret = smod->mod_ops->ff_proc(smod, frames, &sad->ff_mod, &sad->out_mod);
	if (ret) {
		comp_err(dev, "feed forward inner model process error");
		return ret;
	}

	sad->ff_set_frame(&sad->out_mod, sad->out_mod.produced, sink);

	return 0;
}

static int smart_amp_fb_process(struct processing_module *mod,
				const struct audio_stream *source,
				uint32_t frames, const int8_t *chan_map)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct smart_amp_mod_data_base *smod = sad->mod_data;
	struct comp_dev *dev = mod->dev;
	int ret;

	sad->fb_mod.consumed = 0;

	if (frames == 0) {
		comp_warn(dev, "feedback frame size zero warning.");
		return 0;
	}

	if (frames > SMART_AMP_FB_BUF_DB_SZ) {
		comp_err(dev, "feedback frame size overflow: %u", frames);
		sad->fb_mod.consumed = frames;
		return -EINVAL;
	}

	sad->fb_get_frame(&sad->fb_mod, frames, source, chan_map);

	ret = smod->mod_ops->fb_proc(smod, frames, &sad->fb_mod);
	if (ret) {
		comp_err(dev, "feedback inner model process error");
		return ret;
	}

	return 0;
}

static int smart_amp_process(struct processing_module *mod,
			     struct sof_source **sources, int num_of_sources,
			     struct sof_sink **sinks, int num_of_sinks)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *source_buf = sad->source_buf;
	struct comp_buffer *sink_buf = comp_buffer_get_from_sink(sinks[0]);
	uint32_t avail_passthrough_frames;
	uint32_t avail_feedback_frames;
	uint32_t avail_frames;
	uint32_t source_bytes;
	uint32_t sink_bytes;
	uint32_t feedback_bytes;

	comp_dbg(dev, "%d sources %d sinks", num_of_sources, num_of_sinks);

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

			comp_dbg(dev, "processing %u feedback frames (avail_passthrough_frames: %u)",
				 avail_feedback_frames, avail_passthrough_frames);

			/* perform buffer writeback after source_buf process */
			buffer_stream_invalidate(feedback_buf, feedback_bytes);
			smart_amp_fb_process(mod, &feedback_buf->stream,
					     avail_feedback_frames,
					     sad->config.feedback_ch_map);

			comp_dbg(dev, "consumed %u feedback frames",
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
	smart_amp_ff_process(mod, &source_buf->stream, &sink_buf->stream,
			     avail_frames, sad->config.source_ch_map);

	comp_dbg(dev, "processing %u feed forward frames (consumed: %u, produced: %u)",
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

static int smart_amp_reset(struct processing_module *mod)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct smart_amp_mod_data_base *smod = sad->mod_data;
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "smart_amp_reset()");

	sad->ff_get_frame = NULL;
	sad->fb_get_frame = NULL;
	sad->ff_set_frame = NULL;

	/* reset inner model */
	return smod->mod_ops->reset(smod);
}

/* supported formats: {SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE}
 * which are enumerated as 0, 1, and 2. Simply check if supported while fmt <= 2.
 */
static inline bool is_supported_fmt(uint16_t fmt)
{
	return fmt <= SOF_IPC_FRAME_S32_LE;
}

static int smart_amp_resolve_mod_fmt(struct processing_module *mod, uint32_t least_req_depth)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct smart_amp_mod_data_base *smod = sad->mod_data;
	struct comp_dev *dev = mod->dev;
	const uint16_t *mod_fmts;
	int num_mod_fmts;
	int ret;
	int i;

	/* get supported formats from mod */
	ret = smod->mod_ops->get_supported_fmts(smod, &mod_fmts, &num_mod_fmts);
	if (ret) {
		comp_err(dev, "failed to get supported formats");
		return ret;
	}

	for (i = 0; i < num_mod_fmts; i++) {
		if (get_sample_bitdepth(mod_fmts[i]) >= least_req_depth &&
		    is_supported_fmt(mod_fmts[i])) {
			/* set frame format to inner model */
			comp_dbg(dev, "set mod format to %u",
				 mod_fmts[i]);
			ret = smod->mod_ops->set_fmt(smod, mod_fmts[i]);
			if (ret) {
				comp_err(dev,
					 "failed setting format %u",
					 mod_fmts[i]);
				return ret;
			}
			/* return the resolved format for later settings. */
			return mod_fmts[i];
		}
	}

	comp_err(dev, "failed to resolve the frame format for mod");
	return -EINVAL;
}

static int smart_amp_prepare(struct processing_module *mod,
			     struct sof_source **sources, int num_of_sources,
			     struct sof_sink **sinks, int num_of_sinks)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	uint16_t ff_src_fmt, fb_src_fmt, resolved_mod_fmt;
	uint32_t least_req_depth;
	int ret, i;

	comp_dbg(dev, "%d sources %d sinks", num_of_sources, num_of_sinks);
#if CONFIG_IPC_MAJOR_4
	smart_amp_ipc4_params(mod);
#endif

	/* In module API, state is managed by the framework, so no comp_set_state needed */
	for (i = 0; i < num_of_sources; i++) {
		/* NOTE: This should not work in module based environment:
		 *	 sources[i]->bound_module->dev->ipc_config.type == SOF_COMP_DEMUX
		 *	 So let's check which one of the sources is from a capture stream.
		 *	 The code is not tested and may not work.
		 */
		if (sources[i]->bound_module->dev->direction == SOF_IPC_STREAM_CAPTURE)
			sad->feedback_buf = comp_buffer_get_from_source(sources[i]);
		else
			sad->source_buf = comp_buffer_get_from_source(sources[i]);
	}

	/* sink buffer */
	sad->sink_buf = comp_buffer_get_from_sink(sinks[0]);
	if (!sad->sink_buf) {
		comp_err(dev, "no sink buffer");
		return -ENOTCONN;
	}

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
	ret = smart_amp_resolve_mod_fmt(mod, least_req_depth);
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
	comp_dbg(dev, "ff mod buffer channels:%u fmt_conv:%u -> %u",
		 sad->ff_mod.channels, ff_src_fmt, sad->ff_mod.frame_fmt);
	comp_dbg(dev, "output mod buffer channels:%u fmt_conv:%u -> %u",
		 sad->out_mod.channels, sad->out_mod.frame_fmt, ff_src_fmt);

	if (sad->feedback_buf) {
		sad->fb_mod.frame_fmt = resolved_mod_fmt;
		sad->fb_get_frame = smart_amp_get_src_func(fb_src_fmt, sad->fb_mod.frame_fmt);
		comp_dbg(dev, "fb mod buffer channels:%u fmt_conv:%u -> %u",
			 sad->fb_mod.channels, fb_src_fmt, sad->fb_mod.frame_fmt);
	}

	return 0;
}

static struct module_interface smart_amp_interface = {
	.init = smart_amp_init,
	.prepare = smart_amp_prepare,
	.process = smart_amp_process,
	.set_configuration = smart_amp_set_configuration,
	.get_configuration = smart_amp_get_configuration,
	.reset = smart_amp_reset,
	.free = smart_amp_free,
	.trigger = smart_amp_trigger,
};

DECLARE_MODULE_ADAPTER(smart_amp_interface, UUID_SYM, smart_amp_comp_tr);
SOF_MODULE_INIT(smart_amp, sys_comp_module_smart_amp_interface_init);
