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
#include <sof/audio/mixer.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
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

LOG_MODULE_REGISTER(mixer, CONFIG_SOF_LOG_LEVEL);

/* bc06c037-12aa-417c-9a97-89282e321a76 */
DECLARE_SOF_RT_UUID("mixer", mixer_uuid, 0xbc06c037, 0x12aa, 0x417c,
		 0x9a, 0x97, 0x89, 0x28, 0x2e, 0x32, 0x1a, 0x76);

DECLARE_TR_CTX(mixer_tr, SOF_UUID(mixer_uuid), LOG_LEVEL_INFO);


static int mixer_init(struct processing_module *mod)
{
	struct module_data *mod_data = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct mixer_data *md;

	comp_dbg(dev, "mixer_init()");

	md = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*md));
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

	comp_dbg(dev, "mixer_free()");

	rfree(md);

	return 0;
}

/*
 * Mix N source PCM streams to one sink PCM stream. Frames copied is constant.
 */
static int mixer_process(struct processing_module *mod,
			 struct input_stream_buffer *input_buffers, int num_input_buffers,
			 struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct mixer_data *md = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	const struct audio_stream *sources_stream[PLATFORM_MAX_STREAMS];
	int sources_indices[PLATFORM_MAX_STREAMS];
	int32_t i = 0, j = 0;
	uint32_t frames = INT32_MAX;
	/* Redundant, but helps the compiler */
	uint32_t source_bytes = 0;
	uint32_t sink_bytes;
	int active_input_buffers = 0;

	comp_dbg(dev, "mixer_process() %d", num_input_buffers);

	/* too many sources ? */
	if (num_input_buffers >= PLATFORM_MAX_STREAMS)
		return -EINVAL;

	/* check for underruns */
	for (i = 0; i < num_input_buffers; i++) {
		uint32_t avail_frames;

		avail_frames = audio_stream_avail_frames_aligned(mod->input_buffers[i].data,
								 mod->output_buffers[0].data);

		/* if one source is inactive, skip it */
		if (avail_frames == 0)
			continue;

		active_input_buffers++;
		frames = MIN(frames, avail_frames);
	}

	if (!active_input_buffers) {
		/*
		 * Generate silence when sources are inactive. When
		 * sources change to active, additionally keep
		 * generating silence until at least one of the
		 * sources start to have data available (frames!=0).
		 */
		sink_bytes = dev->frames * audio_stream_frame_bytes(mod->output_buffers[0].data);
		if (!audio_stream_set_zero(mod->output_buffers[0].data, sink_bytes))
			mod->output_buffers[0].size = sink_bytes;

		return 0;
	}

	/* Every source has the same format, so calculate bytes based on the first one */
	source_bytes = frames * audio_stream_frame_bytes(mod->input_buffers[0].data);

	sink_bytes = frames * audio_stream_frame_bytes(mod->output_buffers[0].data);

	comp_dbg(dev, "mixer_process(), source_bytes = 0x%x, sink_bytes = 0x%x",
		 source_bytes, sink_bytes);

	/* mix streams */
	for (i = 0; i < num_input_buffers; i++) {
		uint32_t avail_frames;

		avail_frames = audio_stream_avail_frames_aligned(mod->input_buffers[i].data,
								 mod->output_buffers[0].data);

		/* if one source is inactive, skip it */
		if (avail_frames == 0)
			continue;

		sources_indices[j] = i;
		sources_stream[j++] = mod->input_buffers[i].data;
	}

	if (j)
		md->mix_func(dev, mod->output_buffers[0].data, sources_stream, j, frames);
	mod->output_buffers[0].size = sink_bytes;

	/* update source buffer consumed bytes */
	for (i = 0; i < j; i++)
		mod->input_buffers[sources_indices[i]].consumed = source_bytes;

	return 0;
}

static int mixer_reset(struct processing_module *mod)
{
	struct mixer_data *md = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct list_item *blist;
	int dir = dev->pipeline->source_comp->direction;

	comp_dbg(dev, "mixer_reset()");

	if (dir == SOF_IPC_STREAM_PLAYBACK) {
		list_for_item(blist, &dev->bsource_list) {
			/* FIXME: this is racy and implicitly protected by serialised IPCs */
			struct comp_buffer *source = container_of(blist, struct comp_buffer,
								  sink_list);
			bool stop = false;

			if (source->source && source->source->state > COMP_STATE_READY)
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
static inline void mixer_set_frame_alignment(struct audio_stream *source)
{
#if XCHAL_HAVE_HIFI3 || XCHAL_HAVE_HIFI4

	/* Xtensa intrinsics ask for 8-byte aligned. 5.1 format SSE audio
	 * requires 16-byte aligned.
	 */
	const uint32_t byte_align = audio_stream_get_channels(source) == 6 ? 16 : 8;

	/*There is no limit for frame number, so set it as 1*/
	const uint32_t frame_align_req = 1;

#else

	/* Since the generic version process signal sample by sample, so there is no
	 * limit for it, then set the byte_align and frame_align_req to be 1.
	 */
	const uint32_t byte_align = 1;
	const uint32_t frame_align_req = 1;

#endif

	audio_stream_init_alignment_constants(byte_align, frame_align_req, source);
}

static int mixer_prepare(struct processing_module *mod,
			 struct sof_source **sources, int num_of_sources,
			 struct sof_sink **sinks, int num_of_sinks)
{
	struct mixer_data *md = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sink;
	struct list_item *blist;

	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);
	md->mix_func = mixer_get_processing_function(dev, sink);
	mixer_set_frame_alignment(&sink->stream);

	/* check each mixer source state */
	list_for_item(blist, &dev->bsource_list) {
		struct comp_buffer *source;
		bool stop;

		/*
		 * FIXME: this is intrinsically racy. One of mixer sources can
		 * run on a different core and can enter PAUSED or ACTIVE right
		 * after we have checked it here. We should set a flag or a
		 * status to inform any other connected pipelines that we're
		 * preparing the mixer, so they shouldn't touch it until we're
		 * done.
		 */
		source = container_of(blist, struct comp_buffer, sink_list);
		mixer_set_frame_alignment(&source->stream);
		stop = source->source && (source->source->state == COMP_STATE_PAUSED ||
					    source->source->state == COMP_STATE_ACTIVE);

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
	.process_audio_stream = mixer_process,
	.reset = mixer_reset,
	.free = mixer_free,
};

DECLARE_MODULE_ADAPTER(mixer_interface, mixer_uuid, mixer_tr);
SOF_MODULE_INIT(mixer, sys_comp_module_mixer_interface_init);
