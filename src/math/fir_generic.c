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

#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/math/fir_generic.h>
#include <user/fir.h>
#include <rtos/symbol.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/*
 * EQ FIR algorithm code
 */

void fir_reset(struct fir_state_32x16 *fir)
{
	fir->rwi = 0;
	fir->length = 0;
	fir->out_shift = 0;
	fir->coef = NULL;
	/* There may need to know the beginning of dynamic allocation after
	 * reset so omitting setting also fir->delay to NULL.
	 */
}
EXPORT_SYMBOL(fir_reset);

int fir_delay_size(struct sof_fir_coef_data *config)
{
	/* Check FIR tap count for implementation specific constraints */
	if (config->length > SOF_FIR_MAX_LENGTH || config->length < 4)
		return -EINVAL;

	/* The optimization requires the tap count to be multiple of four */
	if (config->length & 0x3)
		return -EINVAL;

	/* The dual sample version needs one more delay entry. To preserve
	 * align for 64 bits need to add two.
	 */
	return (config->length + 4) * sizeof(int32_t);
}
EXPORT_SYMBOL(fir_delay_size);

int fir_init_coef(struct fir_state_32x16 *fir,
		  struct sof_fir_coef_data *config)
{
	fir->rwi = 0;
	fir->taps = (int)config->length;
	fir->length = (int)fir->taps + 2;
	fir->out_shift = (int)config->out_shift;
	fir->coef = ASSUME_ALIGNED(&config->coef[0], 4);
	return 0;
}
EXPORT_SYMBOL(fir_init_coef);

void fir_init_delay(struct fir_state_32x16 *fir, int32_t **data)
{
	fir->delay = *data;
	*data += fir->length; /* Point to next delay line start */
}
EXPORT_SYMBOL(fir_init_delay);

int32_t fir_32x16(struct fir_state_32x16 *fir, int32_t x)
{
	int64_t y = 0;
	int32_t *data = &fir->delay[fir->rwi];
	int16_t *coef = &fir->coef[0];
	int n1;
	int n2;
	int n;
	const int length = fir->length;
	const int taps = fir->taps;
	const int shift = 15 + fir->out_shift;

	/* Bypass is set with length set to zero. */
	if (!fir->length)
		return x;

	/* Write sample to delay */
	*data = x;

	/* Advance write pointer and calculate into n1 max. number of taps
	 * to process before circular wrap.
	 */
	n1 = ++fir->rwi;
	if (fir->rwi == length)
		fir->rwi = 0;

	/* Part 1, loop n1 times */
	n1 = MIN(n1, taps);
	for (n = 0; n < n1; n++) {
		y += (int64_t)(*coef) * (*data);
		coef++;
		data--;
	}

	/* Part 2, un-wrap data, continue n2 times */
	n2 = taps - n1;
	data = &fir->delay[length - 1];
	for (n = 0; n < n2; n++) {
		y += (int64_t)(*coef) * (*data);
		coef++;
		data--;
	}

	/* Q2.46 -> Q2.31, saturate to Q1.31 */
	return sat_int32(y >> shift);
}

void fir_32x16_2x(struct fir_state_32x16 *fir, int32_t x0, int32_t x1, int32_t *y0, int32_t *y1)
{
	int64_t a0 = 0;
	int64_t a1 = 0;
	int32_t sample0;
	int32_t sample1;
	int16_t tap;
	int32_t *data = &fir->delay[fir->rwi];
	int16_t *coef = &fir->coef[0];
	int n1;
	int n2;
	int i;
	const int length = fir->length;
	const int taps = fir->taps;
	const int shift = 15 + fir->out_shift;

	/* Bypass is set with length set to zero. */
	if (!fir->taps) {
		*y0 = x0;
		*y1 = x1;
		return;
	}

	/* Write samples to delay */
	*data = x0;
	*(data + 1) = x1;

	/* Advance write pointer and calculate into n1 max. number of taps
	 * to process before circular wrap.
	 */
	n1 = fir->rwi + 1;
	fir->rwi += 2;
	if (fir->rwi >= length)
		fir->rwi -= length;

	/* Part 1, loop n1 times */
	sample1 = x1;
	n1 = MIN(n1, taps);
	for (i = 0; i < n1; i++) {
		tap = *coef;
		coef++;
		sample0 = *data;
		data--;
		a1 += (int64_t)tap * sample1;
		a0 += (int64_t)tap * sample0;
		sample1 = sample0;
	}

	/* Part 2, un-wrap data, continue n2 times */
	n2 = taps - n1;
	data = &fir->delay[length - 1];
	for (i = 0; i < n2; i++) {
		tap = *coef;
		coef++;
		sample0 = *data;
		data--;
		a1 += (int64_t)tap * sample1;
		a0 += (int64_t)tap * sample0;
		sample1 = sample0;
	}

	/* Q2.46 -> Q2.31, saturate to Q1.31 */
	*y0 = sat_int32(a0 >> shift);
	*y1 = sat_int32(a1 >> shift);
}

#endif
