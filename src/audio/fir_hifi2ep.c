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
 */

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <uapi/eq.h>
#include "fir_config.h"

#if FIR_HIFIEP

#include <xtensa/config/defs.h>
#include <xtensa/tie/xt_hifi2.h>
#include "fir_hifi2ep.h"

/*
 * EQ FIR algorithm code
 */

void fir_reset(struct fir_state_32x16 *fir)
{
	fir->mute = 1;
	fir->length = 0;
	fir->out_shift = 0;
	fir->rwp = NULL;
	fir->delay = NULL;
	fir->delay_end = NULL;
	fir->coef = NULL;
	/* There may need to know the beginning of dynamic allocation after
	 * reset so omitting setting also fir->delay to NULL.
	 */
}

int fir_init_coef(struct fir_state_32x16 *fir, int16_t config[])
{
	struct sof_eq_fir_coef_data *setup;

	/* The length is taps plus two since the filter computes two
	 * samples per call. Length plus one would be minimum but the add
	 * must be even. The even length is needed for 64 bit loads from delay
	 * lines with 32 bit samples.
	 */
	setup = (struct sof_eq_fir_coef_data *)config;
	fir->mute = 0;
	fir->rwp = NULL;
	fir->taps = (int)setup->length;
	fir->length = fir->taps + 2;
	fir->out_shift = (int)setup->out_shift;
	fir->coef = (ae_p16x2s *)&setup->coef[0];
	fir->delay = NULL;
	fir->delay_end = NULL;

	/* Check FIR tap count for implementation specific constraints */
	if (fir->taps > SOF_EQ_FIR_MAX_LENGTH || fir->taps < 4)
		return -EINVAL;

	if (fir->taps & 3)
		return -EINVAL;

	return fir->length;
}

void fir_init_delay(struct fir_state_32x16 *fir, int32_t **data)
{
	fir->delay = (ae_p24f *) *data;
	fir->delay_end = fir->delay + fir->length;
	fir->rwp = (ae_p24x2f *)(fir->delay + fir->length - 1);
	*data += fir->length; /* Point to next delay line start */
}

void fir_get_lrshifts(struct fir_state_32x16 *fir, int *lshift,
		      int *rshift)
{
	if (fir->mute) {
		*lshift = 0;
		*rshift = 31;
	} else {
		*lshift = (fir->out_shift < 0) ? -fir->out_shift : 0;
		*rshift = (fir->out_shift > 0) ? fir->out_shift : 0;
	}
}

/* For even frame lengths use FIR filter that processes two sequential
 * sample per call.
 */
void eq_fir_2x_s32_hifiep(struct fir_state_32x16 fir[],
			  struct comp_buffer *source,
			  struct comp_buffer *sink,
			  int frames, int nch)
{
	struct fir_state_32x16 *f;
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *snk = (int32_t *)sink->w_ptr;
	int32_t *x0;
	int32_t *y0;
	int32_t *x1;
	int32_t *y1;
	int ch;
	int i;
	int rshift;
	int lshift;
	int inc = nch << 1;

	for (ch = 0; ch < nch; ch++) {
		/* Get FIR instance and get shifts to e.g. apply mute
		 * without overhead.
		 */
		f = &fir[ch];
		fir_get_lrshifts(f, &lshift, &rshift);

		/* Setup circular buffer for FIR input data delay */
		fir_hifiep_setup_circular(f);

		x0 = src++;
		y0 = snk++;
		for (i = 0; i < (frames >> 1); i++) {
			x1 = x0 + nch;
			y1 = y0 + nch;
			fir_32x16_2x_hifiep(f, x0, x1, y0, y1, lshift, rshift);
			x0 += inc;
			y0 += inc;
		}
	}
}

/* FIR for any number of frames */
void eq_fir_s32_hifiep(struct fir_state_32x16 fir[], struct comp_buffer *source,
		       struct comp_buffer *sink, int frames, int nch)
{
	struct fir_state_32x16 *f;
	int32_t *src = (int32_t *)source->r_ptr;
	int32_t *snk = (int32_t *)sink->w_ptr;
	int32_t *x;
	int32_t *y;
	int ch;
	int i;
	int rshift;
	int lshift;

	for (ch = 0; ch < nch; ch++) {
		/* Get FIR instance and get shifts to e.g. apply mute
		 * without overhead.
		 */
		f = &fir[ch];
		fir_get_lrshifts(f, &lshift, &rshift);

		/* Setup circular buffer for FIR input data delay */
		fir_hifiep_setup_circular(f);

		x = src++;
		y = snk++;
		for (i = 0; i < frames; i++) {
			fir_32x16_hifiep(f, x, y, lshift, rshift);
			x += nch;
			y += nch;
		}
	}
}

#endif
