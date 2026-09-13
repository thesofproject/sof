// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Sound Open Firmware. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/math/fir_config.h>
#include <sof/common.h>

#if SOF_USE_ARM_SIMD(FILTER)

#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/arm_simd.h>
#include <sof/math/fir_generic.h>
#include <user/fir.h>
#include <rtos/symbol.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/*
 * EQ FIR ARMv7E-M DSP accelerated code
 */

void fir_reset(struct fir_state_32x16 *fir)
{
	fir->rwi = 0;
	fir->length = 0;
	fir->out_shift = 0;
	fir->coef = NULL;
}
EXPORT_SYMBOL(fir_reset);

int fir_delay_size(struct sof_fir_coef_data *config)
{
	if (config->length > SOF_FIR_MAX_LENGTH || config->length < 4)
		return -EINVAL;

	if (config->length & 0x3)
		return -EINVAL;

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
	*data += fir->length;
}
EXPORT_SYMBOL(fir_init_delay);

int32_t fir_32x16(struct fir_state_32x16 *fir, int32_t x)
{
	int64_t y = 0;
	int32_t *data = &fir->delay[fir->rwi];
	const int16_t *coef = &fir->coef[0];
	int n1, n2, n;
	const int length = fir->length;
	const int taps = fir->taps;
	const int shift = 15 + fir->out_shift;

	if (!fir->length)
		return x;

	*data = x;

	n1 = ++fir->rwi;
	if (fir->rwi == length)
		fir->rwi = 0;

	/* Part 1: before circular wrap */
	n1 = MIN(n1, taps);
	int n1_4 = n1 & ~3;
	for (n = 0; n < n1_4; n += 4) {
		uint32_t c01 = *(const uint32_t *)coef;
		uint32_t c23 = *(const uint32_t *)(coef + 2);
		coef += 4;

		int16_t c0 = (int16_t)c01;
		int16_t c1 = (int16_t)(c01 >> 16);
		int16_t c2 = (int16_t)c23;
		int16_t c3 = (int16_t)(c23 >> 16);

		int32_t d0 = data[0];
		int32_t d1 = data[-1];
		int32_t d2 = data[-2];
		int32_t d3 = data[-3];
		data -= 4;

		y = arm_smlal(y, c0, d0);
		y = arm_smlal(y, c1, d1);
		y = arm_smlal(y, c2, d2);
		y = arm_smlal(y, c3, d3);
	}
	for (; n < n1; n++) {
		y = arm_smlal(y, *coef++, *data--);
	}

	/* Part 2: un-wrap data, continue n2 times */
	n2 = taps - n1;
	data = &fir->delay[length - 1];
	int n2_4 = n2 & ~3;
	for (n = 0; n < n2_4; n += 4) {
		uint32_t c01 = *(const uint32_t *)coef;
		uint32_t c23 = *(const uint32_t *)(coef + 2);
		coef += 4;

		int16_t c0 = (int16_t)c01;
		int16_t c1 = (int16_t)(c01 >> 16);
		int16_t c2 = (int16_t)c23;
		int16_t c3 = (int16_t)(c23 >> 16);

		int32_t d0 = data[0];
		int32_t d1 = data[-1];
		int32_t d2 = data[-2];
		int32_t d3 = data[-3];
		data -= 4;

		y = arm_smlal(y, c0, d0);
		y = arm_smlal(y, c1, d1);
		y = arm_smlal(y, c2, d2);
		y = arm_smlal(y, c3, d3);
	}
	for (; n < n2; n++) {
		y = arm_smlal(y, *coef++, *data--);
	}

	return arm_sat_q31(y >> shift);
}
EXPORT_SYMBOL(fir_32x16);

void fir_32x16_2x(struct fir_state_32x16 *fir, int32_t x0, int32_t x1, int32_t *y0, int32_t *y1)
{
	int64_t a0 = 0;
	int64_t a1 = 0;
	int32_t sample1;
	int32_t *data = &fir->delay[fir->rwi];
	const int16_t *coef = &fir->coef[0];
	int n1, n2, i;
	const int length = fir->length;
	const int taps = fir->taps;
	const int shift = 15 + fir->out_shift;

	if (!fir->taps) {
		*y0 = x0;
		*y1 = x1;
		return;
	}

	*data = x0;
	*(data + 1) = x1;

	n1 = fir->rwi + 1;
	fir->rwi += 2;
	if (fir->rwi >= length)
		fir->rwi -= length;

	/* Part 1: before circular wrap */
	sample1 = x1;
	n1 = MIN(n1, taps);
	int n1_4 = n1 & ~3;
	for (i = 0; i < n1_4; i += 4) {
		uint32_t c01 = *(const uint32_t *)coef;
		uint32_t c23 = *(const uint32_t *)(coef + 2);
		coef += 4;

		int16_t c0 = (int16_t)c01;
		int16_t c1 = (int16_t)(c01 >> 16);
		int16_t c2 = (int16_t)c23;
		int16_t c3 = (int16_t)(c23 >> 16);

		int32_t d0 = data[0];
		int32_t d1 = data[-1];
		int32_t d2 = data[-2];
		int32_t d3 = data[-3];
		data -= 4;

		a1 = arm_smlal(a1, c0, sample1);
		a0 = arm_smlal(a0, c0, d0);

		a1 = arm_smlal(a1, c1, d0);
		a0 = arm_smlal(a0, c1, d1);

		a1 = arm_smlal(a1, c2, d1);
		a0 = arm_smlal(a0, c2, d2);

		a1 = arm_smlal(a1, c3, d2);
		a0 = arm_smlal(a0, c3, d3);

		sample1 = d3;
	}
	for (; i < n1; i++) {
		int16_t tap = *coef++;
		int32_t sample0 = *data--;
		a1 = arm_smlal(a1, tap, sample1);
		a0 = arm_smlal(a0, tap, sample0);
		sample1 = sample0;
	}

	/* Part 2: un-wrap data, continue n2 times */
	n2 = taps - n1;
	data = &fir->delay[length - 1];
	int n2_4 = n2 & ~3;
	for (i = 0; i < n2_4; i += 4) {
		uint32_t c01 = *(const uint32_t *)coef;
		uint32_t c23 = *(const uint32_t *)(coef + 2);
		coef += 4;

		int16_t c0 = (int16_t)c01;
		int16_t c1 = (int16_t)(c01 >> 16);
		int16_t c2 = (int16_t)c23;
		int16_t c3 = (int16_t)(c23 >> 16);

		int32_t d0 = data[0];
		int32_t d1 = data[-1];
		int32_t d2 = data[-2];
		int32_t d3 = data[-3];
		data -= 4;

		a1 = arm_smlal(a1, c0, sample1);
		a0 = arm_smlal(a0, c0, d0);

		a1 = arm_smlal(a1, c1, d0);
		a0 = arm_smlal(a0, c1, d1);

		a1 = arm_smlal(a1, c2, d1);
		a0 = arm_smlal(a0, c2, d2);

		a1 = arm_smlal(a1, c3, d2);
		a0 = arm_smlal(a0, c3, d3);

		sample1 = d3;
	}
	for (; i < n2; i++) {
		int16_t tap = *coef++;
		int32_t sample0 = *data--;
		a1 = arm_smlal(a1, tap, sample1);
		a0 = arm_smlal(a0, tap, sample0);
		sample1 = sample0;
	}

	*y0 = arm_sat_q31(a0 >> shift);
	*y1 = arm_sat_q31(a1 >> shift);
}
EXPORT_SYMBOL(fir_32x16_2x);

#endif /* SOF_USE_ARM_SIMD(FILTER) */
