// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <sof/compiler_attributes.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/notification_pool.h>
#include <rtos/init.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <ipc4/base-config.h>
#include <ipc4/notification.h>
#include <user/trace.h>
#include <stddef.h>
#include <stdint.h>

#include "mixin_mixout.h"

LOG_MODULE_REGISTER(mixin_mixout, CONFIG_SOF_LOG_LEVEL);

/* mixin 39656eb2-3b71-4049-8d3f-f92cd5c43c09 */
SOF_DEFINE_REG_UUID(mixin);
DECLARE_TR_CTX(mixin_tr, SOF_UUID(mixin_uuid), LOG_LEVEL_INFO);

/* mixout 3c56505a-24d7-418f-bddc-c1f5a3ac2ae0 */
SOF_DEFINE_REG_UUID(mixout);
DECLARE_TR_CTX(mixout_tr, SOF_UUID(mixout_uuid), LOG_LEVEL_INFO);

#define MIXIN_MAX_SINKS IPC4_MIXIN_MODULE_MAX_OUTPUT_QUEUES
#define MIXOUT_MAX_SOURCES IPC4_MIXOUT_MODULE_MAX_INPUT_QUEUES

/*
 * Unfortunately, if we have to support a topology with a single mixin
 * connected to multiple mixouts, we cannot use simple implementation as in
 * mixer component. We either need to use intermediate buffer between mixin and
 * mixout, or use a more complex implementation as described below.
 *
 * This implementation does not use buffer between mixin and mixout. Mixed
 * data is written directly to mixout sink buffer. Most of the mixing is done
 * by mixins in mixin_process(). Simply speaking, if no data present in mixout
 * sink -- mixin just copies its source data to mixout sink. If mixout sink
 * has some data (written there previously by some other mixin) -- mixin reads
 * data from mixout sink, mixes it with its source data and writes back to
 * mixout sink.
 *
 * Such implementation has less buffer reads/writes than simple implementation
 * using intermediate buffer between mixin and mixout.
 */

struct mixin_sink_config {
	enum ipc4_mixer_mode mixer_mode;
	uint32_t output_channel_count;
	uint32_t output_channel_map;
	/* Gain as described in struct ipc4_mixer_mode_sink_config */
	uint16_t gain;
};

/* mixin component private data */
struct mixin_data {
	mix_func mix;
	mix_func gain_mix;
	struct mixin_sink_config sink_config[MIXIN_MAX_SINKS];
#if CONFIG_XRUN_NOTIFICATIONS_ENABLE
	uint32_t last_reported_underrun;
	uint32_t underrun_notification_period;
#endif
};

/*
 * Mixin calls "consume" on its source data but never calls "produce" -- that one is called
 * by mixout for its sink data. So between mixin_process() and mixout_process() a number of
 * consumed (in mixin) yet not produced (in mixout) frames should be stored for each mixin
 * and mixout pair.
 */
struct pending_frames {
	struct comp_dev *mixin;
	uint32_t frames;
};

/* mixout component private data */
struct mixout_data {
	/* number of currently mixed frames in mixout sink buffer */
	uint32_t mixed_frames;

	/*
	 * Source data is consumed by mixins in mixin_process() but sink data cannot be
	 * immediately produced. Sink data is produced by mixout in mixout_process() after
	 * ensuring all connected mixins have mixed their data into mixout sink buffer.
	 * So for each connected mixin, mixout keeps knowledge of data already consumed
	 * by mixin but not yet produced in mixout.
	 */
	struct pending_frames pending_frames[MIXOUT_MAX_SOURCES];

	/*
	 * When several mixins are connected to one mixout (a typical case) mixout sink
	 * buffer is acquired (via sink_get_buffer() call) in mixin_process() of first
	 * mixin. Other connected mixins just use a pointer to the buffer stored below.
	 * The buffer is released (committed by sink_commit_buffer() call) in mixout_process().
	 */
	struct cir_buf_ptr acquired_buf;
	uint32_t acquired_buf_free_frames;
};

/* NULL is also a valid mixin argument: in such case the function returns first unused entry */
static struct pending_frames *get_mixin_pending_frames(struct mixout_data *mixout_data,
						       const struct comp_dev *mixin)
{
	int i;

	for (i = 0; i < MIXOUT_MAX_SOURCES; i++)
		if (mixout_data->pending_frames[i].mixin == mixin)
			return &mixout_data->pending_frames[i];

	return NULL;
}

static int mixin_init(struct processing_module *mod)
{
	struct module_data *mod_data = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct mixin_data *md;
	int i;

	comp_dbg(dev, "mixin_init()");

	md = mod_zalloc(mod, sizeof(*md));
	if (!md)
		return -ENOMEM;

	mod_data->private = md;
#if CONFIG_XRUN_NOTIFICATIONS_ENABLE
	md->underrun_notification_period = MIXIN_MODULE_DEFAULT_UNDERRUN_NOTIFICATION_PERIOD;
#endif

	for (i = 0; i < MIXIN_MAX_SINKS; i++) {
		md->sink_config[i].mixer_mode = IPC4_MIXER_NORMAL_MODE;
		md->sink_config[i].gain = IPC4_MIXIN_UNITY_GAIN;
	}

	mod->skip_src_buffer_invalidate = true;

	mod->max_sinks = MIXIN_MAX_SINKS;
	return 0;
}

static int mixout_init(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct mixout_data *mo_data;

	comp_dbg(dev, "mixout_new()");

	mo_data = mod_zalloc(mod, sizeof(*mo_data));
	if (!mo_data)
		return -ENOMEM;

	mod->priv.private = mo_data;

	mod->skip_sink_buffer_writeback = true;

	mod->max_sources = MIXOUT_MAX_SOURCES;
	return 0;
}

static int mixin_free(struct processing_module *mod)
{
	return 0;
}

static int mixout_free(struct processing_module *mod)
{
	return 0;
}

static int mix(struct comp_dev *dev, const struct mixin_data *mixin_data,
	       uint16_t sink_index, struct cir_buf_ptr *sink,
	       uint32_t start_sample, uint32_t mixed_samples,
	       const struct cir_buf_ptr *source, uint32_t sample_count)
{
	const struct mixin_sink_config *sink_config;

	if (sink_index >= MIXIN_MAX_SINKS) {
		comp_err(dev, "Sink index out of range: %u, max sinks count: %u",
			 (uint32_t)sink_index, MIXIN_MAX_SINKS);
		return -EINVAL;
	}

	sink_config = &mixin_data->sink_config[sink_index];

	if (sink_config->gain == IPC4_MIXIN_UNITY_GAIN) {
		mixin_data->mix(sink, start_sample, mixed_samples,
			source, sample_count, sink_config->gain);
	} else {
		mixin_data->gain_mix(sink, start_sample, mixed_samples,
			     source, sample_count, sink_config->gain);
	}

	return 0;
}

/* mix silence into stream, i.e. set not yet mixed data in stream to zero */
static void silence(struct cir_buf_ptr *stream, uint32_t start_offset,
		    uint32_t mixed_bytes, uint32_t size)
{
	uint32_t skip_mixed_bytes;
	uint8_t *ptr;
	int n;

	assert(mixed_bytes >= start_offset);
	skip_mixed_bytes = mixed_bytes - start_offset;

	if (size <= skip_mixed_bytes)
		return;

	size -= skip_mixed_bytes;
	ptr = (uint8_t *)stream->ptr + mixed_bytes;

	while (size) {
		ptr = cir_buf_wrap(ptr, stream->buf_start, stream->buf_end);
		n = MIN((uint8_t *)stream->buf_end - ptr, size);
		memset(ptr, 0, n);
		size -= n;
		ptr += n;
	}
}

#if CONFIG_XRUN_NOTIFICATIONS_ENABLE
static void mixin_check_notify_underrun(struct comp_dev *dev, struct mixin_data *mixin_data,
					size_t source_avail, size_t sinks_free)
{
	struct ipc_msg *notify;

	mixin_data->last_reported_underrun++;

	if (!source_avail && mixin_data->last_reported_underrun >=
	    mixin_data->underrun_notification_period) {
		mixin_data->last_reported_underrun = 0;

		notify = ipc_notification_pool_get(IPC4_RESOURCE_EVENT_SIZE);
		if (!notify)
			return;

		mixer_underrun_notif_msg_init(notify, dev->ipc_config.id, false,
					      source_avail, sinks_free);
		ipc_msg_send(notify, notify->tx_data, false);
	}
}
#endif

/* Most of the mixing is done here on mixin side. mixin mixes its source data
 * into each connected mixout sink buffer. Basically, if mixout sink buffer has
 * no data, mixin copies its source data into mixout sink buffer. If mixout sink
 * buffer has some data (written there by other mixin), mixin reads mixout sink
 * buffer data, mixes it with its source data and writes back to mixout sink
 * buffer. So after all mixin mixin_process() calls, mixout sink buffer contains
 * mixed data. Every mixin calls xxx_consume() on its processed source data, but
 * they do not call xxx_produce(). That is done on mixout side in mixout_process().
 *
 * Since there is no garantie that mixout processing is done in time we have
 * to account for a possibility having not yet produced data in mixout sink
 * buffer that was written there on previous run(s) of mixin_process(). So for each
 * mixin <--> mixout pair we track consumed yet not produced (pending_frames) data
 * amount. That value is also used in mixout_process() to calculate how many data
 * was actually mixed and so xxx_produce() is called for that amount.
 */
static int mixin_process(struct processing_module *mod,
			 struct sof_source **sources, int num_of_sources,
			 struct sof_sink **sinks, int num_of_sinks)
{
	struct mixin_data *mixin_data = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	uint32_t source_avail_frames, sinks_free_frames;
	struct processing_module *active_mixouts[MIXIN_MAX_SINKS];
	uint16_t sinks_ids[MIXIN_MAX_SINKS];
	uint32_t bytes_to_consume = 0;
	uint32_t frames_to_copy;
	struct pending_frames *pending_frames;
	int i, ret;
	struct cir_buf_ptr source_ptr;

	comp_dbg(dev, "mixin_process()");

	source_avail_frames = source_get_data_frames_available(sources[0]);
	sinks_free_frames = INT32_MAX;

	if (num_of_sinks > MIXIN_MAX_SINKS) {
		comp_err(dev, "Invalid output sink count %d",
			 num_of_sinks);
		return -EINVAL;
	}

	/* first, let's find out how many frames can be now processed --
	 * it is a minimal value among frames available in source buffer
	 * and frames free in each connected mixout sink buffer.
	 */
	for (i = 0; i < num_of_sinks; i++) {
		struct comp_buffer *unused_in_between_buf;
		struct comp_dev *mixout;
		struct sof_sink *mixout_sink;
		struct mixout_data *mixout_data;
		struct processing_module *mixout_mod;
		uint32_t free_frames;

		/* WORKAROUND: since mixin is always connected to mixout, we can safely assume
		 * mixin sink interface is implemented via comp_buffer. This is, of course,
		 * not the case for other modules.
		 * TODO: find out a solution to reach mixout without knowledge of mixin
		 * sof_sink implementation.
		 */
		/* unused buffer between mixin and mixout */
		unused_in_between_buf = comp_buffer_get_from_sink(sinks[i]);
		mixout = comp_buffer_get_sink_component(unused_in_between_buf);

		/* Skip non-active mixout like it is not connected so it does not
		 * block other possibly connected mixouts. In addition, non-active
		 * mixouts might have their sink buffer/interface not yet configured.
		 */
		if (mixout->state != COMP_STATE_ACTIVE) {
			active_mixouts[i] = NULL;
			continue;
		}

		mixout_mod = comp_mod(mixout);
		active_mixouts[i] = mixout_mod;
		mixout_sink = mixout_mod->sinks[0];

		/* mixout might be created on another pipeline. Its sink stream params are usually
		 * configured in .prepare(). It is possible that such .prepare() was not yet called
		 * for mixout pipeline. Hence the check above if mixout state is active. However,
		 * let's just in case check here if sink stream params are really configured as
		 * proceeding with unconfigured sink will lead to hard to debug bugs.
		 * Unconfigured stream params are filled with zeros.
		 * TODO: introduce something like sink_is_configured() ?
		 */
		if (!mixout_sink || sink_get_channels(mixout_sink) == 0) {
			comp_err(dev, "mixout sink not configured!");
			return -EINVAL;
		}

		sinks_ids[i] = IPC4_SRC_QUEUE_ID(buf_get_id(unused_in_between_buf));

		mixout_data = module_get_private_data(mixout_mod);
		pending_frames = get_mixin_pending_frames(mixout_data, dev);
		if (!pending_frames) {
			comp_err(dev, "No source info");
			return -EINVAL;
		}

		/* In theory, though unlikely, mixout sink can be connected to some module on
		 * another core. In this case free space in mixout sink buffer can suddenly increase
		 * (data consumed on another core) after the buffer was already acquired. Let's only
		 * access free space that was at the moment of acquiring the buffer.
		 */
		free_frames = mixout_data->acquired_buf.ptr ?
			mixout_data->acquired_buf_free_frames :
			sink_get_free_frames(mixout_sink);

		/* mixout sink buffer may still have not yet produced data -- data
		 * consumed and written there by mixin on previous mixin_process() run.
		 * We do NOT want to overwrite that data.
		 */
		assert(free_frames >= pending_frames->frames);
		sinks_free_frames = MIN(sinks_free_frames, free_frames - pending_frames->frames);
	}

	if (sinks_free_frames == 0 || sinks_free_frames == INT32_MAX)
		return 0;

#if CONFIG_XRUN_NOTIFICATIONS_ENABLE
	size_t frame_bytes = source_get_frame_bytes(sources[0]);
	size_t min_frames = MIN(dev->frames, sinks_free_frames);

	mixin_check_notify_underrun(dev, mixin_data,
				    source_avail_frames * frame_bytes,
				    min_frames * frame_bytes);
#endif

	if (source_avail_frames > 0) {
		size_t buf_size;

		frames_to_copy = MIN(source_avail_frames, sinks_free_frames);
		bytes_to_consume = frames_to_copy * source_get_frame_bytes(sources[0]);

		source_get_data(sources[0], bytes_to_consume, (const void **)&source_ptr.ptr,
				(const void **)&source_ptr.buf_start, &buf_size);
		source_ptr.buf_end = (uint8_t *)source_ptr.buf_start + buf_size;
	} else {
		/* if source does not produce any data -- do NOT block mixing but generate
		 * silence as that source output.
		 *
		 * here frames_to_copy is silence size.
		 *
		 * FIXME: does not work properly for freq like 44.1 kHz.
		 */
		frames_to_copy = MIN(dev->frames, sinks_free_frames);
	}

	/* iterate over all connected mixouts and mix source data into each mixout sink buffer */
	for (i = 0; i < num_of_sinks; i++) {
		struct mixout_data *mixout_data;
		struct processing_module *mixout_mod;
		uint32_t start_frame;

		mixout_mod = active_mixouts[i];
		if (!mixout_mod)
			continue;

		mixout_data = module_get_private_data(mixout_mod);
		pending_frames = get_mixin_pending_frames(mixout_data, dev);
		if (!pending_frames) {
			comp_err(dev, "No source info");
			return -EINVAL;
		}

		/* Skip data from previous run(s) not yet produced in mixout_process().
		 * Normally start_frame would be 0 unless mixout pipeline has serious
		 * performance problems with processing data on time in mixout.
		 */
		start_frame = pending_frames->frames;

		/* mixout sink buffer is acquired here by its first connected mixin and is
		 * released in mixout_process(). Other connected mixins just use a pointer
		 * stored in mixout_data->acquired_buf.
		 */
		if (!mixout_data->acquired_buf.ptr) {
			struct sof_sink *sink = mixout_mod->sinks[0];
			size_t free_bytes = sink_get_free_size(sink);
			size_t buf_size;

			sink_get_buffer(sink, free_bytes, &mixout_data->acquired_buf.ptr,
					&mixout_data->acquired_buf.buf_start, &buf_size);
			mixout_data->acquired_buf.buf_end =
				(uint8_t *)mixout_data->acquired_buf.buf_start + buf_size;
			mixout_data->acquired_buf_free_frames =
				free_bytes / sink_get_frame_bytes(sink);
		}

		/* if source does not produce any data but mixin is in active state -- generate
		 * silence instead of that source data
		 */
		if (source_avail_frames == 0) {
			uint32_t frame_bytes = sink_get_frame_bytes(mixout_mod->sinks[0]);

			/* generate silence */
			silence(&mixout_data->acquired_buf, start_frame * frame_bytes,
				mixout_data->mixed_frames * frame_bytes,
				frames_to_copy * frame_bytes);
		} else {
			uint32_t channel_count = sink_get_channels(mixout_mod->sinks[0]);

			/* basically, if sink buffer has no data -- copy source data there, if
			 * sink buffer has some data (written by another mixin) mix that data
			 * with source data.
			 */
			ret = mix(dev, mixin_data, sinks_ids[i], &mixout_data->acquired_buf,
				  start_frame * channel_count,
				  mixout_data->mixed_frames * channel_count,
				  &source_ptr, frames_to_copy * channel_count);
			if (ret < 0)
				return ret;
		}

		pending_frames->frames += frames_to_copy;

		if (frames_to_copy + start_frame > mixout_data->mixed_frames)
			mixout_data->mixed_frames = frames_to_copy + start_frame;
	}

	if (bytes_to_consume)
		source_release_data(sources[0], bytes_to_consume);

	return 0;
}

/* mixout just commits its sink buffer with data already mixed by mixins */
static int mixout_process(struct processing_module *mod,
			  struct sof_source **sources, int num_of_sources,
			  struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_dev *dev = mod->dev;
	struct mixout_data *md;
	uint32_t frames_to_produce = INT32_MAX;
	uint32_t bytes_to_produce;
	struct pending_frames *pending_frames;
	int i;

	comp_dbg(dev, "mixout_process()");

	md = module_get_private_data(mod);

	/* iterate over all connected mixins to find minimal value of frames they consumed
	 * (i.e., mixed into mixout sink buffer). That is the amount that can/should be
	 * produced now.
	 */
	for (i = 0; i < num_of_sources; i++) {
		struct comp_buffer *unused_in_between_buf;
		struct comp_dev *mixin;

		/* WORKAROUND: since mixin is always connected to mixout, we can safely assume
		 * mixout source interface is implemented via comp_buffer. This is, of course,
		 * not the case for other modules.
		 * TODO: find out a solution to reach mixin without knowledge of mixout
		 * sof_source implementation.
		 */
		unused_in_between_buf = comp_buffer_get_from_source(sources[i]);
		mixin = comp_buffer_get_source_component(unused_in_between_buf);

		pending_frames = get_mixin_pending_frames(md, mixin);
		if (!pending_frames)
			continue;

		if (mixin->state == COMP_STATE_ACTIVE || pending_frames->frames)
			frames_to_produce = MIN(frames_to_produce, pending_frames->frames);
	}

	if (frames_to_produce > 0 && frames_to_produce < INT32_MAX) {
		for (i = 0; i < num_of_sources; i++) {
			struct comp_buffer *unused_in_between_buf;
			struct comp_dev *mixin;

			unused_in_between_buf = comp_buffer_get_from_source(sources[i]);
			mixin = comp_buffer_get_source_component(unused_in_between_buf);

			pending_frames = get_mixin_pending_frames(md, mixin);
			if (!pending_frames)
				continue;

			if (pending_frames->frames >= frames_to_produce)
				pending_frames->frames -= frames_to_produce;
			else
				pending_frames->frames = 0;
		}

		assert(md->mixed_frames >= frames_to_produce);
		md->mixed_frames -= frames_to_produce;

		bytes_to_produce = frames_to_produce * sink_get_frame_bytes(sinks[0]);
	} else {
		/* FIXME: does not work properly for freq like 44.1 kHz */
		bytes_to_produce = dev->frames * sink_get_frame_bytes(sinks[0]);
		bytes_to_produce = MIN(bytes_to_produce, sink_get_free_size(sinks[0]));

		if (!md->acquired_buf.ptr) {
			size_t buf_size;

			sink_get_buffer(sinks[0], bytes_to_produce, &md->acquired_buf.ptr,
					&md->acquired_buf.buf_start, &buf_size);
			md->acquired_buf.buf_end = (uint8_t *)md->acquired_buf.buf_start + buf_size;
		}

		cir_buf_set_zero(md->acquired_buf.ptr, md->acquired_buf.buf_start,
				 md->acquired_buf.buf_end, bytes_to_produce);
	}

	sink_commit_buffer(sinks[0], bytes_to_produce);
	md->acquired_buf.ptr = NULL;

	return 0;
}

static int mixin_reset(struct processing_module *mod)
{
	struct mixin_data *mixin_data = module_get_private_data(mod);

	mixin_data->mix = NULL;
	mixin_data->gain_mix = NULL;

	return 0;
}

static int mixout_reset(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;

	/* FIXME: move this to module_adapter_reset() */
	if (dev->pipeline->source_comp->direction == SOF_IPC_STREAM_PLAYBACK) {
		int i;

		for (i = 0; i < mod->num_of_sources; i++) {
			const struct comp_buffer *source_buf;
			bool stop;

			/* WORKAROUND: since mixin is always connected to mixout, we can safely
			 * assume mixout source interface is implemented via comp_buffer. This is,
			 * of course, not the case for other modules.
			 * TODO: find out a solution to reach mixin without knowledge of mixout
			 * sof_source implementation.
			 */
			source_buf = comp_buffer_get_from_source(mod->sources[i]);
			stop = (dev->pipeline ==
					comp_buffer_get_source_component(source_buf)->pipeline &&
				comp_buffer_get_source_state(source_buf) > COMP_STATE_PAUSED);

			if (stop)
				/* should not reset the downstream components */
				return PPL_STATUS_PATH_STOP;
		}
	}

	return 0;
}

/* params are derived from base config for ipc4 path */
static int mixin_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_dev *dev = mod->dev;
	int i;
	int ret;

	comp_dbg(dev, "mixin_params()");

	ipc4_base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, params);

	/* Buffers between mixins and mixouts are not used (mixin writes data directly to mixout
	 * sink). But, anyway, let's setup these buffers properly just in case.
	 */

	/* FIXME: there are 2 problems with the loop below:
	 *
	 * (1) struct sof_audio_stream_params contains two frame format members: frame_fmt
	 * and valid_sample_fmt both of type enum sof_ipc_frame. That is excessive as
	 * enum sof_ipc_frame describes both container and sample size and so having one
	 * variable of this type is enough. frame_fmt is set by comp_verify_params(), however,
	 * valid_sample_fmt does not. Hence valid_sample_fmt is set below in a loop. If mess
	 * with having both frame_fmt and valid_sample_fmt in SOF is solved when this loop can
	 * be removed.
	 *
	 * (2) comp_verify_params() setup sink buffers for playback pipeline and source buffers
	 * for capture pipelines. So in case problem (1) is solved the loop is only needed if
	 * mixin is on capture pipeline and mixout is in playback pipeline. Such topology seems
	 * makes not much sense and probably never used. In all other cases comp_verify_params()
	 * will be sufficient to setup buffers and so the loop below may be removed.
	 */
	for (i = 0; i < mod->num_of_sinks; i++) {
		enum sof_ipc_frame frame_fmt, valid_fmt;

		sink_set_channels(mod->sinks[i], mod->priv.cfg.base_cfg.audio_fmt.channels_count);

		/* comp_verify_params() does not modify valid_sample_fmt (a BUG?),
		 * let's do this here
		 */
		audio_stream_fmt_conversion(mod->priv.cfg.base_cfg.audio_fmt.depth,
					    mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth,
					    &frame_fmt, &valid_fmt,
					    mod->priv.cfg.base_cfg.audio_fmt.s_type);

		sink_set_frm_fmt(mod->sinks[i], frame_fmt);
		sink_set_valid_fmt(mod->sinks[i], valid_fmt);
	}

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "comp_verify_params() failed!");
		return -EINVAL;
	}

	return 0;
}

/*
 * Prepare the mixer. The mixer may already be running at this point with other
 * sources. Make sure we only prepare the "prepared" source streams and not
 * the active or inactive sources.
 *
 * We should also make sure that we propagate the prepare call to downstream
 * if downstream is not currently active.
 */
static int mixin_prepare(struct processing_module *mod,
			 struct sof_source **sources, int num_of_sources,
			 struct sof_sink **sinks, int num_of_sinks)
{
	struct mixin_data *md = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	enum sof_ipc_frame fmt;
	int ret;

	comp_info(dev, "mixin_prepare()");

	ret = mixin_params(mod);
	if (ret < 0)
		return ret;

	fmt = sink_get_valid_fmt(sinks[0]);

	/* currently inactive so setup mixer */
	switch (fmt) {
	case SOF_IPC_FRAME_S16_LE:
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		mixin_get_processing_functions(fmt, &md->mix, &md->gain_mix);
		break;
	default:
		comp_err(dev, "unsupported data format %d", fmt);
		return -EINVAL;
	}

	if (!md->mix || !md->gain_mix) {
		comp_err(dev, "have not found suitable processing functions");
		return -EINVAL;
	}

	return 0;
}

static int mixout_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct comp_dev *dev = mod->dev;
	enum sof_ipc_frame frame_fmt, valid_fmt;
	int ret;

	comp_dbg(dev, "mixout_params()");

	ipc4_base_module_cfg_to_stream_params(&mod->priv.cfg.base_cfg, params);

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "comp_verify_params() failed!");
		return -EINVAL;
	}

	/* FIXME: the code below could/should be removed once mess with having both frame_fmt
	 * and valid_sample_fmt is solved (see comment in mixin_params()).
	 */

	/* comp_verify_params() does not modify valid_sample_fmt (a BUG?), let's do this here */
	audio_stream_fmt_conversion(mod->priv.cfg.base_cfg.audio_fmt.depth,
				    mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    mod->priv.cfg.base_cfg.audio_fmt.s_type);

	sink_set_valid_fmt(mod->sinks[0], valid_fmt);

	return 0;
}

static int mixout_prepare(struct processing_module *mod,
			  struct sof_source **sources, int num_of_sources,
			  struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_dev *dev = mod->dev;
	struct mixout_data *md;
	int ret, i;

	ret = mixout_params(mod);
	if (ret < 0)
		return ret;

	comp_dbg(dev, "mixout_prepare()");

	/*
	 * Since mixout sink buffer stream is reset on .prepare(), let's
	 * reset counters for not yet produced frames in that buffer.
	 */
	md = module_get_private_data(mod);
	md->mixed_frames = 0;

	for (i = 0; i < MIXOUT_MAX_SOURCES; i++)
		md->pending_frames[i].frames = 0;

	return 0;
}

static int mixout_bind(struct processing_module *mod, struct bind_info *bind_data)
{
	struct ipc4_module_bind_unbind *bu;
	struct comp_dev *mixin;
	struct pending_frames *pending_frames;
	int src_id;
	struct mixout_data *mixout_data;

	bu = bind_data->ipc4_data;
	src_id = IPC4_COMP_ID(bu->primary.r.module_id, bu->primary.r.instance_id);

	/* we are only interested in bind for mixin -> mixout pair */
	if (mod->dev->ipc_config.id == src_id)
		return 0;

	mixin = ipc4_get_comp_dev(src_id);
	if (!mixin) {
		comp_err(mod->dev, "mixout_bind: no mixin with ID %d found", src_id);
		return -EINVAL;
	}

	mixout_data = module_get_private_data(mod);

	pending_frames = get_mixin_pending_frames(mixout_data, mixin);
	/*
	 * this should never happen as pending_frames info for a particular mixin and mixout pair
	 * should have been already cleared in mixout_unbind()
	 */
	if (pending_frames) {
		pending_frames->mixin = NULL;
		pending_frames->frames = 0;
	}

	/* find an empty slot in the pending_frames array */
	pending_frames = get_mixin_pending_frames(mixout_data, NULL);
	if (!pending_frames) {
		/* no free slot in pending_frames array */
		comp_err(mod->dev, "Too many inputs!");
		return -ENOMEM;
	}

	pending_frames->frames = 0;
	pending_frames->mixin = mixin;

	return 0;
}

static int mixout_unbind(struct processing_module *mod, struct bind_info *unbind_data)
{
	struct ipc4_module_bind_unbind *bu;
	struct comp_dev *mixin;
	struct pending_frames *pending_frames;
	int src_id;
	struct mixout_data *mixout_data;

	comp_dbg(mod->dev, "mixout_unbind()");

	bu = unbind_data->ipc4_data;
	src_id = IPC4_COMP_ID(bu->primary.r.module_id, bu->primary.r.instance_id);

	/* we are only interested in unbind for mixin -> mixout pair */
	if (mod->dev->ipc_config.id == src_id)
		return 0;

	mixin = ipc4_get_comp_dev(src_id);
	if (!mixin) {
		comp_err(mod->dev, "mixout_bind: no mixin with ID %d found", src_id);
		return -EINVAL;
	}

	mixout_data = module_get_private_data(mod);

	/* remove mixin from pending_frames array */
	pending_frames = get_mixin_pending_frames(mixout_data, mixin);
	if (pending_frames) {
		pending_frames->mixin = NULL;
		pending_frames->frames = 0;
	}

	return 0;
}

static int mixin_set_config(struct processing_module *mod, uint32_t config_id,
			    enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			    const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			    size_t response_size)
{
	struct mixin_data *mixin_data = module_get_private_data(mod);
	const struct ipc4_mixer_mode_config *cfg;
	struct comp_dev *dev = mod->dev;
	int i;
	uint32_t sink_index;
	uint16_t gain;

	if (config_id != IPC4_MIXER_MODE) {
		comp_err(dev, "mixin_set_config() unsupported param ID: %u", config_id);
		return -EINVAL;
	}

	if (!(pos & MODULE_CFG_FRAGMENT_SINGLE)) {
		comp_err(dev, "mixin_set_config() data is expected to be sent as one chunk");
		return -EINVAL;
	}

	/* for a single chunk data, data_offset_size is size */
	if (data_offset_size < sizeof(struct ipc4_mixer_mode_config)) {
		comp_err(dev, "mixin_set_config() too small data size: %u", data_offset_size);
		return -EINVAL;
	}

	if (data_offset_size > SOF_IPC_MSG_MAX_SIZE) {
		comp_err(dev, "mixin_set_config() too large data size: %u", data_offset_size);
		return -EINVAL;
	}

	cfg = (const struct ipc4_mixer_mode_config *)fragment;

	if (cfg->mixer_mode_config_count < 1 || cfg->mixer_mode_config_count > MIXIN_MAX_SINKS) {
		comp_err(dev, "mixin_set_config() invalid mixer_mode_config_count: %u",
			 cfg->mixer_mode_config_count);
		return -EINVAL;
	}

	if (sizeof(struct ipc4_mixer_mode_config) +
	    (cfg->mixer_mode_config_count - 1) * sizeof(struct ipc4_mixer_mode_sink_config) >
	    data_offset_size) {
		comp_err(dev, "unexpected data size: %u", data_offset_size);
		return -EINVAL;
	}

	for (i = 0; i < cfg->mixer_mode_config_count; i++) {
		sink_index = cfg->mixer_mode_sink_configs[i].output_queue_id;
		if (sink_index >= MIXIN_MAX_SINKS) {
			comp_err(dev, "invalid sink index: %u", sink_index);
			return -EINVAL;
		}

		gain = cfg->mixer_mode_sink_configs[i].gain;
		if (gain > IPC4_MIXIN_UNITY_GAIN)
			gain = IPC4_MIXIN_UNITY_GAIN;
		mixin_data->sink_config[sink_index].gain = gain;

		comp_dbg(dev, "gain 0x%x will be applied for sink %u",
			 gain, sink_index);

		if (cfg->mixer_mode_sink_configs[i].mixer_mode ==
			IPC4_MIXER_CHANNEL_REMAPPING_MODE) {
			uint32_t channel_count =
				cfg->mixer_mode_sink_configs[i].output_channel_count;
			if (channel_count < 1 || channel_count > 8) {
				comp_err(dev, "Invalid output_channel_count %u for sink %u",
					 channel_count, sink_index);
				return -EINVAL;
			}

			mixin_data->sink_config[sink_index].output_channel_count = channel_count;
			mixin_data->sink_config[sink_index].output_channel_map =
				cfg->mixer_mode_sink_configs[i].output_channel_map;

			comp_dbg(dev, "output_channel_count: %u, chmap: 0x%x for sink: %u",
				 channel_count,
				 mixin_data->sink_config[sink_index].output_channel_map,
				 sink_index);
		}

		mixin_data->sink_config[sink_index].mixer_mode =
			cfg->mixer_mode_sink_configs[i].mixer_mode;
	}

	return 0;
}

#if CONFIG_XRUN_NOTIFICATIONS_ENABLE
static int mixin_set_config_param(struct processing_module *mod, uint32_t param_id_data)
{
	struct mixin_data *mixin_data = module_get_private_data(mod);
	union config_param_id_data cfg;

	cfg.dw = param_id_data;

	if (cfg.f.id == IPC4_MIXER_UNDERRUN_NOTIF_PERIOD) {
		if (cfg.f.data16 < MIXIN_MODULE_MIN_UNDERRUN_NOTIFICATION_PERIOD)
			return -EINVAL;

		mixin_data->underrun_notification_period = cfg.f.data16;
		return 0;
	}
	return -EINVAL;
}

static int mixin_get_config_param(struct processing_module *mod, uint32_t *param_id_data)
{
	struct mixin_data *mixin_data = module_get_private_data(mod);
	union config_param_id_data cfg;

	cfg.dw = *param_id_data;

	if (cfg.f.id == IPC4_MIXER_UNDERRUN_NOTIF_PERIOD) {
		cfg.f.data16 = mixin_data->underrun_notification_period;
		*param_id_data = cfg.dw;
		return 0;
	}
	return -EINVAL;
}
#endif

static const struct module_interface mixin_interface = {
	.init = mixin_init,
	.prepare = mixin_prepare,
	.process = mixin_process,
	.set_configuration = mixin_set_config,
#if CONFIG_XRUN_NOTIFICATIONS_ENABLE
	.set_config_param = mixin_set_config_param,
	.get_config_param = mixin_get_config_param,
#endif
	.reset = mixin_reset,
	.free = mixin_free
};

static const struct module_interface mixout_interface = {
	.init = mixout_init,
	.prepare = mixout_prepare,
	.process = mixout_process,
	.reset = mixout_reset,
	.free = mixout_free,
	.bind = mixout_bind,
	.unbind = mixout_unbind
};

#if CONFIG_COMP_MIXIN_MIXOUT_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest[] __section(".module") __used =
{
	SOF_LLEXT_MODULE_MANIFEST("MIXIN", &mixin_interface, 1, SOF_REG_UUID(mixin), 30),
	SOF_LLEXT_MODULE_MANIFEST("MIXOUT", &mixout_interface, 1, SOF_REG_UUID(mixout), 30),
};

SOF_LLEXT_BUILDINFO;

#else

DECLARE_MODULE_ADAPTER(mixin_interface, mixin_uuid, mixin_tr);
SOF_MODULE_INIT(mixin, sys_comp_module_mixin_interface_init);

DECLARE_MODULE_ADAPTER(mixout_interface, mixout_uuid, mixout_tr);
SOF_MODULE_INIT(mixout, sys_comp_module_mixout_interface_init);

#endif
