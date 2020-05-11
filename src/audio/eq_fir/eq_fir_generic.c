// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/math/fir_config.h>

#if FIR_GENERIC

#include <sof/audio/eq_fir/eq_fir.h>
#include <sof/math/fir_generic.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#if CONFIG_FORMAT_S16LE
void eq_fir_s16(struct fir_state_32x16 fir[], const struct audio_stream *source,
		struct audio_stream *sink, int frames, int nch)
{
	struct fir_state_32x16 *filter;
	int16_t *x;
	int16_t *y;
	int32_t z;
	int idx;
	int ch;
	int i;

	for (ch = 0; ch < nch; ch++) {
		filter = &fir[ch];
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s16(source, idx);
			y = audio_stream_write_frag_s16(sink, idx);
			z = fir_32x16(filter, *x << 16);
			*y = sat_int16(Q_SHIFT_RND(z, 31, 15));
			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
void eq_fir_s24(struct fir_state_32x16 fir[], const struct audio_stream *source,
		struct audio_stream *sink, int frames, int nch)
{
	struct fir_state_32x16 *filter;
	int32_t *x;
	int32_t *y;
	int32_t z;
	int idx;
	int ch;
	int i;

	for (ch = 0; ch < nch; ch++) {
		filter = &fir[ch];
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s32(source, idx);
			y = audio_stream_write_frag_s32(sink, idx);
			z = fir_32x16(filter, *x << 8);
			*y = sat_int24(Q_SHIFT_RND(z, 31, 23));
			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
void eq_fir_s32(struct fir_state_32x16 fir[], const struct audio_stream *source,
		struct audio_stream *sink, int frames, int nch)
{
	struct fir_state_32x16 *filter;
	int32_t *x;
	int32_t *y;
	int idx;
	int ch;
	int i;

	for (ch = 0; ch < nch; ch++) {
		filter = &fir[ch];
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s32(source, idx);
			y = audio_stream_write_frag_s32(sink, idx);
			*y = fir_32x16(filter, *x);
			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#endif /* FIR_GENERIC */
