/*
 * Copyright (c) 2019, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

#include <config.h>

#if CONFIG_COMP_MUX

#include <sof/audio/mux.h>

/*
 * \brief Fetch 16b samples from source buffer and perform routing operations
 *	  based on mask provided.
 * \param[in,out] source Source buffer.
 * \param[in] num_ch Number of channels in source buffer.
 * \param[in] offset Offset in source buffer.
 * \param[in] mask Routing bitmask for calculating output sample.
 */
static inline int32_t calc_sample_s16le(struct comp_buffer *source,
					uint8_t num_ch, uint32_t offset,
					uint8_t mask)
{
	int32_t sample = 0;
	int16_t *src;
	int8_t in_ch;

	if (mask == 0)
		return 0;

	for (in_ch = 0; in_ch < num_ch; in_ch++) {
		if (mask & BIT(in_ch)) {
			src = buffer_read_frag_s16(source, offset + in_ch);
			sample += *src;
		}
	}

	return sample;
}

/*
 * \brief Fetch 24b samples from source buffer and perform routing operations
 *	  based on mask provided.
 * \param[in,out] source Source buffer.
 * \param[in] num_ch Number of channels in source buffer.
 * \param[in] offset Offset in source buffer.
 * \param[in] mask Routing bitmask for calculating output sample.
 */
static inline int32_t calc_sample_s24le(struct comp_buffer *source,
					uint8_t num_ch, uint32_t offset,
					uint8_t mask)
{
	int32_t sample = 0;
	int32_t *src;
	int8_t in_ch;

	if (mask == 0)
		return 0;

	for (in_ch = 0; in_ch < num_ch; in_ch++) {
		if (mask & BIT(in_ch)) {
			src = buffer_read_frag_s32(source, offset + in_ch);
			sample += sign_extend_s24(*src);
		}
	}

	return sample;
}

/*
 * \brief Fetch 32b samples from source buffer and perform routing operations
 *	  based on mask provided.
 * \param[in,out] source Source buffer.
 * \param[in] num_ch Number of channels in source buffer.
 * \param[in] offset Offset in source buffer.
 * \param[in] mask Routing bitmask for calculating output sample.
 */
static inline int64_t calc_sample_s32le(struct comp_buffer *source,
					uint8_t num_ch, uint32_t offset,
					uint8_t mask)
{
	int64_t sample = 0;
	int32_t *src;
	int8_t in_ch;

	if (mask == 0)
		return 0;

	for (in_ch = 0; in_ch < num_ch; in_ch++) {
		if (mask & BIT(in_ch)) {
			src = buffer_read_frag_s32(source, offset + in_ch);
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
 * \param[in,out] dev Demux base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] data Parameters describing channel count and routing.
 */
static void demux_s16le(struct comp_dev *dev, struct comp_buffer *sink,
			struct comp_buffer *source, uint32_t frames,
			struct mux_stream_data *data)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t sample;
	int16_t *dst;
	uint8_t i;
	uint8_t out_ch;

	for (i = 0; i < frames; i++) {
		for (out_ch = 0; out_ch < data->num_channels; out_ch++) {
			sample = calc_sample_s16le(source,
						   cd->config.num_channels,
						   i * cd->config.num_channels,
						   data->mask[out_ch]);

			/* saturate to 16 bits */
			dst = buffer_write_frag_s16(sink,
				i * data->num_channels + out_ch);
			*dst = sat_int16(sample);
		}
	}
}

/* \brief Demuxing 24 bit streams.
 *
 * Source stream is routed to sink with regard to routing bitmasks from
 * mux_stream_data structure. Each bitmask describes composition of single
 * output channel.
 *
 * \param[in,out] dev Demux base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] data Parameters describing channel count and routing.
 */
static void demux_s24le(struct comp_dev *dev, struct comp_buffer *sink,
			struct comp_buffer *source, uint32_t frames,
			struct mux_stream_data *data)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t sample;
	int32_t *dst;
	uint8_t i;
	uint8_t out_ch;

	for (i = 0; i < frames; i++) {
		for (out_ch = 0; out_ch < data->num_channels; out_ch++) {
			sample = calc_sample_s24le(source,
						   cd->config.num_channels,
						   i * cd->config.num_channels,
						   data->mask[out_ch]);

			/* saturate to 24 bits */
			dst = buffer_write_frag_s32(sink,
				i * data->num_channels + out_ch);
			*dst = sat_int24(sample);
		}
	}
}

/* \brief Demuxing 32 bit streams.
 *
 * Source stream is routed to sink with regard to routing bitmasks from
 * mux_stream_data structure. Each bitmask describes composition of single
 * output channel.
 *
 * \param[in,out] dev Demux base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] source Source buffer.
 * \param[in] frames Number of frames to process.
 * \param[in] data Parameters describing channel count and routing.
 */
static void demux_s32le(struct comp_dev *dev, struct comp_buffer *sink,
			struct comp_buffer *source, uint32_t frames,
			struct mux_stream_data *data)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int64_t sample;
	int32_t *dst;
	uint8_t i;
	uint8_t out_ch;

	for (i = 0; i < frames; i++) {
		for (out_ch = 0; out_ch < data->num_channels; out_ch++) {
			sample = calc_sample_s32le(source,
						   cd->config.num_channels,
						   i * cd->config.num_channels,
						   data->mask[out_ch]);

			/* saturate to 32 bits */
			dst = buffer_write_frag_s32(sink,
				i * data->num_channels + out_ch);
			*dst = sat_int32(sample);
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
 * \param[in,out] dev Demux base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] sources Array of source buffers.
 * \param[in] frames Number of frames to process.
 * \param[in] data Array of parameters describing channel count and routing for
 *		   each stream.
 */
static void mux_s16le(struct comp_dev *dev, struct comp_buffer *sink,
		      struct comp_buffer **sources, uint32_t frames,
		      struct mux_stream_data *data)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	uint8_t i;
	uint8_t j;
	uint8_t out_ch;
	int16_t *dst;
	int32_t sample;

	for (i = 0; i < frames; i++) {
		for (out_ch = 0; out_ch < cd->config.num_channels; out_ch++) {
			sample = 0;

			for (j = 0; j < MUX_MAX_STREAMS; j++) {
				source = sources[j];
				if (!source)
					continue;

				sample += calc_sample_s16le(source,
						data[j].num_channels,
						i * data[j].num_channels,
						data[j].mask[out_ch]);
			}
			dst = buffer_write_frag_s16(sink,
				i * data->num_channels + out_ch);
			*dst = sat_int16(sample);
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
 * \param[in,out] dev Demux base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] sources Array of source buffers.
 * \param[in] frames Number of frames to process.
 * \param[in] data Array of parameters describing channel count and routing for
 *		   each stream.
 */
static void mux_s24le(struct comp_dev *dev, struct comp_buffer *sink,
		      struct comp_buffer **sources, uint32_t frames,
		      struct mux_stream_data *data)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	uint8_t i;
	uint8_t j;
	uint8_t out_ch;
	int32_t *dst;
	int32_t sample;

	for (i = 0; i < frames; i++) {
		for (out_ch = 0; out_ch < cd->config.num_channels; out_ch++) {
			sample = 0;
			for (j = 0; j < MUX_MAX_STREAMS; j++) {
				source = sources[j];
				if (!source)
					continue;

				sample += calc_sample_s24le(source,
						data[j].num_channels,
						i * data[j].num_channels,
						data[j].mask[out_ch]);
			}
			dst = buffer_write_frag_s32(sink,
				i * data->num_channels + out_ch);
			*dst = sat_int24(sample);
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
 * \param[in,out] dev Demux base component device.
 * \param[in,out] sink Destination buffer.
 * \param[in,out] sources Array of source buffers.
 * \param[in] frames Number of frames to process.
 * \param[in] data Array of parameters describing channel count and routing for
 *		   each stream.
 */
static void mux_s32le(struct comp_dev *dev, struct comp_buffer *sink,
		      struct comp_buffer **sources, uint32_t frames,
		      struct mux_stream_data *data)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *source;
	uint8_t i;
	uint8_t j;
	uint8_t out_ch;
	int32_t *dst;
	int64_t sample;

	for (i = 0; i < frames; i++) {
		for (out_ch = 0; out_ch < cd->config.num_channels; out_ch++) {
			sample = 0;
			for (j = 0; j < MUX_MAX_STREAMS; j++) {
				source = sources[j];
				if (!source)
					continue;

				sample += calc_sample_s32le(source,
						data[j].num_channels,
						i * data[j].num_channels,
						data[j].mask[out_ch]);
			}
			dst = buffer_write_frag_s32(sink,
				i * data->num_channels + out_ch);
			*dst = sat_int32(sample);
		}
	}
}

const struct comp_func_map mux_func_map[] = {
	{ SOF_IPC_FRAME_S16_LE, &mux_s16le, &demux_s16le },
	{ SOF_IPC_FRAME_S24_4LE, &mux_s24le, &demux_s24le },
	{ SOF_IPC_FRAME_S32_LE, &mux_s32le, &demux_s32le },
};

mux_func mux_get_processing_function(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	uint8_t i;

	for (i = 0; i < ARRAY_SIZE(mux_func_map); i++) {
		if (cd->config.frame_format == mux_func_map[i].frame_format)
			return mux_func_map[i].mux_proc_func;
	}

	return NULL;
}

demux_func demux_get_processing_function(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	uint8_t i;

	for (i = 0; i < ARRAY_SIZE(mux_func_map); i++) {
		if (cd->config.frame_format == mux_func_map[i].frame_format)
			return mux_func_map[i].demux_proc_func;
	}

	return NULL;
}

#endif /* CONFIG_COMP_MUX */
