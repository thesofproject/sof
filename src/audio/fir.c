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

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <uapi/user/eq.h>
#include "fir_config.h"

#if FIR_GENERIC

#include "fir.h"

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

size_t fir_init_coef(struct fir_state_32x16 *fir,
		     struct sof_eq_fir_coef_data *config)
{
	fir->rwi = 0;
	fir->length = (int)config->length;
	fir->out_shift = (int)config->out_shift;
	fir->coef = &config->coef[0];
	fir->delay = NULL;

	/* Check for sane FIR length. The length is constrained to be a
	 * multiple of 4 for optimized code.
	 */
	if (fir->length > SOF_EQ_FIR_MAX_LENGTH || fir->length < 1)
		return -EINVAL;

	return fir->length * sizeof(int32_t);
}

void fir_init_delay(struct fir_state_32x16 *fir, int32_t **data)
{
	fir->delay = *data;
	*data += fir->length; /* Point to next delay line start */
}

void eq_fir_s16(struct fir_state_32x16 fir[], struct comp_buffer *source,
		struct comp_buffer *sink, int frames, int nch)
{
	struct fir_state_32x16 *filter;
	int16_t *src = (int16_t *)source->r_ptr;
	int16_t *snk = (int16_t *)sink->w_ptr;
	int16_t *x;
	int16_t *y;
	int32_t z;
	int ch;
	int i;

	for (ch = 0; ch < nch; ch++) {
		filter = &fir[ch];
		x = src++;
		y = snk++;
		for (i = 0; i < frames; i++) {
			z = fir_32x16(filter, *x << 16);
			*y = sat_int16(Q_SHIFT_RND(z, 31, 15));
			x += nch;
			y += nch;
		}
	}
}

void eq_fir_s24(struct fir_state_32x16 fir[], struct comp_buffer *source,
		struct comp_buffer *sink, int frames, int nch)
{
	struct fir_state_32x16 *filter;
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *snk = (int32_t *)sink->w_ptr;
	int32_t *x;
	int32_t *y;
	int32_t z;
	int ch;
	int i;

	for (ch = 0; ch < nch; ch++) {
		filter = &fir[ch];
		x = src++;
		y = snk++;
		for (i = 0; i < frames; i++) {
			z = fir_32x16(filter, *x << 8);
			*y = sat_int24(Q_SHIFT_RND(z, 31, 23));
			x += nch;
			y += nch;
		}
	}
}

void eq_fir_s32(struct fir_state_32x16 fir[], struct comp_buffer *source,
		struct comp_buffer *sink, int frames, int nch)
{
	struct fir_state_32x16 *filter;
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *snk = (int32_t *)sink->w_ptr;
	int32_t *x;
	int32_t *y;
	int ch;
	int i;

	for (ch = 0; ch < nch; ch++) {
		filter = &fir[ch];
		x = src++;
		y = snk++;
		for (i = 0; i < frames; i++) {
			*y = fir_32x16(filter, *x);
			x += nch;
			y += nch;
		}
	}
}

#endif
