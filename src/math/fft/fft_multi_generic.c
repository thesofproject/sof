// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025-2026 Intel Corporation.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/audio/format.h>
#include <sof/math/icomplex32.h>
#include <sof/common.h>
#include <sof/math/fft.h>
#include <string.h>

#ifdef DEBUG_DUMP_TO_FILE
#include <stdio.h>
#endif

/* Twiddle factor tables defined in fft_multi.c via twiddle_3072_32.h */
#define FFT_MULTI_TWIDDLE_SIZE 2048
extern const int32_t multi_twiddle_real_32[];
extern const int32_t multi_twiddle_imag_32[];

/* Constants for size 3 DFT */
#define DFT3_COEFR -1073741824 /* int32(-0.5 * 2^31) */
#define DFT3_COEFI 1859775393  /* int32(sqrt(3) / 2 * 2^31) */
#define DFT3_SCALE 715827883   /* int32(1/3*2^31) */

#ifdef FFT_GENERIC

void dft3_32(struct icomplex32 *x_in, struct icomplex32 *y)
{
	const struct icomplex32 c0 = {DFT3_COEFR, -DFT3_COEFI};
	const struct icomplex32 c1 = {DFT3_COEFR, DFT3_COEFI};
	struct icomplex32 x[3];
	struct icomplex32 p1, p2, sum;
	int i;

	for (i = 0; i < 3; i++) {
		x[i].real = Q_MULTSR_32X32((int64_t)x_in[i].real, DFT3_SCALE, 31, 31, 31);
		x[i].imag = Q_MULTSR_32X32((int64_t)x_in[i].imag, DFT3_SCALE, 31, 31, 31);
	}

	/*
	 *      | 1   1   1 |
	 * c =  | 1  c0  c1 | , x = [ x0 x1 x2 ]
	 *      | 1  c1  c0 |
	 *
	 * y(0) = c(0,0) * x(0) + c(1,0) * x(1) + c(2,0) * x(2)
	 * y(1) = c(0,1) * x(0) + c(1,1) * x(1) + c(2,1) * x(2)
	 * y(2) = c(0,2) * x(0) + c(1,2) * x(1) + c(2,2) * x(2)
	 */

	/* y(0) = 1 * x(0) + 1 * x(1) + 1 * x(2) */
	icomplex32_adds(&x[0], &x[1], &sum);
	icomplex32_adds(&x[2], &sum, &y[0]);

	/* y(1) = 1 * x(0) + c0 * x(1) + c1 * x(2) */
	icomplex32_mul(&c0, &x[1], &p1);
	icomplex32_mul(&c1, &x[2], &p2);
	icomplex32_adds(&p1, &p2, &sum);
	icomplex32_adds(&x[0], &sum, &y[1]);

	/* y(2) = 1 * x(0) + c1 * x(1) + c0 * x(2) */
	icomplex32_mul(&c1, &x[1], &p1);
	icomplex32_mul(&c0, &x[2], &p2);
	icomplex32_adds(&p1, &p2, &sum);
	icomplex32_adds(&x[0], &sum, &y[2]);
}

void fft_multi_execute_32(struct fft_multi_plan *plan, bool ifft)
{
	struct icomplex32 x[FFT_MULTI_COUNT_MAX];
	struct icomplex32 y[FFT_MULTI_COUNT_MAX];
	struct icomplex32 t, c;
	int i, j, k, m;

	/* Handle 2^N FFT */
	if (plan->num_ffts == 1) {
		memset(plan->outb32, 0, plan->fft_size * sizeof(struct icomplex32));
		fft_execute_32(plan->fft_plan[0], ifft);
		return;
	}

#ifdef DEBUG_DUMP_TO_FILE
	FILE *fh1 = fopen("debug_fft_multi_int1.txt", "w");
	FILE *fh2 = fopen("debug_fft_multi_int2.txt", "w");
	FILE *fh3 = fopen("debug_fft_multi_twiddle.txt", "w");
	FILE *fh4 = fopen("debug_fft_multi_dft_out.txt", "w");
#endif

	/* convert to complex conjugate for IFFT */
	if (ifft) {
		for (i = 0; i < plan->total_size; i++)
			icomplex32_conj(&plan->inb32[i]);
	}

	/* Copy input buffers */
	k = 0;
	for (i = 0; i < plan->fft_size; i++)
		for (j = 0; j < plan->num_ffts; j++)
			plan->tmp_i32[j][i] = plan->inb32[k++];

	/* Clear output buffers and call individual FFTs*/
	for (j = 0; j < plan->num_ffts; j++) {
		memset(&plan->tmp_o32[j][0], 0, plan->fft_size * sizeof(struct icomplex32));
		fft_execute_32(plan->fft_plan[j], 0);
	}

#ifdef DEBUG_DUMP_TO_FILE
	for (j = 0; j < plan->num_ffts; j++)
		for (i = 0; i < plan->fft_size; i++)
			fprintf(fh1, "%d %d\n", plan->tmp_o32[j][i].real, plan->tmp_o32[j][i].imag);
#endif

	/* Multiply with twiddle factors */
	m = FFT_MULTI_TWIDDLE_SIZE / 2 / plan->fft_size;
	for (j = 1; j < plan->num_ffts; j++) {
		for (i = 0; i < plan->fft_size; i++) {
			c = plan->tmp_o32[j][i];
			k = j * i * m;
			t.real = multi_twiddle_real_32[k];
			t.imag = multi_twiddle_imag_32[k];
			// fprintf(fh3, "%d %d\n", t.real, t.imag);
			icomplex32_mul(&t, &c, &plan->tmp_o32[j][i]);
		}
	}

#ifdef DEBUG_DUMP_TO_FILE
	for (j = 0; j < plan->num_ffts; j++)
		for (i = 0; i < plan->fft_size; i++)
			fprintf(fh2, "%d %d\n", plan->tmp_o32[j][i].real, plan->tmp_o32[j][i].imag);
#endif

	/* DFT of size 3 */
	j = plan->fft_size;
	k = 2 * plan->fft_size;
	for (i = 0; i < plan->fft_size; i++) {
		x[0] = plan->tmp_o32[0][i];
		x[1] = plan->tmp_o32[1][i];
		x[2] = plan->tmp_o32[2][i];
		dft3_32(x, y);
		plan->outb32[i] = y[0];
		plan->outb32[i + j] = y[1];
		plan->outb32[i + k] = y[2];
	}

#ifdef DEBUG_DUMP_TO_FILE
	for (i = 0; i < plan->total_size; i++)
		fprintf(fh4, "%d %d\n", plan->outb32[i].real, plan->outb32[i].imag);
#endif

	/* shift back for IFFT */

	/* TODO: Check if time shift method for IFFT is more efficient or more accurate
	 * tmp = 1 / N * fft(X);
	 * x = tmp([1 N:-1:2])
	 */
	if (ifft) {
		/*
		 * no need to divide N as it is already done in the input side
		 * for Q1.31 format. Instead, we need to multiply N to compensate
		 * the shrink we did in the FFT transform.
		 */
		for (i = 0; i < plan->total_size; i++) {
			/* Need to negate imag part to match reference */
			plan->outb32[i].imag = -plan->outb32[i].imag;
			icomplex32_shift(&plan->outb32[i], plan->fft_plan[0]->len,
					 &plan->outb32[i]);
			plan->outb32[i].real = sat_int32((int64_t)plan->outb32[i].real * 3);
			plan->outb32[i].imag = sat_int32((int64_t)plan->outb32[i].imag * 3);
		}
	}

#ifdef DEBUG_DUMP_TO_FILE
	fclose(fh1);
	fclose(fh2);
	fclose(fh3);
	fclose(fh4);
#endif
}

#endif /* FFT_GENERIC */
