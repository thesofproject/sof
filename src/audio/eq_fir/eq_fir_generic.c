// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/math/fir_config.h>
#include <sof/common.h>

#if SOF_USE_HIFI(NONE, FILTER)

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/math/fir_generic.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "eq_fir.h"

LOG_MODULE_DECLARE(eq_fir, CONFIG_SOF_LOG_LEVEL);

#if CONFIG_FORMAT_S16LE
void eq_fir_s16(struct fir_state_32x16 fir[], struct cir_buf_source *source,
		struct cir_buf_sink *sink, int frames, int channels)
{
	struct fir_state_32x16 *filter;
	int32_t z;
	const int16_t *x0;
	int16_t *y0;
	const int16_t *x = source->ptr;
	int16_t *y = sink->ptr;
	int nmax, n, i, j;
	int remaining_samples = frames * channels;

	while (remaining_samples) {
		nmax = cir_buf_samples_without_wrap_s16(x, source->buf_end);
		n = MIN(remaining_samples, nmax);
		nmax = cir_buf_samples_without_wrap_s16(y, sink->buf_end);
		n = MIN(n, nmax);
		for (j = 0; j < channels; j++) {
			x0 = x + j;
			y0 = y + j;
			filter = &fir[j];
			for (i = 0; i < n; i += channels) {
				z = fir_32x16(filter, *x0 << 16);
				*y0 = sat_int16(Q_SHIFT_RND(z, 31, 15));
				x0 += channels;
				y0 += channels;
			}
		}
		remaining_samples -= n;
		x = source_cir_buf_wrap(x + n, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
void eq_fir_s24(struct fir_state_32x16 fir[], struct cir_buf_source *source,
		struct cir_buf_sink *sink, int frames, int channels)
{
	struct fir_state_32x16 *filter;
	int32_t z;
	const int32_t *x0;
	int32_t *y0;
	const int32_t *x = source->ptr;
	int32_t *y = sink->ptr;
	int nmax, n, i, j;
	int remaining_samples = frames * channels;

	while (remaining_samples) {
		nmax = cir_buf_samples_without_wrap_s32(x, source->buf_end);
		n = MIN(remaining_samples, nmax);
		nmax = cir_buf_samples_without_wrap_s32(y, sink->buf_end);
		n = MIN(n, nmax);
		for (j = 0; j < channels; j++) {
			x0 = x + j;
			y0 = y + j;
			filter = &fir[j];
			for (i = 0; i < n; i += channels) {
				z = fir_32x16(filter, *x0 << 8);
				*y0 = sat_int24(Q_SHIFT_RND(z, 31, 23));
				x0 += channels;
				y0 += channels;
			}
		}
		remaining_samples -= n;
		x = source_cir_buf_wrap(x + n, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
void eq_fir_s32(struct fir_state_32x16 fir[], struct cir_buf_source *source,
		struct cir_buf_sink *sink, int frames, int channels)
{
	struct fir_state_32x16 *filter;
	const int32_t *x0;
	int32_t *y0;
	const int32_t *x = source->ptr;
	int32_t *y = sink->ptr;
	int nmax, n, i, j;
	int remaining_samples = frames * channels;

	while (remaining_samples) {
		nmax = cir_buf_samples_without_wrap_s32(x, source->buf_end);
		n = MIN(remaining_samples, nmax);
		nmax = cir_buf_samples_without_wrap_s32(y, sink->buf_end);
		n = MIN(n, nmax);
		for (j = 0; j < channels; j++) {
			x0 = x + j;
			y0 = y + j;
			filter = &fir[j];
			for (i = 0; i < n; i += channels) {
				*y0 = fir_32x16(filter, *x0);
				x0 += channels;
				y0 += channels;
			}
		}
		remaining_samples -= n;
		x = source_cir_buf_wrap(x + n, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#endif /* FILTER_HIFI_NONE */
