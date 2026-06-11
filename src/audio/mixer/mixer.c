// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
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
#include <user/trace.h>
#include <stddef.h>
#include <stdint.h>

#include "mixer.h"

LOG_MODULE_REGISTER(mixer, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(mixer);

static int mixer_init(struct processing_module *mod)
{
	struct module_data *mod_data = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct mixer_data *md;

	comp_dbg(dev, "entry");

	md = mod_zalloc(mod, sizeof(*md));
	if (!md)
		return -ENOMEM;

	mod_data->private = md;
	mod->verify_params_flags = BUFF_PARAMS_CHANNELS;
	mod->no_pause = true;
	mod->max_sources = MIXER_MAX_SOURCES;

	return 0;
}

static int mixer_free(struct processing_module *mod)
{
	struct mixer_data *md = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "entry");

	mod_free(mod, md);

	return 0;
}

/*
 * Mix N source PCM streams to one sink PCM stream. Frames copied is constant.
 */
static int mixer_process(struct processing_module *mod,
			 struct sof_source **sources, int num_of_sources,
			 struct sof_sink **sinks, int num_of_sinks)
{
	struct mixer_data *md = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct sof_source *active_sources[PLATFORM_MAX_STREAMS];
	struct cir_buf_source source_bufs[PLATFORM_MAX_STREAMS];
	struct cir_buf_sink sink_buf;
	size_t bytes, samples, source_bytes, sink_bytes;
	size_t frames = SIZE_MAX;
	int active_input_buffers = 0;
	int i, j, ret;

	comp_dbg(dev, "%d", num_of_sources);

	/* too many sources ? */
	if (num_of_sources >= PLATFORM_MAX_STREAMS)
		return -EINVAL;

	/* find active sources and compute frame count */
	for (i = 0; i < num_of_sources; i++) {
		size_t avail_frames = source_sink_avail_frames_aligned(sources[i], sinks[0]);

		/* if one source is inactive, skip it */
		if (avail_frames == 0)
			continue;

		frames = MIN(frames, avail_frames);
		active_sources[active_input_buffers] = sources[i];
		active_input_buffers++;
	}

	if (!active_input_buffers) {
		/*
		 * Generate silence when sources are inactive. When
		 * sources change to active, additionally keep
		 * generating silence until at least one of the
		 * sources start to have data available (frames!=0).
		 */
		return sink_fill_with_silence(sinks[0],
					      dev->frames * sink_get_frame_bytes(sinks[0]));
	}

	comp_dbg(dev, "frames = %zu", frames);

	sink_bytes = frames * sink_get_frame_bytes(sinks[0]);
	samples = frames * sink_get_channels(sinks[0]);

	/* acquire the sink buffer */
	ret = sink_get_buffer(sinks[0], sink_bytes, &sink_buf.ptr, &sink_buf.buf_start, &bytes);
	if (ret < 0)
		return ret;
	sink_buf.buf_end = (char *)sink_buf.buf_start + bytes;

	/* Every source has the same format, so calculate bytes based on the first one */
	source_bytes = frames * source_get_frame_bytes(active_sources[0]);

	/* acquire all active source buffers */
	for (i = 0; i < active_input_buffers; i++) {
		ret = source_get_data(active_sources[i], source_bytes, &source_bufs[i].ptr,
				      &source_bufs[i].buf_start, &bytes);
		if (ret < 0) {
			for (j = 0; j < i; j++)
				source_release_data(active_sources[j], 0);
			sink_commit_buffer(sinks[0], 0);
			return ret;
		}
		source_bufs[i].buf_end = (const char *)source_bufs[i].buf_start + bytes;
	}

	md->mix_func(&sink_buf, source_bufs, active_input_buffers, samples);

	/* commit the consumed and produced data */
	for (i = 0; i < active_input_buffers; i++)
		source_release_data(active_sources[i], source_bytes);
	sink_commit_buffer(sinks[0], sink_bytes);

	return 0;
}

static int mixer_reset(struct processing_module *mod)
{
	struct mixer_data *md = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int dir = dev->pipeline->source_comp->direction;

	comp_dbg(dev, "entry");

	if (dir == SOF_IPC_STREAM_PLAYBACK) {
		int i;

		for (i = 0; i < mod->num_of_sources; i++) {
			/* FIXME: this is racy and implicitly protected by serialised IPCs */
			bool stop = false;

			if (source_get_comp_state(mod->sources[i]) > COMP_STATE_READY)
				stop = true;

			/* only mix the sources with the same state with mixer */
			if (stop)
				/* should not reset the downstream components */
				return PPL_STATUS_PATH_STOP;
		}
	}

	md->mix_func = NULL;

	return 0;
}

/* init and calculate the aligned setting for available frames and free frames retrieve*/
#if XCHAL_HAVE_HIFI3 || XCHAL_HAVE_HIFI4
static inline uint32_t mixer_get_byte_align(uint32_t channels)
{
	/* Xtensa intrinsics ask for 8-byte aligned. 5.1 format SSE audio
	 * requires 16-byte aligned.
	 */
	return channels == 6 ? 16 : 8;
}

static void mixer_set_source_frame_alignment(struct sof_source *src)
{
	const uint32_t byte_align = mixer_get_byte_align(source_get_channels(src));

	/* There is no limit for frame number, so set it as 1 */
	const uint32_t frame_align_req = 1;

	source_set_alignment_constants(src, byte_align, frame_align_req);
}

static void mixer_set_sink_frame_alignment(struct sof_sink *snk)
{
	const uint32_t byte_align = mixer_get_byte_align(sink_get_channels(snk));

	/* There is no limit for frame number, so set it as 1 */
	const uint32_t frame_align_req = 1;

	sink_set_alignment_constants(snk, byte_align, frame_align_req);
}
#endif

static int mixer_prepare(struct processing_module *mod,
			 struct sof_source **sources, int num_of_sources,
			 struct sof_sink **sinks, int num_of_sinks)
{
	struct mixer_data *md = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	int i;

	if (!num_of_sinks) {
		comp_err(dev, "no sink");
		return -ENOTCONN;
	}

#if XCHAL_HAVE_HIFI3 || XCHAL_HAVE_HIFI4
	mixer_set_sink_frame_alignment(sinks[0]);
	for (i = 0; i < num_of_sources; i++)
		mixer_set_source_frame_alignment(sources[i]);
#endif

	md->mix_func = mixer_get_processing_function(dev, sink_get_frm_fmt(sinks[0]));

	/* check each mixer source state */
	for (i = 0; i < num_of_sources; i++) {
		int state = source_get_comp_state(sources[i]);
		bool stop;

		/*
		 * FIXME: this is intrinsically racy. One of mixer sources can
		 * run on a different core and can enter PAUSED or ACTIVE right
		 * after we have checked it here. We should set a flag or a
		 * status to inform any other connected pipelines that we're
		 * preparing the mixer, so they shouldn't touch it until we're
		 * done.
		 */
		stop = state == COMP_STATE_PAUSED || state == COMP_STATE_ACTIVE;

		/* only prepare downstream if we have no active sources */
		if (stop)
			return PPL_STATUS_PATH_STOP;
	}

	/* prepare downstream */
	return 0;
}

static const struct module_interface mixer_interface = {
	.init = mixer_init,
	.prepare = mixer_prepare,
	.process = mixer_process,
	.reset = mixer_reset,
	.free = mixer_free,
};

DECLARE_TR_CTX(mixer_tr, SOF_UUID(mixer_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(mixer_interface, mixer_uuid, mixer_tr);
SOF_MODULE_INIT(mixer, sys_comp_module_mixer_interface_init);
