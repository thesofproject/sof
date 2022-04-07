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
#include <sof/debug/panic.h>

int pcm_convert_as_linear(const struct audio_stream __sparse_cache *source, uint32_t ioffset,
			  struct audio_stream __sparse_cache *sink, uint32_t ooffset,
			  uint32_t samples, pcm_converter_lin_func converter)
{
	const int s_size_in = audio_stream_sample_bytes(source);
	const int s_size_out = audio_stream_sample_bytes(sink);
	const int log2_s_size_in = ffs(s_size_in) - 1;
	const int log2_s_size_out = ffs(s_size_out) - 1;
	char *r_ptr = audio_stream_get_frag(source, source->r_ptr, ioffset,
					    s_size_in);
	char *w_ptr = audio_stream_get_frag(sink, sink->w_ptr, ooffset,
					    s_size_out);
	int i = 0;
	int chunk;
	int N1, N2;

	/* assert enough avail/free samples in source and sink buffer */
	if (audio_stream_get_avail_samples(source) < samples + ioffset)
		return -EINVAL;
	if (audio_stream_get_free_samples(sink) < samples + ooffset)
		return -EINVAL;

	while (i < samples) {
		/* calculate chunk size */
		/* "">> log2_s_size" is equal "/ s_size" here */
		N1 = audio_stream_bytes_without_wrap(source, r_ptr) >>
			log2_s_size_in;
		N2 = audio_stream_bytes_without_wrap(sink, w_ptr) >>
			log2_s_size_out;
		chunk = MIN(N1, N2);
		chunk = MIN(chunk, samples - i);

		/* run conversion on linear memory region */
		converter(r_ptr, w_ptr, chunk);

		/* move pointers */
		r_ptr = audio_stream_wrap(source, r_ptr + chunk * s_size_in);
		w_ptr = audio_stream_wrap(sink, w_ptr + chunk * s_size_out);
		i += chunk;
	}

	return samples;
}
