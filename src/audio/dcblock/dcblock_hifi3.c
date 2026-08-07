// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <andrula.song@intel.com>

#include <stdint.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/audio_stream.h>

#include "dcblock.h"

#if SOF_USE_HIFI(3, DCBLOCK)

#include <xtensa/tie/xt_hifi3.h>
LOG_MODULE_DECLARE(dcblock, CONFIG_SOF_LOG_LEVEL);

static inline ae_int32x2  dcblock_cal(ae_int32x2 R, ae_int32x2 state_x, ae_int32x2 state_y,
				      ae_int32x2 sample)
{
	ae_int64 out, temp;

	/* R: Q2.30, y_prev: Q1.31 the result is Q2.62 */
	temp = AE_MULF32S_LL(R, state_y);
	out = AE_SUB64(AE_MOVAD32_L(sample), AE_MOVAD32_L(state_x));
	/* shift out to 2.62 */
	out = AE_ADD64S(AE_SLAI64S(out, 31), temp);
	/* shift out to 1.63 */
	return AE_ROUND32F64SSYM(AE_SLAI64S(out, 1));
}

/* Set source as circular buffer 0 for the strided per-channel reads */
static inline void dcblock_set_circular(void *src_begin, void *src_end)
{
	AE_SETCBEGIN0(src_begin);
	AE_SETCEND0(src_end);
}

#if CONFIG_FORMAT_S16LE
static int dcblock_s16_default(struct comp_data *cd,
			       struct cir_buf_source *source,
			       struct cir_buf_sink *sink,
			       uint32_t frames)
{
	ae_int16 *src = (ae_int16 *)source->ptr;
	ae_int16 *dst = (ae_int16 *)sink->ptr;
	ae_int16 *x_start = (ae_int16 *)source->buf_start;
	ae_int16 *y_start = (ae_int16 *)sink->buf_start;
	ae_int16 *x_end = (ae_int16 *)source->buf_end;
	ae_int16 *y_end = (ae_int16 *)sink->buf_end;
	ae_int16 *in, *out;
	ae_int32x2 R, state_x, state_y, sample;
	ae_int16x4 in_sample, out_sample;
	int x_size = x_end - x_start;
	int y_size = y_end - y_start;
	int nch = cd->channels;
	int remaining = frames * nch;
	const int inc = nch * sizeof(ae_int16);
	int ch, i, n;

	/* Set source as circular buffer 0 for the strided per-channel reads */
	dcblock_set_circular(x_start, x_end);

	while (remaining) {
		/* The sink uses linear stores, so limit the chunk to the sink
		 * contiguous space. The source wraps via circular addressing.
		 */
		n = y_end - dst;
		n = MIN(n, remaining);
		for (ch = 0; ch < nch; ch++) {
			in = src + ch;
			out = dst + ch;
			state_x = cd->state[ch].x_prev;
			state_y = cd->state[ch].y_prev;
			R = cd->R_coeffs[ch];
			for (i = 0; i < n; i += nch) {
				/* Load a 16 bit sample*/
				AE_L16_XC(in_sample, in, inc);
				/* store the 16 bit sample to high 16bit of 32bit register*/
				sample = AE_CVT32X2F16_32(in_sample);
				state_y = dcblock_cal(R, state_x, state_y, sample);
				state_x = sample;
				out_sample = AE_ROUND16X4F32SSYM(state_y, state_y);
				AE_S16_0_XP(out_sample, out, inc);
			}
			cd->state[ch].x_prev = state_x;
			cd->state[ch].y_prev = state_y;
		}
		remaining -= n;
		dst += n;
		if (dst >= y_end)
			dst -= y_size;
		src += n;
		if (src >= x_end)
			src -= x_size;
	}

	return 0;
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static int dcblock_s24_default(struct comp_data *cd,
			       struct cir_buf_source *source,
			       struct cir_buf_sink *sink,
			       uint32_t frames)
{
	ae_int32 *src = (ae_int32 *)source->ptr;
	ae_int32 *dst = (ae_int32 *)sink->ptr;
	ae_int32 *x_start = (ae_int32 *)source->buf_start;
	ae_int32 *y_start = (ae_int32 *)sink->buf_start;
	ae_int32 *x_end = (ae_int32 *)source->buf_end;
	ae_int32 *y_end = (ae_int32 *)sink->buf_end;
	ae_int32 *in, *out;
	ae_int32x2 R, state_x, state_y;
	ae_int32x2 in_sample, out_sample;
	int x_size = x_end - x_start;
	int y_size = y_end - y_start;
	int nch = cd->channels;
	int remaining = frames * nch;
	const int inc = nch * sizeof(ae_int32);
	int ch, i, n;

	dcblock_set_circular(x_start, x_end);

	while (remaining) {
		n = y_end - dst;
		n = MIN(n, remaining);
		for (ch = 0; ch < nch; ch++) {
			in = src + ch;
			out = dst + ch;
			state_x = cd->state[ch].x_prev;
			state_y = cd->state[ch].y_prev;
			R = cd->R_coeffs[ch];
			for (i = 0; i < n; i += nch) {
				AE_L32_XC(in_sample, in, inc);
				in_sample = AE_SLAI32(in_sample, 8);
				state_y = dcblock_cal(R, state_x, state_y, in_sample);
				state_x = in_sample;
				out_sample = AE_SRAI32R(state_y, 8);
				out_sample = AE_SLAI32S(out_sample, 8);
				out_sample = AE_SRAI32R(out_sample, 8);
				AE_S32_L_XP(out_sample, out, inc);
			}
			cd->state[ch].x_prev = state_x;
			cd->state[ch].y_prev = state_y;
		}
		remaining -= n;
		dst += n;
		if (dst >= y_end)
			dst -= y_size;
		src += n;
		if (src >= x_end)
			src -= x_size;
	}

	return 0;
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static int dcblock_s32_default(struct comp_data *cd,
			       struct cir_buf_source *source,
			       struct cir_buf_sink *sink,
			       uint32_t frames)
{
	ae_int32 *src = (ae_int32 *)source->ptr;
	ae_int32 *dst = (ae_int32 *)sink->ptr;
	ae_int32 *x_start = (ae_int32 *)source->buf_start;
	ae_int32 *y_start = (ae_int32 *)sink->buf_start;
	ae_int32 *x_end = (ae_int32 *)source->buf_end;
	ae_int32 *y_end = (ae_int32 *)sink->buf_end;
	ae_int32 *in, *out;
	ae_int32x2 R, state_x, state_y;
	ae_int32x2 in_sample;
	int x_size = x_end - x_start;
	int y_size = y_end - y_start;
	int nch = cd->channels;
	int remaining = frames * nch;
	const int inc = nch * sizeof(ae_int32);
	int ch, i, n;

	dcblock_set_circular(x_start, x_end);

	while (remaining) {
		n = y_end - dst;
		n = MIN(n, remaining);
		for (ch = 0; ch < nch; ch++) {
			in = src + ch;
			out = dst + ch;
			state_x = cd->state[ch].x_prev;
			state_y = cd->state[ch].y_prev;
			R = cd->R_coeffs[ch];
			for (i = 0; i < n; i += nch) {
				AE_L32_XC(in_sample, in, inc);
				state_y = dcblock_cal(R, state_x, state_y, in_sample);
				state_x = in_sample;
				AE_S32_L_XP(state_y, out, inc);
			}
			cd->state[ch].x_prev = state_x;
			cd->state[ch].y_prev = state_y;
		}
		remaining -= n;
		dst += n;
		if (dst >= y_end)
			dst -= y_size;
		src += n;
		if (src >= x_end)
			src -= x_size;
	}

	return 0;
}
#endif /* CONFIG_FORMAT_S32LE */

const struct dcblock_func_map dcblock_fnmap[] = {
/* { SOURCE_FORMAT , PROCESSING FUNCTION } */
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, dcblock_s16_default },
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, dcblock_s24_default },
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, dcblock_s32_default },
#endif /* CONFIG_FORMAT_S32LE */
};

const size_t dcblock_fncount = ARRAY_SIZE(dcblock_fnmap);
#endif
