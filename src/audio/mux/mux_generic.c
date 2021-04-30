// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>

#if CONFIG_COMP_MUX

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/mux.h>
#include <sof/bit.h>
#include <sof/common.h>
#include <ipc/stream.h>
#include <stddef.h>
#include <stdint.h>

static void mux_check_for_wrap(struct audio_stream *sink,
			       const struct audio_stream **sources,
			       struct mux_look_up *lookup)
{
	const struct audio_stream *source;
	uint32_t elem;

	/* check sources and destinations for wrap */
	for (elem = 0; elem < lookup->num_elems; elem++) {
		source = sources[lookup->copy_elem[elem].stream_id];
		lookup->copy_elem[elem].dest =
			audio_stream_wrap(sink, lookup->copy_elem[elem].dest);
		lookup->copy_elem[elem].src =
			audio_stream_wrap(source, lookup->copy_elem[elem].src);
	}
}

static void demux_check_for_wrap(struct audio_stream *sink,
				 const struct audio_stream *source,
				 struct mux_look_up *lookup)
{
	uint32_t elem;

	/* check sources and destinations for wrap */
	for (elem = 0; elem < lookup->num_elems; elem++) {
		lookup->copy_elem[elem].dest =
			audio_stream_wrap(sink, lookup->copy_elem[elem].dest);
		lookup->copy_elem[elem].src =
			audio_stream_wrap(source, lookup->copy_elem[elem].src);
	}
}

#if CONFIG_FORMAT_S16LE

static uint32_t demux_calc_frames_without_wrap_s16(struct comp_dev *dev,
						   struct audio_stream *sink,
						   const struct audio_stream
						   *source,
						   struct mux_look_up *lookup)
{
	uint32_t frames;
	uint32_t min_frames;
	void *ptr;

	/* for demux we process each source buffer separately - dest/src for
	 * each copy_elem refers to the same sink/source buffer, so min_frames
	 * calculation based only on lookup table first element is sufficient.
	 */
	ptr = (int16_t *)lookup->copy_elem[0].dest -
		lookup->copy_elem[0].out_ch;
	min_frames = audio_stream_frames_without_wrap(sink, ptr);

	ptr = (int16_t *)lookup->copy_elem[0].src -
		lookup->copy_elem[0].in_ch;
	frames = audio_stream_frames_without_wrap(source, ptr);

	min_frames = (frames < min_frames) ? frames : min_frames;

	return min_frames;
}

static uint32_t mux_calc_frames_without_wrap_s16(struct comp_dev *dev,
						 struct audio_stream *sink,
						 const struct audio_stream
						 **sources,
						 struct mux_look_up *lookup)
{
	const struct audio_stream *source;
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
	min_frames = audio_stream_frames_without_wrap(sink, ptr);

	for (elem = 0; elem < lookup->num_elems; elem++) {
		source = sources[lookup->copy_elem[elem].stream_id];

		ptr = (int16_t *)lookup->copy_elem[elem].src -
			lookup->copy_elem[elem].in_ch;
		frames = audio_stream_frames_without_wrap(source, ptr);

		min_frames = (frames < min_frames) ? frames : min_frames;
	}

	return min_frames;
}

static void mux_init_look_up_pointers_s16(struct comp_dev *dev,
					  struct audio_stream *sink,
					  const struct audio_stream **sources,
					  struct mux_look_up *lookup)
{
	const struct audio_stream *source;
	uint32_t elem;

	/* init pointers */
	for (elem = 0; elem < lookup->num_elems; elem++) {
		source = sources[lookup->copy_elem[elem].stream_id];

		lookup->copy_elem[elem].src = (int16_t *)source->r_ptr +
			lookup->copy_elem[elem].in_ch;
		lookup->copy_elem[elem].src_inc = source->channels;

		lookup->copy_elem[elem].dest = (int16_t *)sink->w_ptr +
			lookup->copy_elem[elem].out_ch;
		lookup->copy_elem[elem].dest_inc = sink->channels;
	}
}

static void demux_init_look_up_pointers_s16(struct comp_dev *dev,
					    struct audio_stream *sink,
					    const struct audio_stream *source,
					    struct mux_look_up *lookup)
{
	uint32_t elem;

	/* init pointers */
	for (elem = 0; elem < lookup->num_elems; elem++) {
		lookup->copy_elem[elem].src = (int16_t *)source->r_ptr +
			lookup->copy_elem[elem].in_ch;
		lookup->copy_elem[elem].src_inc = source->channels;

		lookup->copy_elem[elem].dest = (int16_t *)sink->w_ptr +
			lookup->copy_elem[elem].out_ch;
		lookup->copy_elem[elem].dest_inc = sink->channels;
	}
}

/**
 * Source stream are routed to sinks with regard to look up table based on
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
static void demux_s16le(struct comp_dev *dev, struct audio_stream *sink,
			const struct audio_stream *source, uint32_t frames,
			struct mux_look_up *lookup)
{
	uint8_t i;
	int16_t *src;
	int16_t *dst;
	uint32_t elem;
	uint32_t frames_without_wrap;

	comp_dbg(dev, "demux_s16le()");

	if (!lookup || !lookup->num_elems)
		return;

	demux_init_look_up_pointers_s16(dev, sink, source, lookup);

	while (frames) {
		frames_without_wrap =
			demux_calc_frames_without_wrap_s16(dev, sink, source,
							   lookup);

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

		demux_check_for_wrap(sink, source, lookup);

		frames -= frames_without_wrap;
	}
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
static void mux_s16le(struct comp_dev *dev, struct audio_stream *sink,
		      const struct audio_stream **sources, uint32_t frames,
		      struct mux_look_up *lookup)
{
	uint8_t i;
	int16_t *src;
	int16_t *dst;
	uint32_t elem;
	uint32_t frames_without_wrap;

	comp_dbg(dev, "mux_s16le()");

	if (!lookup || !lookup->num_elems)
		return;

	mux_init_look_up_pointers_s16(dev, sink, sources, lookup);

	while (frames) {
		frames_without_wrap =
			mux_calc_frames_without_wrap_s16(dev, sink, sources,
							 lookup);

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

		mux_check_for_wrap(sink, sources, lookup);

		frames -= frames_without_wrap;
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE

static uint32_t mux_calc_frames_without_wrap_s32(struct comp_dev *dev,
						 struct audio_stream *sink,
						 const struct audio_stream
						 **sources,
						 struct mux_look_up *lookup)
{
	const struct audio_stream *source;
	uint32_t frames;
	uint32_t min_frames;
	uint32_t elem;
	void *ptr;

	/* dest pointer for all copy_elems in lookup refers to the same
	 * sink buffer (mux has one sink buffer), so dest min_frames
	 * calculation based only on lookup table first element is sufficient.
	 */
	ptr = (int32_t *)lookup->copy_elem[0].dest -
		lookup->copy_elem[0].out_ch;
	min_frames = audio_stream_frames_without_wrap(sink, ptr);

	for (elem = 0; elem < lookup->num_elems; elem++) {
		source = sources[lookup->copy_elem[elem].stream_id];

		ptr = (int32_t *)lookup->copy_elem[elem].src -
			lookup->copy_elem[elem].in_ch;
		frames = audio_stream_frames_without_wrap(source, ptr);

		min_frames = (frames < min_frames) ? frames : min_frames;
	}

	return min_frames;
}

static uint32_t demux_calc_frames_without_wrap_s32(struct comp_dev *dev,
						   struct audio_stream *sink,
						   const struct audio_stream
						   *source,
						   struct mux_look_up *lookup)
{
	uint32_t frames;
	uint32_t min_frames;
	void *ptr;

	/* for demux we process each source buffer separately - dest/src for
	 * each copy_elem refers to the same sink/source buffer, so min_frames
	 * calculation based only on lookup table first element is sufficient.
	 */
	ptr = (int32_t *)lookup->copy_elem[0].dest -
		lookup->copy_elem[0].out_ch;
	min_frames = audio_stream_frames_without_wrap(sink, ptr);

	ptr = (int32_t *)lookup->copy_elem[0].src -
		lookup->copy_elem[0].in_ch;
	frames = audio_stream_frames_without_wrap(source, ptr);

	min_frames = (frames < min_frames) ? frames : min_frames;

	return min_frames;
}

static void mux_init_look_up_pointers_s32(struct comp_dev *dev,
					  struct audio_stream *sink,
					  const struct audio_stream **sources,
					  struct mux_look_up *lookup)
{
	const struct audio_stream *source;
	uint32_t elem;

	/* init pointers */
	for (elem = 0; elem < lookup->num_elems; elem++) {
		source = sources[lookup->copy_elem[elem].stream_id];

		lookup->copy_elem[elem].src = (int32_t *)source->r_ptr +
			lookup->copy_elem[elem].in_ch;
		lookup->copy_elem[elem].src_inc = source->channels;

		lookup->copy_elem[elem].dest = (int32_t *)sink->w_ptr +
			lookup->copy_elem[elem].out_ch;
		lookup->copy_elem[elem].dest_inc = sink->channels;
	}
}

static void demux_init_look_up_pointers_s32(struct comp_dev *dev,
					    struct audio_stream *sink,
					    const struct audio_stream *source,
					    struct mux_look_up *lookup)
{
	uint32_t elem;

	/* init pointers */
	for (elem = 0; elem < lookup->num_elems; elem++) {
		lookup->copy_elem[elem].src = (int32_t *)source->r_ptr +
			lookup->copy_elem[elem].in_ch;
		lookup->copy_elem[elem].src_inc = source->channels;

		lookup->copy_elem[elem].dest = (int32_t *)sink->w_ptr +
			lookup->copy_elem[elem].out_ch;
		lookup->copy_elem[elem].dest_inc = sink->channels;
	}
}

/**
 * Source stream are routed to sinks with regard to look up table based on
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
static void demux_s32le(struct comp_dev *dev, struct audio_stream *sink,
			const struct audio_stream *source, uint32_t frames,
			struct mux_look_up *lookup)
{
	uint8_t i;
	int32_t *src;
	int32_t *dst;
	uint32_t elem;
	uint32_t frames_without_wrap;

	comp_dbg(dev, "demux_s32le");

	if (!lookup || !lookup->num_elems)
		return;

	demux_init_look_up_pointers_s32(dev, sink, source, lookup);

	while (frames) {
		frames_without_wrap =
			demux_calc_frames_without_wrap_s32(dev, sink, source,
							   lookup);

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

		demux_check_for_wrap(sink, source, lookup);

		frames -= frames_without_wrap;
	}
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
static void mux_s32le(struct comp_dev *dev, struct audio_stream *sink,
		      const struct audio_stream **sources, uint32_t frames,
		      struct mux_look_up *lookup)
{
	uint8_t i;
	int32_t *src;
	int32_t *dst;
	uint32_t elem;
	uint32_t frames_without_wrap;

	comp_dbg(dev, "mux_s32le()");

	if (!lookup || !lookup->num_elems)
		return;

	mux_init_look_up_pointers_s32(dev, sink, sources, lookup);

	while (frames) {
		frames_without_wrap =
			mux_calc_frames_without_wrap_s32(dev, sink, sources,
							 lookup);

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

		mux_check_for_wrap(sink, sources, lookup);

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

void mux_prepare_look_up_table(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	uint8_t i;
	uint8_t j;
	uint8_t k;
	uint8_t idx = 0;

	/* Prepare look up table */
	for (i = 0; i < cd->config.num_streams; i++) {
		for (j = 0; j < PLATFORM_MAX_CHANNELS; j++) {
			for (k = 0; k < PLATFORM_MAX_CHANNELS; k++) {
				if (cd->config.streams[i].mask[j] & BIT(k)) {
					/* MUX component has only one sink */
					cd->lookup[0].copy_elem[idx].in_ch = j;
					cd->lookup[0].copy_elem[idx].out_ch = k;
					cd->lookup[0].copy_elem[idx].stream_id =
						i;
					cd->lookup[0].num_elems = ++idx;
				}
			}
		}
	}
}

void demux_prepare_look_up_table(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	uint8_t i;
	uint8_t j;
	uint8_t k;
	uint8_t idx;

	/* Prepare look up table */
	for (i = 0; i < cd->config.num_streams; i++) {
		idx = 0;
		for (j = 0; j < PLATFORM_MAX_CHANNELS; j++) {
			for (k = 0; k < PLATFORM_MAX_CHANNELS; k++) {
				if (cd->config.streams[i].mask[j] & BIT(k)) {
					/* DEMUX component has only one source */
					cd->lookup[i].copy_elem[idx].in_ch = k;
					cd->lookup[i].copy_elem[idx].out_ch = j;
					cd->lookup[i].copy_elem[idx].stream_id =
						i;
					cd->lookup[i].num_elems = ++idx;
				}
			}
		}
	}
}

mux_func mux_get_processing_function(struct comp_dev *dev)
{
	struct comp_buffer *sinkb;
	uint8_t i;

	if (list_is_empty(&dev->bsink_list))
		return NULL;

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	for (i = 0; i < ARRAY_SIZE(mux_func_map); i++) {
		if (sinkb->stream.frame_fmt == mux_func_map[i].frame_format)
			return mux_func_map[i].mux_proc_func;
	}

	return NULL;
}

demux_func demux_get_processing_function(struct comp_dev *dev)
{
	struct comp_buffer *sourceb;
	uint8_t i;

	if (list_is_empty(&dev->bsource_list))
		return NULL;

	sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
				sink_list);

	for (i = 0; i < ARRAY_SIZE(mux_func_map); i++) {
		if (sourceb->stream.frame_fmt == mux_func_map[i].frame_format)
			return mux_func_map[i].demux_proc_func;
	}

	return NULL;
}

#endif /* CONFIG_COMP_MUX */
