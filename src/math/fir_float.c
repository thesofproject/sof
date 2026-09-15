// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Sound Open Firmware. All rights reserved.

#include <sof/math/fir_config.h>
#include <sof/common.h>
#include <sof/math/fir_float.h>
#include <rtos/symbol.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

void fir_reset_float(struct fir_state_float *fir)
{
	fir->rwi = 0;
	fir->taps = 0;
	fir->length = 0;
	fir->out_shift = 0;
	fir->coef = NULL;
}
EXPORT_SYMBOL(fir_reset_float);

int fir_delay_size_float(struct sof_fir_coef_data *config)
{
	if (config->length > SOF_FIR_MAX_LENGTH || config->length < 4)
		return -EINVAL;

	if (config->length & 0x3)
		return -EINVAL;

	return (config->length + 4) * sizeof(float);
}
EXPORT_SYMBOL(fir_delay_size_float);

int fir_coef_size_float(struct sof_fir_coef_data *config)
{
	if (config->length > SOF_FIR_MAX_LENGTH || config->length < 4)
		return -EINVAL;

	return config->length * sizeof(float);
}
EXPORT_SYMBOL(fir_coef_size_float);

int fir_init_coef_float(struct fir_state_float *fir,
			struct sof_fir_coef_data *config,
			float **coef_storage)
{
	fir->rwi = 0;
	fir->taps = (int)config->length;
	fir->length = (int)fir->taps + 2;
	fir->out_shift = 0;

	if (coef_storage && *coef_storage) {
		float scale;
		int shift = config->out_shift;
		int i;

		fir->coef = *coef_storage;
		*coef_storage += fir->taps;

		if (shift >= 0 && shift <= 30)
			scale = (1.0f / 32768.0f) / (float)(1 << shift);
		else if (shift < 0 && shift >= -30)
			scale = (1.0f / 32768.0f) * (float)(1 << (-shift));
		else
			scale = 1.0f / 32768.0f;

		for (i = 0; i < fir->taps; i++)
			fir->coef[i] = (float)config->coef[i] * scale;
	}

	return 0;
}
EXPORT_SYMBOL(fir_init_coef_float);

void fir_init_delay_float(struct fir_state_float *fir, float **data)
{
	fir->delay = *data;
	*data += fir->length;
}
EXPORT_SYMBOL(fir_init_delay_float);

float fir_float(struct fir_state_float *fir, float x)
{
	float *data = &fir->delay[fir->rwi];
	const float *coef = &fir->coef[0];
	float y = 0.0f;
	int n1, n2, n;
	const int length = fir->length;
	const int taps = fir->taps;

	if (!fir->length)
		return x;

	*data = x;

	n1 = ++fir->rwi;
	if (fir->rwi == length)
		fir->rwi = 0;

	n1 = MIN(n1, taps);
	for (n = 0; n < n1; n++) {
		y += (*coef) * (*data);
		coef++;
		data--;
	}

	n2 = taps - n1;
	data = &fir->delay[length - 1];
	for (n = 0; n < n2; n++) {
		y += (*coef) * (*data);
		coef++;
		data--;
	}

	return y;
}
EXPORT_SYMBOL(fir_float);

void fir_float_2x(struct fir_state_float *fir, float x0, float x1, float *y0, float *y1)
{
	float a0 = 0.0f;
	float a1 = 0.0f;
	float sample0;
	float sample1;
	float tap;
	float *data = &fir->delay[fir->rwi];
	const float *coef = &fir->coef[0];
	int n1, n2, i;
	const int length = fir->length;
	const int taps = fir->taps;

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

	sample1 = x1;
	n1 = MIN(n1, taps);

	for (i = 0; i < n1; i++) {
		tap = *coef++;
		sample0 = *data--;
		a1 += tap * sample1;
		a0 += tap * sample0;
		sample1 = sample0;
	}

	n2 = taps - n1;
	data = &fir->delay[length - 1];
	for (i = 0; i < n2; i++) {
		tap = *coef++;
		sample0 = *data--;
		a1 += tap * sample1;
		a0 += tap * sample0;
		sample1 = sample0;
	}

	*y0 = a0;
	*y1 = a1;
}
EXPORT_SYMBOL(fir_float_2x);
