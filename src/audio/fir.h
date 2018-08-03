/*
 * Copyright (c) 2017, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef FIR_H
#define FIR_H

#include "fir_config.h"

#if FIR_GENERIC

#include <sof/audio/format.h>

struct fir_state_32x16 {
	int rwi; /* Circular read and write index */
	int length; /* Number of FIR taps */
	int delay_size; /* Actual delay lentgh, must be >= length */
	int out_shift; /* Amount of right shifts at output */
	int16_t *coef; /* Pointer to FIR coefficients */
	int32_t *delay; /* Pointer to FIR delay line */
};

void fir_reset(struct fir_state_32x16 *fir);

int fir_init_coef(struct fir_state_32x16 *fir, int16_t config[]);

void fir_init_delay(struct fir_state_32x16 *fir, int32_t **data);

void eq_fir_s32(struct fir_state_32x16 fir[], struct comp_buffer *source,
		struct comp_buffer *sink, int frames, int nch);

/* The next functions are inlined to optmize execution speed */

static inline void fir_part_32x16(int64_t *y, int taps, const int16_t c[],
	int *ic, int32_t d[], int *id)
{
	int n;

	/* Data is Q8.24, coef is Q1.15, product is Q9.39 */
	for (n = 0; n < taps; n++) {
		*y += (int64_t)c[*ic] * d[*id];
		(*ic)++;
		(*id)--;
	}
}

static inline int32_t fir_32x16(struct fir_state_32x16 *fir, int32_t x)
{
	int64_t y = 0;
	int n1;
	int n2;
	int i = 0; /* Start from 1st tap */
	int tmp_ri;

	/* Write sample to delay */
	fir->delay[fir->rwi] = x;

	/* Start FIR calculation. Calculate first number of taps possible to
	 * calculate before circular wrap need.
	 */
	n1 = fir->rwi + 1;
	/* Point to newest sample and advance read index */
	tmp_ri = (fir->rwi)++;
	if (fir->rwi == fir->delay_size)
		fir->rwi = 0;

	if (n1 > fir->length) {
		/* No need to un-wrap fir read index, make sure ri
		 * is >= 0 after FIR computation.
		 */
		fir_part_32x16(&y, fir->length, fir->coef, &i, fir->delay,
			&tmp_ri);
	} else {
		n2 = fir->length - n1;
		/* Part 1, loop n1 times, fir_ri becomes -1 */
		fir_part_32x16(&y, n1, fir->coef, &i, fir->delay, &tmp_ri);

		/* Part 2, unwrap fir_ri, continue rest of filter */
		tmp_ri = fir->delay_size - 1;
		fir_part_32x16(&y, n2, fir->coef, &i, fir->delay, &tmp_ri);
	}
	/* Q9.39 -> Q9.24, saturate to Q8.24 */
	y = sat_int32(y >> (15 + fir->out_shift));

	return (int32_t)y;
}

#endif
#endif
