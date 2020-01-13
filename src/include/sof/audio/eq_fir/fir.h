/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_AUDIO_EQ_FIR_FIR_H__
#define __SOF_AUDIO_EQ_FIR_FIR_H__

#include <sof/audio/eq_fir/fir_config.h>

#if FIR_GENERIC

#include <sof/audio/format.h>
#include <stdint.h>

struct comp_buffer;
struct sof_eq_fir_coef_data;

struct fir_state_32x16 {
	int rwi; /* Circular read and write index */
	int taps; /* Number of FIR taps */
	int length; /* Number of FIR taps */
	int out_shift; /* Amount of right shifts at output */
	int16_t *coef; /* Pointer to FIR coefficients */
	int32_t *delay; /* Pointer to FIR delay line */
};

void fir_reset(struct fir_state_32x16 *fir);

int fir_delay_size(struct sof_eq_fir_coef_data *config);

int fir_init_coef(struct fir_state_32x16 *fir,
		  struct sof_eq_fir_coef_data *config);

void fir_init_delay(struct fir_state_32x16 *fir, int32_t **data);

#if CONFIG_FORMAT_S16LE
void eq_fir_s16(struct fir_state_32x16 *fir, const struct audio_stream *source,
		struct audio_stream *sink, int frames, int nch);
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
void eq_fir_s24(struct fir_state_32x16 *fir, const struct audio_stream *source,
		struct audio_stream *sink, int frames, int nch);
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
void eq_fir_s32(struct fir_state_32x16 *fir, const struct audio_stream *source,
		struct audio_stream *sink, int frames, int nch);
#endif /* CONFIG_FORMAT_S32LE */

/* The next functions are inlined to optmize execution speed */

static inline int32_t fir_32x16(struct fir_state_32x16 *fir, int32_t x)
{
	int64_t y = 0;
	int32_t *data = &fir->delay[fir->rwi];
	int16_t *coef = &fir->coef[0];
	int n1;
	int n2;
	int n;

	/* Bypass is set with length set to zero. */
	if (!fir->length)
		return x;

	/* Write sample to delay */
	*data = x;

	/* Advance write pointer and calculate into n1 max. number of taps
	 * to process before circular wrap.
	 */
	n1 = ++fir->rwi;
	if (fir->rwi == fir->length)
		fir->rwi = 0;

	/* Check if no need to un-wrap FIR data. */
	if (n1 > fir->length) {
		/* Data is Q1.31, coef is Q1.15, product is Q2.46 */
		for (n = 0; n < fir->length; n++) {
			y += (int64_t)(*coef) * (*data);
			coef++;
			data--;
		}

		/* Q2.46 -> Q2.31, saturate to Q1.31 */
		return sat_int32(y >> (15 + fir->out_shift));
	}

	/* Part 1, loop n1 times */
	for (n = 0; n < n1; n++) {
		y += (int64_t)(*coef) * (*data);
		coef++;
		data--;
	}

	/* Part 2, un-wrap data, continue n2 times */
	n2 = fir->length - n1;
	data = &fir->delay[fir->length - 1];
	for (n = 0; n < n2; n++) {
		y += (int64_t)(*coef) * (*data);
		coef++;
		data--;
	}

	/* Q2.46 -> Q2.31, saturate to Q1.31 */
	return sat_int32(y >> (15 + fir->out_shift));
}

#endif
#endif /* __SOF_AUDIO_EQ_FIR_FIR_H__ */
