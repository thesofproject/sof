// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>

#include <config.h>

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

#if CONFIG_FORMAT_S16LE
/*
 * \brief Fetch 16b samples from source buffer and perform routing operations
 *	  based on mask provided.
 * \param[in,out] source Source buffer.
 * \param[in] offset Offset in source buffer.
 * \param[in] mask Routing bitmask for calculating output sample.
 */
UT_STATIC inline int32_t calc_sample_s16le(const struct audio_stream *source,
					   uint32_t offset, uint8_t mask)
{
	int32_t sample = 0;
	int16_t *src;
	int8_t in_ch;

	if (mask == 0)
		return 0;

	for (in_ch = 0; in_ch < source->channels; in_ch++) {
		if (mask & BIT(in_ch)) {
			src = audio_stream_read_frag_s16(source,
							 offset + in_ch);
			sample += *src;
		}
	}

	return sample;
}

/* \brief Demuxing 16 bit streams.
 *
 * Source stream is routed to sink with regard to routing bitmasks from
 * mux_stream_data structure. Each bitmask describes composition of single
 * output channel.
 *
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] data Parameters describing channel count and routing.
 */
static void demux_s16le(struct audio_stream *sink,
			const struct audio_stream *source, uint32_t frames,
			struct mux_stream_data *data)
{
	int32_t sample;
	int16_t *dst;
	uint32_t dst_idx;
	uint8_t i;
	uint8_t out_ch;

	for (i = 0; i < frames; i++) {
		for (out_ch = 0; out_ch < sink->channels; out_ch++) {
			sample = calc_sample_s16le(source,
						   i * source->channels,
						   data->mask[out_ch]);

			/* saturate to 16 bits */
			dst_idx = i * sink->channels + out_ch;
			dst = audio_stream_write_frag_s16(sink, dst_idx);
			*dst = sat_int16(sample);
		}
	}
}

/* \brief Muxing 16 bit streams.
 *
 * Source streams are routed to sink with regard to routing bitmasks from
 * mux_stream_data structures array. Each source stream has bitmask for each
 * of it's channels describing to which channels of output stream it
 * contributes.
 *
 * \param[in,out] sink Destination buffer.
 * \param[in,out] sources Array of source buffers.
 * \param[in] frames Number of frames to process.
 * \param[in] data Array of parameters describing channel count and routing for
 *		   each stream.
 */
static void mux_s16le(struct audio_stream *sink,
		      const struct audio_stream **sources, uint32_t frames,
		      struct mux_stream_data *data)
{
	const struct audio_stream *source;
	uint8_t i;
	uint8_t j;
	uint8_t out_ch;
	int16_t *dst;
	uint32_t dst_idx;
	int32_t sample;

	for (i = 0; i < frames; i++) {
		for (out_ch = 0; out_ch < sink->channels; out_ch++) {
			sample = 0;

			for (j = 0; j < MUX_MAX_STREAMS; j++) {
				source = sources[j];
				if (!source)
					continue;

				sample += calc_sample_s16le(source,
						i * source->channels,
						data[j].mask[out_ch]);
			}
			dst_idx = i * sink->channels + out_ch;
			dst = audio_stream_write_frag_s16(sink, dst_idx);
			*dst = sat_int16(sample);
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
/*
 * \brief Fetch 24b samples from source buffer and perform routing operations
 *	  based on mask provided.
 * \param[in,out] source Source buffer.
 * \param[in] offset Offset in source buffer.
 * \param[in] mask Routing bitmask for calculating output sample.
 */
UT_STATIC inline int32_t calc_sample_s24le(const struct audio_stream *source,
					   uint32_t offset, uint8_t mask)
{
	int32_t sample = 0;
	int32_t *src;
	int8_t in_ch;

	if (mask == 0)
		return 0;

	for (in_ch = 0; in_ch < source->channels; in_ch++) {
		if (mask & BIT(in_ch)) {
			src = audio_stream_read_frag_s32(source,
							 offset + in_ch);
			sample += sign_extend_s24(*src);
		}
	}

	return sample;
}

/* \brief Demuxing 24 bit streams.
 *
 * Source stream is routed to sink with regard to routing bitmasks from
 * mux_stream_data structure. Each bitmask describes composition of single
 * output channel.
 *
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] data Parameters describing channel count and routing.
 */
static void demux_s24le(struct audio_stream *sink,
			const struct audio_stream *source, uint32_t frames,
			struct mux_stream_data *data)
{
	int32_t sample;
	int32_t *dst;
	uint32_t dst_idx;
	uint8_t i;
	uint8_t out_ch;

	for (i = 0; i < frames; i++) {
		for (out_ch = 0; out_ch < sink->channels; out_ch++) {
			sample = calc_sample_s24le(source,
						   i * source->channels,
						   data->mask[out_ch]);

			/* saturate to 24 bits */
			dst_idx = i * sink->channels + out_ch;
			dst = audio_stream_write_frag_s32(sink, dst_idx);
			*dst = sat_int24(sample);
		}
	}
}

/* \brief Muxing 24 bit streams.
 *
 * Source streams are routed to sink with regard to routing bitmasks from
 * mux_stream_data structures array. Each source stream has bitmask for each
 * of it's channels describing to which channels of output stream it
 * contributes.
 *
 * \param[in,out] sink Destination buffer.
 * \param[in,out] sources Array of source buffers.
 * \param[in] frames Number of frames to process.
 * \param[in] data Array of parameters describing channel count and routing for
 *		   each stream.
 */
static void mux_s24le(struct audio_stream *sink,
		      const struct audio_stream **sources, uint32_t frames,
		      struct mux_stream_data *data)
{
	const struct audio_stream *source;
	uint8_t i;
	uint8_t j;
	uint8_t out_ch;
	int32_t *dst;
	uint32_t dst_idx;
	int32_t sample;

	for (i = 0; i < frames; i++) {
		for (out_ch = 0; out_ch < sink->channels; out_ch++) {
			sample = 0;
			for (j = 0; j < MUX_MAX_STREAMS; j++) {
				source = sources[j];
				if (!source)
					continue;

				sample += calc_sample_s24le(source,
						i * source->channels,
						data[j].mask[out_ch]);
			}
			dst_idx = i * sink->channels + out_ch;
			dst = audio_stream_write_frag_s32(sink, dst_idx);
			*dst = sat_int24(sample);
		}
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/*
 * \brief Fetch 32b samples from source buffer and perform routing operations
 *	  based on mask provided.
 * \param[in,out] source Source buffer.
 * \param[in] offset Offset in source buffer.
 * \param[in] mask Routing bitmask for calculating output sample.
 */
UT_STATIC inline int64_t calc_sample_s32le(const struct audio_stream *source,
					   uint32_t offset, uint8_t mask)
{
	int64_t sample = 0;
	int32_t *src;
	int8_t in_ch;

	if (mask == 0)
		return 0;

	for (in_ch = 0; in_ch < source->channels; in_ch++) {
		if (mask & BIT(in_ch)) {
			src = audio_stream_read_frag_s32(source,
							 offset + in_ch);
			sample += *src;
		}
	}

	return sample;
}

/* \brief Demuxing 32 bit streams.
 *
 * Source stream is routed to sink with regard to routing bitmasks from
 * mux_stream_data structure. Each bitmask describes composition of single
 * output channel.
 *
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] data Parameters describing channel count and routing.
 */
static void demux_s32le(struct audio_stream *sink,
			const struct audio_stream *source, uint32_t frames,
			struct mux_stream_data *data)
{
	int64_t sample;
	int32_t *dst;
	uint32_t dst_idx;
	uint8_t i;
	uint8_t out_ch;

	for (i = 0; i < frames; i++) {
		for (out_ch = 0; out_ch < sink->channels; out_ch++) {
			sample = calc_sample_s32le(source,
						   i * source->channels,
						   data->mask[out_ch]);

			/* saturate to 32 bits */
			dst_idx = i * sink->channels + out_ch;
			dst = audio_stream_write_frag_s32(sink, dst_idx);
			*dst = sat_int32(sample);
		}
	}
}

/* \brief Muxing 32 bit streams.
 *
 * Source streams are routed to sink with regard to routing bitmasks from
 * mux_stream_data structures array. Each source stream has bitmask for each
 * of it's channels describing to which channels of output stream it
 * contributes.
 *
 * \param[in,out] sink Destination buffer.
 * \param[in,out] sources Array of source buffers.
 * \param[in] frames Number of frames to process.
 * \param[in] data Array of parameters describing channel count and routing for
 *		   each stream.
 */
static void mux_s32le(struct audio_stream *sink,
		      const struct audio_stream **sources, uint32_t frames,
		      struct mux_stream_data *data)
{
	const struct audio_stream *source;
	uint8_t i;
	uint8_t j;
	uint8_t out_ch;
	int32_t *dst;
	uint32_t dst_idx;
	int64_t sample;

	for (i = 0; i < frames; i++) {
		for (out_ch = 0; out_ch < sink->channels; out_ch++) {
			sample = 0;
			for (j = 0; j < MUX_MAX_STREAMS; j++) {
				source = sources[j];
				if (!source)
					continue;

				sample += calc_sample_s32le(source,
						i * source->channels,
						data[j].mask[out_ch]);
			}
			dst_idx = i * sink->channels + out_ch;
			dst = audio_stream_write_frag_s32(sink, dst_idx);
			*dst = sat_int32(sample);
		}
	}
}

#endif /* CONFIG_FORMAT_S32LE */

const struct comp_func_map mux_func_map[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, &mux_s16le, &demux_s16le },
#endif
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, &mux_s24le, &demux_s24le },
#endif
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, &mux_s32le, &demux_s32le },
#endif
};

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
