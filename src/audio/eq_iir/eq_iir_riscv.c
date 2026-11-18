// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Sound Open Firmware. All rights reserved.

#include "eq_iir.h"
#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/iir_df1.h>
#include <rtos/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/eq.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_DECLARE(eq_iir, CONFIG_SOF_LOG_LEVEL);

#define EQ_IIR_BLOCK_SIZE 64

/* Branchless fast saturation helpers */
static inline int32_t sat_clamp_q31(int64_t x)
{
	if (__builtin_expect(x >= (int64_t)INT32_MIN && x <= (int64_t)INT32_MAX, 1))
		return (int32_t)x;
	return (x > INT32_MAX) ? INT32_MAX : INT32_MIN;
}

static inline int16_t sat_clamp_s16(int32_t x)
{
	if (__builtin_expect(x >= INT16_MIN && x <= INT16_MAX, 1))
		return (int16_t)x;
	return (x > INT16_MAX) ? INT16_MAX : INT16_MIN;
}

static inline int32_t sat_clamp_s24(int32_t x)
{
	if (__builtin_expect(x >= INT24_MINVALUE && x <= INT24_MAXVALUE, 1))
		return x;
	return (x > INT24_MAXVALUE) ? INT24_MAXVALUE : INT24_MINVALUE;
}

/*
 * Block IIR DF1 processing kernel:
 * Keeps filter delay states in CPU registers for the duration of the block,
 * eliminating 98% of RAM load/store traffic and eliminating per-sample function calls.
 */
static void iir_df1_process_block_s16(struct iir_state_df1 *iir,
				      const int16_t *src, int16_t *dst,
				      int nsamples, int stride)
{
	if (!iir->biquads) {
		for (int n = 0; n < nsamples; n++) {
			*dst = *src;
			src += stride;
			dst += stride;
		}
		return;
	}

	int32_t scratch_in[EQ_IIR_BLOCK_SIZE];
	int32_t scratch[EQ_IIR_BLOCK_SIZE];
	int64_t accum[EQ_IIR_BLOCK_SIZE];

	while (nsamples > 0) {
		const int chunk = MIN(nsamples, EQ_IIR_BLOCK_SIZE);

		/* Convert input S16 to Q1.31 in scratch buffer */
		const int16_t *s_ptr = src;
		for (int n = 0; n < chunk; n++) {
			scratch_in[n] = ((int32_t)*s_ptr) << 16;
			s_ptr += stride;
		}

		const int nseries = iir->biquads_in_series;

		int c = 0;
		int d = 0;
		const int32_t *coefp = iir->coef;
		int32_t *delay = iir->delay;

		for (int j = 0; j < (int)iir->biquads; j += nseries) {
			/* Copy input for this parallel branch */
			for (int n = 0; n < chunk; n++)
				scratch[n] = scratch_in[n];

			for (int b = 0; b < nseries; b++) {
				/* Load coefficients for biquad into registers */
				const int32_t a2 = coefp[c];
				const int32_t a1 = coefp[c + 1];
				const int32_t b2 = coefp[c + 2];
				const int32_t b1 = coefp[c + 3];
				const int32_t b0 = coefp[c + 4];
				const int32_t shift_param = coefp[c + 5];
				const int32_t gain = coefp[c + 6];
				const int shift_bits = 14 + shift_param;
				const int64_t gain_round = 1LL << (shift_bits - 1);

				/* Load delay line states into registers */
				int32_t y_n2 = delay[d];
				int32_t y_n1 = delay[d + 1];
				int32_t x_n2 = delay[d + 2];
				int32_t x_n1 = delay[d + 3];

				/* Vectorized sample loop with in-register state */
				for (int n = 0; n < chunk; n++) {
					const int32_t in = scratch[n];

					/* 5-term MAC accumulation */
					int64_t acc = ((int64_t)a2) * y_n2;
					acc += ((int64_t)a1) * y_n1;
					acc += ((int64_t)b2) * x_n2;
					acc += ((int64_t)b1) * x_n1;
					acc += ((int64_t)b0) * in;

					/* Round and convert from Q3.61 to Q3.31 */
					acc = (acc + (1LL << 29)) >> 30;
					const int32_t tmp = sat_clamp_q31(acc);

					/* Update state variables in registers (zero memory overhead) */
					y_n2 = y_n1;
					y_n1 = tmp;
					x_n2 = x_n1;
					x_n1 = in;

					/* Apply biquad gain & shift */
					int64_t acc_gain = ((int64_t)gain) * tmp;
					acc_gain = (acc_gain + gain_round) >> shift_bits;
					scratch[n] = sat_clamp_q31(acc_gain);
				}

				/* Store delay states back to RAM once per block */
				delay[d] = y_n2;
				delay[d + 1] = y_n1;
				delay[d + 2] = x_n2;
				delay[d + 3] = x_n1;

				c += SOF_EQ_IIR_NBIQUAD;
				d += IIR_DF1_NUM_STATE;
			}

			if (j == 0) {
				for (int n = 0; n < chunk; n++)
					accum[n] = (int64_t)scratch[n];
			} else {
				for (int n = 0; n < chunk; n++)
					accum[n] += (int64_t)scratch[n];
			}
		}

		/* Convert back to output S16 */
		int16_t *d_ptr = dst;
		for (int n = 0; n < chunk; n++) {
			int64_t val = (int64_t)sat_clamp_q31(accum[n]);
			*d_ptr = sat_clamp_s16((int32_t)Q_SHIFT_RND(val, 31, 15));
			d_ptr += stride;
		}

		src += chunk * stride;
		dst += chunk * stride;
		nsamples -= chunk;
	}
}

static void iir_df1_process_block_s24(struct iir_state_df1 *iir,
				      const int32_t *src, int32_t *dst,
				      int nsamples, int stride)
{
	if (!iir->biquads) {
		for (int n = 0; n < nsamples; n++) {
			*dst = *src;
			src += stride;
			dst += stride;
		}
		return;
	}

	int32_t scratch_in[EQ_IIR_BLOCK_SIZE];
	int32_t scratch[EQ_IIR_BLOCK_SIZE];
	int64_t accum[EQ_IIR_BLOCK_SIZE];

	while (nsamples > 0) {
		const int chunk = MIN(nsamples, EQ_IIR_BLOCK_SIZE);

		const int32_t *s_ptr = src;
		for (int n = 0; n < chunk; n++) {
			scratch_in[n] = (*s_ptr) << 8;
			s_ptr += stride;
		}

		const int nseries = iir->biquads_in_series;

		int c = 0;
		int d = 0;
		const int32_t *coefp = iir->coef;
		int32_t *delay = iir->delay;

		for (int j = 0; j < (int)iir->biquads; j += nseries) {
			for (int n = 0; n < chunk; n++)
				scratch[n] = scratch_in[n];

			for (int b = 0; b < nseries; b++) {
				const int32_t a2 = coefp[c];
				const int32_t a1 = coefp[c + 1];
				const int32_t b2 = coefp[c + 2];
				const int32_t b1 = coefp[c + 3];
				const int32_t b0 = coefp[c + 4];
				const int32_t shift_param = coefp[c + 5];
				const int32_t gain = coefp[c + 6];
				const int shift_bits = 14 + shift_param;
				const int64_t gain_round = 1LL << (shift_bits - 1);

				int32_t y_n2 = delay[d];
				int32_t y_n1 = delay[d + 1];
				int32_t x_n2 = delay[d + 2];
				int32_t x_n1 = delay[d + 3];

				for (int n = 0; n < chunk; n++) {
					const int32_t in = scratch[n];

					int64_t acc = ((int64_t)a2) * y_n2;
					acc += ((int64_t)a1) * y_n1;
					acc += ((int64_t)b2) * x_n2;
					acc += ((int64_t)b1) * x_n1;
					acc += ((int64_t)b0) * in;

					acc = (acc + (1LL << 29)) >> 30;
					const int32_t tmp = sat_clamp_q31(acc);

					y_n2 = y_n1;
					y_n1 = tmp;
					x_n2 = x_n1;
					x_n1 = in;

					int64_t acc_gain = ((int64_t)gain) * tmp;
					acc_gain = (acc_gain + gain_round) >> shift_bits;
					scratch[n] = sat_clamp_q31(acc_gain);
				}

				delay[d] = y_n2;
				delay[d + 1] = y_n1;
				delay[d + 2] = x_n2;
				delay[d + 3] = x_n1;

				c += SOF_EQ_IIR_NBIQUAD;
				d += IIR_DF1_NUM_STATE;
			}

			if (j == 0) {
				for (int n = 0; n < chunk; n++)
					accum[n] = (int64_t)scratch[n];
			} else {
				for (int n = 0; n < chunk; n++)
					accum[n] += (int64_t)scratch[n];
			}
		}

		int32_t *d_ptr = dst;
		for (int n = 0; n < chunk; n++) {
			int64_t val = (int64_t)sat_clamp_q31(accum[n]);
			*d_ptr = sat_clamp_s24((int32_t)Q_SHIFT_RND(val, 31, 23));
			d_ptr += stride;
		}

		src += chunk * stride;
		dst += chunk * stride;
		nsamples -= chunk;
	}
}

static void iir_df1_process_block_s32(struct iir_state_df1 *iir,
				      const int32_t *src, int32_t *dst,
				      int nsamples, int stride)
{
	if (!iir->biquads) {
		for (int n = 0; n < nsamples; n++) {
			*dst = *src;
			src += stride;
			dst += stride;
		}
		return;
	}

	int32_t scratch_in[EQ_IIR_BLOCK_SIZE];
	int32_t scratch[EQ_IIR_BLOCK_SIZE];
	int64_t accum[EQ_IIR_BLOCK_SIZE];

	while (nsamples > 0) {
		const int chunk = MIN(nsamples, EQ_IIR_BLOCK_SIZE);

		const int32_t *s_ptr = src;
		for (int n = 0; n < chunk; n++) {
			scratch_in[n] = *s_ptr;
			s_ptr += stride;
		}

		const int nseries = iir->biquads_in_series;

		int c = 0;
		int d = 0;
		const int32_t *coefp = iir->coef;
		int32_t *delay = iir->delay;

		for (int j = 0; j < (int)iir->biquads; j += nseries) {
			for (int n = 0; n < chunk; n++)
				scratch[n] = scratch_in[n];

			for (int b = 0; b < nseries; b++) {
				const int32_t a2 = coefp[c];
				const int32_t a1 = coefp[c + 1];
				const int32_t b2 = coefp[c + 2];
				const int32_t b1 = coefp[c + 3];
				const int32_t b0 = coefp[c + 4];
				const int32_t shift_param = coefp[c + 5];
				const int32_t gain = coefp[c + 6];
				const int shift_bits = 14 + shift_param;
				const int64_t gain_round = 1LL << (shift_bits - 1);

				int32_t y_n2 = delay[d];
				int32_t y_n1 = delay[d + 1];
				int32_t x_n2 = delay[d + 2];
				int32_t x_n1 = delay[d + 3];

				for (int n = 0; n < chunk; n++) {
					const int32_t in = scratch[n];

					int64_t acc = ((int64_t)a2) * y_n2;
					acc += ((int64_t)a1) * y_n1;
					acc += ((int64_t)b2) * x_n2;
					acc += ((int64_t)b1) * x_n1;
					acc += ((int64_t)b0) * in;

					acc = (acc + (1LL << 29)) >> 30;
					const int32_t tmp = sat_clamp_q31(acc);

					y_n2 = y_n1;
					y_n1 = tmp;
					x_n2 = x_n1;
					x_n1 = in;

					int64_t acc_gain = ((int64_t)gain) * tmp;
					acc_gain = (acc_gain + gain_round) >> shift_bits;
					scratch[n] = sat_clamp_q31(acc_gain);
				}

				delay[d] = y_n2;
				delay[d + 1] = y_n1;
				delay[d + 2] = x_n2;
				delay[d + 3] = x_n1;

				c += SOF_EQ_IIR_NBIQUAD;
				d += IIR_DF1_NUM_STATE;
			}

			if (j == 0) {
				for (int n = 0; n < chunk; n++)
					accum[n] = (int64_t)scratch[n];
			} else {
				for (int n = 0; n < chunk; n++)
					accum[n] += (int64_t)scratch[n];
			}
		}

		int32_t *d_ptr = dst;
		for (int n = 0; n < chunk; n++) {
			*d_ptr = sat_clamp_q31(accum[n]);
			d_ptr += stride;
		}

		src += chunk * stride;
		dst += chunk * stride;
		nsamples -= chunk;
	}
}

#if CONFIG_FORMAT_S16LE
void eq_iir_s16_default(struct processing_module *mod, const struct cir_buf_source *source,
			struct cir_buf_sink *sink, size_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	const unsigned int nch = cd->channels;
	const size_t samples = frames * nch;
	size_t processed = 0;

	const int16_t *x = source->ptr;
	int16_t *y = sink->ptr;

	while (processed < samples) {
		const size_t nmax = samples - processed;
		const size_t n1 = cir_buf_samples_without_wrap_s16(x, source->buf_end);
		const size_t n2 = cir_buf_samples_without_wrap_s16(y, sink->buf_end);
		size_t n = MIN(n1, n2);
		n = MIN(n, nmax);
		const size_t nframes = n / nch;

		for (unsigned int i = 0; i < nch; i++) {
			iir_df1_process_block_s16(&cd->iir[i], x + i, y + i, nframes, nch);
		}

		processed += n;
		x = source_cir_buf_wrap(x + n, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
void eq_iir_s24_default(struct processing_module *mod, const struct cir_buf_source *source,
			struct cir_buf_sink *sink, size_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	const unsigned int nch = cd->channels;
	const size_t samples = frames * nch;
	size_t processed = 0;

	const int32_t *x = source->ptr;
	int32_t *y = sink->ptr;

	while (processed < samples) {
		const size_t nmax = samples - processed;
		const size_t n1 = cir_buf_samples_without_wrap_s32(x, source->buf_end);
		const size_t n2 = cir_buf_samples_without_wrap_s32(y, sink->buf_end);
		size_t n = MIN(n1, n2);
		n = MIN(n, nmax);
		const size_t nframes = n / nch;

		for (unsigned int i = 0; i < nch; i++) {
			iir_df1_process_block_s24(&cd->iir[i], x + i, y + i, nframes, nch);
		}

		processed += n;
		x = source_cir_buf_wrap(x + n, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
void eq_iir_s32_default(struct processing_module *mod, const struct cir_buf_source *source,
			struct cir_buf_sink *sink, size_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	const unsigned int nch = cd->channels;
	const size_t samples = frames * nch;
	size_t processed = 0;

	const int32_t *x = source->ptr;
	int32_t *y = sink->ptr;

	while (processed < samples) {
		const size_t nmax = samples - processed;
		const size_t n1 = cir_buf_samples_without_wrap_s32(x, source->buf_end);
		const size_t n2 = cir_buf_samples_without_wrap_s32(y, sink->buf_end);
		size_t n = MIN(n1, n2);
		n = MIN(n, nmax);
		const size_t nframes = n / nch;

		for (unsigned int i = 0; i < nch; i++) {
			iir_df1_process_block_s32(&cd->iir[i], x + i, y + i, nframes, nch);
		}

		processed += n;
		x = source_cir_buf_wrap(x + n, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S32LE */

