// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>

/**
 * \file audio/pcm_converter/pcm_converter.c
 * \brief PCM converter common functions
 * \authors Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 */

#include <sof/compiler_attributes.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/pcm_converter.h>
#include <rtos/panic.h>

int pcm_convert_as_linear(const struct cir_buf_source *source, size_t s_size_in,
			  struct cir_buf_sink *sink, size_t s_size_out,
			  size_t samples, pcm_converter_lin_func converter)
{
	const int log2_s_size_in = ffs(s_size_in) - 1;
	const int log2_s_size_out = ffs(s_size_out) - 1;
	const char *r_ptr = source->ptr;
	char *w_ptr = sink->ptr;
	size_t i = 0;
	size_t chunk;
	size_t N1, N2;


	while (i < samples) {
		/* calculate chunk size; shifting by log2_s_size is dividing by s_size */
		N1 = ((const char *)source->buf_end - r_ptr) >> log2_s_size_in;
		N2 = ((char *)sink->buf_end - w_ptr) >> log2_s_size_out;
		chunk = MIN(N1, N2);
		chunk = MIN(chunk, samples - i);

		/* run conversion on linear memory region */
		converter(r_ptr, w_ptr, chunk);

		/* move pointers */
		r_ptr = source_cir_buf_wrap(r_ptr + chunk * s_size_in,
					    source->buf_start, source->buf_end);
		w_ptr = cir_buf_wrap(w_ptr + chunk * s_size_out,
				     sink->buf_start, sink->buf_end);
		i += chunk;
	}

	return samples;
}

/* Copy "bytes" of raw data from a source to a sink circular buffer. */
static void just_copy_bytes(const struct cir_buf_source *source, struct cir_buf_sink *sink,
			    size_t bytes)
{
	cir_buf_copy(source->ptr, source->buf_start, source->buf_end,
		     sink->ptr, sink->buf_start, sink->buf_end, bytes);
}

int just_copy_1b(const struct cir_buf_source *source, uint32_t src_channels,
		 struct cir_buf_sink *sink, uint32_t sink_channels,
		 size_t samples, uint32_t chmap)
{
	just_copy_bytes(source, sink, samples);
	return samples;
}

int just_copy_2b(const struct cir_buf_source *source, uint32_t src_channels,
		 struct cir_buf_sink *sink, uint32_t sink_channels,
		 size_t samples, uint32_t chmap)
{
	just_copy_bytes(source, sink, samples * sizeof(int16_t));
	return samples;
}

int just_copy_3b(const struct cir_buf_source *source, uint32_t src_channels,
		 struct cir_buf_sink *sink, uint32_t sink_channels,
		 size_t samples, uint32_t chmap)
{
	just_copy_bytes(source, sink, samples * 3);
	return samples;
}

int just_copy_4b(const struct cir_buf_source *source, uint32_t src_channels,
		 struct cir_buf_sink *sink, uint32_t sink_channels,
		 size_t samples, uint32_t chmap)
{
	just_copy_bytes(source, sink, samples * sizeof(int32_t));
	return samples;
}
