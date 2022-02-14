// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/math/fir_config.h>

#if FIR_GENERIC

#include <sof/common.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/math/fir_generic.h>
#include <user/fir.h>
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

int fir_delay_size(struct sof_fir_coef_data *config)
{
	/* Check for sane FIR length. The generic version does not
	 * have other constraints.
	 */
	if (config->length > SOF_FIR_MAX_LENGTH || config->length < 1)
		return -EINVAL;

	return config->length * sizeof(int32_t);
}

int fir_init_coef(struct fir_state_32x16 *fir,
		  struct sof_fir_coef_data *config)
{
	fir->rwi = 0;
	fir->length = (int)config->length;
	fir->taps = fir->length; /* The same for generic C version */
	fir->out_shift = (int)config->out_shift;
	fir->coef = ASSUME_ALIGNED(&config->coef[0], 4);
	return 0;
}

void fir_init_delay(struct fir_state_32x16 *fir, int32_t **data)
{
	fir->delay = *data;
	*data += fir->length; /* Point to next delay line start */
}

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

#endif
