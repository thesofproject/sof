// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>

#if CONFIG_COMP_MUX

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/sink_source_utils.h>
#include <rtos/bit.h>
#include <sof/common.h>
#include <ipc/stream.h>
#include <stddef.h>
#include <stdint.h>

#include "mux.h"

LOG_MODULE_DECLARE(muxdemux, CONFIG_SOF_LOG_LEVEL);

static void mux_check_for_wrap(struct cir_buf_sink *sink,
			       struct cir_buf_source *source_bufs,
			       struct mux_look_up *lookup)
{
	struct cir_buf_source *source;
	uint32_t elem;

	/* check sources and destinations for wrap */
	for (elem = 0; elem < lookup->num_elems; elem++) {
		source = &source_bufs[lookup->copy_elem[elem].stream_id];
		lookup->copy_elem[elem].dest =
			cir_buf_wrap(lookup->copy_elem[elem].dest,
				     sink->buf_start, sink->buf_end);
		lookup->copy_elem[elem].src =
			cir_buf_wrap(lookup->copy_elem[elem].src,
				     (void *)source->buf_start, (void *)source->buf_end);
	}
}

#if CONFIG_FORMAT_S16LE

static uint32_t mux_calc_frames_without_wrap_s16(struct cir_buf_sink *sink,
						 struct cir_buf_source *source_bufs,
						 struct mux_look_up *lookup)
{
	struct cir_buf_source *source;
	uint32_t frames;
	uint32_t min_frames;
	uint32_t elem;
	void *ptr;

	/* dest pointer for all copy_elems in lookup refers to the same
	 * sink buffer (mux has one sink buffer), so dest min_frames
	 * calculation based only on lookup table first element is sufficient.
	 */
	ptr = (int16_t *)lookup->copy_elem[0].dest -
		lookup->copy_elem[0].out_ch;
	min_frames = circ_buf_frames_without_wrap(ptr, sink->buf_end, sizeof(int16_t),
						  lookup->copy_elem[0].dest_inc);

	for (elem = 0; elem < lookup->num_elems; elem++) {
		source = &source_bufs[lookup->copy_elem[elem].stream_id];

		ptr = (int16_t *)lookup->copy_elem[elem].src -
			lookup->copy_elem[elem].in_ch;
		frames = circ_buf_frames_without_wrap(ptr, source->buf_end, sizeof(int16_t),
						      lookup->copy_elem[elem].src_inc);

		min_frames = (frames < min_frames) ? frames : min_frames;
	}

	return min_frames;
}

static void mux_init_look_up_pointers_s16(struct sof_sink *sink,
					  struct cir_buf_sink *sink_buf,
					  struct sof_source **sources,
					  struct cir_buf_source *source_bufs,
					  struct mux_look_up *lookup)
{
	uint32_t elem;
	uint32_t sid;

	/* init pointers */
	for (elem = 0; elem < lookup->num_elems; elem++) {
		sid = lookup->copy_elem[elem].stream_id;

		lookup->copy_elem[elem].src = (int16_t *)source_bufs[sid].ptr +
			lookup->copy_elem[elem].in_ch;
		lookup->copy_elem[elem].src_inc = source_get_channels(sources[sid]);

		lookup->copy_elem[elem].dest = (int16_t *)sink_buf->ptr +
			lookup->copy_elem[elem].out_ch;
		lookup->copy_elem[elem].dest_inc = sink_get_channels(sink);
	}
}

static int demux_s16le(struct comp_dev *dev, struct sof_sink *sink,
			struct sof_source *source, const void *source_data,
			const void *source_start, size_t source_size,
			uint32_t frames, struct mux_look_up *lookup)
{
	const int16_t *x_start = source_start;
	const int16_t *x_end = x_start + (source_size >> 1);
	int16_t *y, *y_start, *y_end;
	int y_size;
	int source_channels = source_get_channels(source);
	int sink_channels = sink_get_channels(sink);
	int bytes = frames * sink_get_frame_bytes(sink);
	uint32_t elem;
	uint32_t i;
	int ret;

	comp_dbg(dev, "entry");

	if (!lookup || !lookup->num_elems)
		return 0;

	/* obtain the sink circular buffer for this output stream */
	ret = sink_get_buffer_s16(sink, bytes, &y, &y_start, &y_size);
	if (ret)
		return ret;

	y_end = y_start + y_size;

	/* init pointers based on the freshly obtained buffers */
	for (elem = 0; elem < lookup->num_elems; elem++) {
		lookup->copy_elem[elem].src = (int16_t *)source_data +
			lookup->copy_elem[elem].in_ch;
		lookup->copy_elem[elem].src_inc = source_channels;

		lookup->copy_elem[elem].dest = y + lookup->copy_elem[elem].out_ch;
		lookup->copy_elem[elem].dest_inc = sink_channels;
	}

	while (frames) {
		int16_t *src = (int16_t *)lookup->copy_elem[0].src -
			lookup->copy_elem[0].in_ch;
		int16_t *dst = (int16_t *)lookup->copy_elem[0].dest -
			lookup->copy_elem[0].out_ch;
		uint32_t source_frames_without_wrap =
			circ_buf_frames_without_wrap(src, x_end, sizeof(*src), source_channels);
		uint32_t sink_frames_without_wrap =
			circ_buf_frames_without_wrap(dst, y_end, sizeof(*dst), sink_channels);
		uint32_t frames_without_wrap;

		frames_without_wrap = MIN(source_frames_without_wrap,
					  sink_frames_without_wrap);
		frames_without_wrap = MIN(frames, frames_without_wrap);

		for (i = 0; i < frames_without_wrap; i++) {
			for (elem = 0; elem < lookup->num_elems; elem++) {
				src = (int16_t *)lookup->copy_elem[elem].src;
				dst = (int16_t *)lookup->copy_elem[elem].dest;
				*dst = *src;
				lookup->copy_elem[elem].src = src +
					lookup->copy_elem[elem].src_inc;
				lookup->copy_elem[elem].dest = dst +
					lookup->copy_elem[elem].dest_inc;
			}
		}

		/* check sources and destinations for wrap */
		for (elem = 0; elem < lookup->num_elems; elem++) {
			src = (int16_t *)lookup->copy_elem[elem].src;
			dst = (int16_t *)lookup->copy_elem[elem].dest;
			if (src >= x_end)
				src -= (source_size >> 1);
			if (dst >= y_end)
				dst -= y_size;
			lookup->copy_elem[elem].src = src;
			lookup->copy_elem[elem].dest = dst;
		}

		frames -= frames_without_wrap;
	}

	sink_commit_buffer(sink, bytes);

	return 0;
}

/**
 * Source streams are routed to sink with regard to look up table based on
 * routing bitmasks from mux_stream_data structures array. Each sink channel
 * has it's own lookup[].copy_elem describing source and sink fragment of
 * memory featured in copying.
 *
 * @param[in] dev Component device
 * @param[in,out] sink Destination buffer.
 * @param[in,out] sources Array of source buffers.
 * @param[in] frames Number of frames to process.
 * @param[in] lookup mux look up table.
 */
static void mux_s16le(struct comp_dev *dev, struct sof_sink *sink,
		      struct cir_buf_sink *sink_buf, struct sof_source **sources,
		      struct cir_buf_source *source_bufs, uint32_t frames,
		      struct mux_look_up *lookup)
{
	uint32_t i;
	int16_t *src;
	int16_t *dst;
	uint32_t elem;
	uint32_t frames_without_wrap;

	comp_dbg(dev, "entry");

	if (!lookup || !lookup->num_elems)
		return;

	mux_init_look_up_pointers_s16(sink, sink_buf, sources, source_bufs, lookup);

	while (frames) {
		frames_without_wrap =
			mux_calc_frames_without_wrap_s16(sink_buf, source_bufs, lookup);

		frames_without_wrap = MIN(frames, frames_without_wrap);

		for (i = 0; i < frames_without_wrap; i++) {
			for (elem = 0; elem < lookup->num_elems; elem++) {
				src = (int16_t *)lookup->copy_elem[elem].src;
				dst = (int16_t *)lookup->copy_elem[elem].dest;
				*dst = *src;
				lookup->copy_elem[elem].src = src +
					lookup->copy_elem[elem].src_inc;
				lookup->copy_elem[elem].dest = dst +
					lookup->copy_elem[elem].dest_inc;
			}
		}

		mux_check_for_wrap(sink_buf, source_bufs, lookup);

		frames -= frames_without_wrap;
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE

static uint32_t mux_calc_frames_without_wrap_s32(struct cir_buf_sink *sink,
						 struct cir_buf_source *source_bufs,
						 struct mux_look_up *lookup)
{
	struct cir_buf_source *source;
	uint32_t frames;
	uint32_t min_frames;
	uint32_t elem;
	void *ptr;

	/* dest pointer for all copy_elems in lookup refers to the same
	 * sink buffer (mux has one sink buffer), so dest min_frames
	 * calculation based only on lookup table first element is sufficient.
	 */
	ptr = (int32_t *)lookup->copy_elem[0].dest - lookup->copy_elem[0].out_ch;
	min_frames = circ_buf_frames_without_wrap(ptr, sink->buf_end, sizeof(int32_t),
						  lookup->copy_elem[0].dest_inc);

	for (elem = 0; elem < lookup->num_elems; elem++) {
		source = &source_bufs[lookup->copy_elem[elem].stream_id];

		ptr = (int32_t *)lookup->copy_elem[elem].src - lookup->copy_elem[elem].in_ch;
		frames = circ_buf_frames_without_wrap(ptr, source->buf_end, sizeof(int32_t),
						      lookup->copy_elem[elem].src_inc);

		min_frames = (frames < min_frames) ? frames : min_frames;
	}

	return min_frames;
}

static void mux_init_look_up_pointers_s32(struct sof_sink *sink,
					  struct cir_buf_sink *sink_buf,
					  struct sof_source **sources,
					  struct cir_buf_source *source_bufs,
					  struct mux_look_up *lookup)
{
	uint32_t elem;
	uint32_t sid;

	/* init pointers */
	for (elem = 0; elem < lookup->num_elems; elem++) {
		sid = lookup->copy_elem[elem].stream_id;

		lookup->copy_elem[elem].src = (int32_t *)source_bufs[sid].ptr +
			lookup->copy_elem[elem].in_ch;
		lookup->copy_elem[elem].src_inc = source_get_channels(sources[sid]);

		lookup->copy_elem[elem].dest = (int32_t *)sink_buf->ptr +
			lookup->copy_elem[elem].out_ch;
		lookup->copy_elem[elem].dest_inc = sink_get_channels(sink);
	}
}

/**
 * Source stream is routed to sinks with regard to look up table based on
 * routing bitmasks from mux_stream_data structures array. Each sink channel
 * has it's own lookup[].copy_elem describing source and sink fragment of
 * memory featured in copying.
 *
 * @param[in] dev Component device
 * @param[in,out] sink Destination sink (sof_sink handle).
 * @param[in] source Source handle, used for channel count metadata only.
 * @param[in] source_data Read pointer into the source circular buffer.
 * @param[in] source_start Start address of the source circular buffer.
 * @param[in] source_size Size of the source circular buffer in bytes.
 * @param[in] frames Number of frames to process.
 * @param[in] lookup mux look up table.
 * @return 0 on success, negative error code otherwise.
 */
static int demux_s32le(struct comp_dev *dev, struct sof_sink *sink,
			struct sof_source *source, const void *source_data,
			const void *source_start, size_t source_size,
			uint32_t frames, struct mux_look_up *lookup)
{
	const int32_t *x_start = source_start;
	const int32_t *x_end = x_start + (source_size >> 2);
	int32_t *y, *y_start, *y_end;
	int y_size;
	int source_channels = source_get_channels(source);
	int sink_channels = sink_get_channels(sink);
	int bytes = frames * sink_get_frame_bytes(sink);
	uint32_t elem;
	uint32_t i;
	int ret;

	comp_dbg(dev, "entry");

	if (!lookup || !lookup->num_elems)
		return 0;

	/* obtain the sink circular buffer for this output stream */
	ret = sink_get_buffer_s32(sink, bytes, &y, &y_start, &y_size);
	if (ret)
		return ret;

	y_end = y_start + y_size;

	/* init pointers based on the freshly obtained buffers */
	for (elem = 0; elem < lookup->num_elems; elem++) {
		lookup->copy_elem[elem].src = (int32_t *)source_data +
			lookup->copy_elem[elem].in_ch;
		lookup->copy_elem[elem].src_inc = source_channels;

		lookup->copy_elem[elem].dest = y + lookup->copy_elem[elem].out_ch;
		lookup->copy_elem[elem].dest_inc = sink_channels;
	}

	while (frames) {
		int32_t *src = (int32_t *)lookup->copy_elem[0].src -
			lookup->copy_elem[0].in_ch;
		int32_t *dst = (int32_t *)lookup->copy_elem[0].dest -
			lookup->copy_elem[0].out_ch;
		uint32_t source_frames_without_wrap =
			circ_buf_frames_without_wrap(src, x_end, sizeof(*src), source_channels);
		uint32_t sink_frames_without_wrap =
			circ_buf_frames_without_wrap(dst, y_end, sizeof(*dst), sink_channels);
		uint32_t frames_without_wrap;

		frames_without_wrap = MIN(source_frames_without_wrap,
					  sink_frames_without_wrap);
		frames_without_wrap = MIN(frames, frames_without_wrap);

		for (i = 0; i < frames_without_wrap; i++) {
			for (elem = 0; elem < lookup->num_elems; elem++) {
				src = (int32_t *)lookup->copy_elem[elem].src;
				dst = (int32_t *)lookup->copy_elem[elem].dest;
				*dst = *src;
				lookup->copy_elem[elem].src = src +
					lookup->copy_elem[elem].src_inc;
				lookup->copy_elem[elem].dest = dst +
					lookup->copy_elem[elem].dest_inc;
			}
		}

		/* check sources and destinations for wrap */
		for (elem = 0; elem < lookup->num_elems; elem++) {
			src = (int32_t *)lookup->copy_elem[elem].src;
			dst = (int32_t *)lookup->copy_elem[elem].dest;
			if (src >= x_end)
				src -= (source_size >> 2);
			if (dst >= y_end)
				dst -= y_size;
			lookup->copy_elem[elem].src = src;
			lookup->copy_elem[elem].dest = dst;
		}

		frames -= frames_without_wrap;
	}

	sink_commit_buffer(sink, bytes);

	return 0;
}

/**
 * Source streams are routed to sink with regard to look up table based on
 * routing bitmasks from mux_stream_data structures array. Each sink channel
 * has it's own lookup[].copy_elem describing source and sink fragment of
 * memory featured in copying.
 *
 * @param[in] dev Component device
 * @param[in,out] sink Destination buffer.
 * @param[in,out] sources Array of source buffers.
 * @param[in] frames Number of frames to process.
 * @param[in] lookup mux look up table.
 */
static void mux_s32le(struct comp_dev *dev, struct sof_sink *sink,
		      struct cir_buf_sink *sink_buf, struct sof_source **sources,
		      struct cir_buf_source *source_bufs, uint32_t frames,
		      struct mux_look_up *lookup)
{
	uint32_t i;
	int32_t *src;
	int32_t *dst;
	uint32_t elem;
	uint32_t frames_without_wrap;

	comp_dbg(dev, "entry");

	if (!lookup || !lookup->num_elems)
		return;

	mux_init_look_up_pointers_s32(sink, sink_buf, sources, source_bufs, lookup);

	while (frames) {
		frames_without_wrap =
			mux_calc_frames_without_wrap_s32(sink_buf, source_bufs, lookup);

		frames_without_wrap = MIN(frames, frames_without_wrap);

		for (i = 0; i < frames_without_wrap; i++) {
			for (elem = 0; elem < lookup->num_elems; elem++) {
				src = (int32_t *)lookup->copy_elem[elem].src;
				dst = (int32_t *)lookup->copy_elem[elem].dest;
				*dst = *src;
				lookup->copy_elem[elem].src = src +
					lookup->copy_elem[elem].src_inc;
				lookup->copy_elem[elem].dest = dst +
					lookup->copy_elem[elem].dest_inc;
			}
		}

		mux_check_for_wrap(sink_buf, source_bufs, lookup);

		frames -= frames_without_wrap;
	}
}

#endif /* CONFIG_FORMAT_S24LE CONFIG_FORMAT_S32LE */

const struct comp_func_map mux_func_map[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, &mux_s16le, &demux_s16le },
#endif
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, &mux_s32le, &demux_s32le },
#endif
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, &mux_s32le, &demux_s32le },
#endif
};

void mux_prepare_look_up_table(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	uint32_t i;
	uint32_t j;
	uint32_t k;
	uint32_t idx = 0;

	/* Prepare look up table */
	for (i = 0; i < cd->config.num_streams; i++) {
		for (j = 0; j < PLATFORM_MAX_CHANNELS; j++) {
			for (k = 0; k < PLATFORM_MAX_CHANNELS; k++) {
				if (cd->config.streams[i].mask[j] & BIT(k)) {
					/* MUX component has only one sink */
					cd->lookup[0].copy_elem[idx].in_ch = j;
					cd->lookup[0].copy_elem[idx].out_ch = k;
					cd->lookup[0].copy_elem[idx].stream_id = i;
					cd->lookup[0].num_elems = ++idx;
				}
			}
		}
	}
}

void demux_prepare_look_up_table(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	uint32_t i;
	uint32_t j;
	uint32_t k;
	uint32_t idx;

	/* Prepare look up table */
	for (i = 0; i < cd->config.num_streams; i++) {
		idx = 0;
		for (j = 0; j < PLATFORM_MAX_CHANNELS; j++) {
			for (k = 0; k < PLATFORM_MAX_CHANNELS; k++) {
				if (cd->config.streams[i].mask[j] & BIT(k)) {
					/* DEMUX component has only one source */
					cd->lookup[i].copy_elem[idx].in_ch = k;
					cd->lookup[i].copy_elem[idx].out_ch = j;
					cd->lookup[i].copy_elem[idx].stream_id = i;
					cd->lookup[i].num_elems = ++idx;
				}
			}
		}
	}
}

mux_func mux_get_processing_function(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sinkb;
	uint32_t i;

	if (list_is_empty(&dev->bsink_list))
		return NULL;

	sinkb = comp_dev_get_first_data_consumer(dev);

	for (i = 0; i < ARRAY_SIZE(mux_func_map); i++) {
		enum sof_ipc_frame fmt = audio_stream_get_frm_fmt(&sinkb->stream);


		if (fmt == mux_func_map[i].frame_format)
			return mux_func_map[i].mux_proc_func;
	}

	return NULL;
}

demux_func demux_get_processing_function(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sourceb;
	uint32_t i;

	if (list_is_empty(&dev->bsource_list))
		return NULL;

	sourceb = comp_dev_get_first_data_producer(dev);

	for (i = 0; i < ARRAY_SIZE(mux_func_map); i++) {
		enum sof_ipc_frame fmt = audio_stream_get_frm_fmt(&sourceb->stream);

		if (fmt == mux_func_map[i].frame_format)
			return mux_func_map[i].demux_proc_func;
	}

	return NULL;
}

#endif /* CONFIG_COMP_MUX */
