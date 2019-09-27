// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/eq_fir.h>

#if !CONFIG_FIR_ARCH

#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <user/eq.h>
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
	int16_t *x;
	int16_t *y;
	int32_t z;
	int idx;
	int ch;
	int i;

	for (ch = 0; ch < nch; ch++) {
		filter = &fir[ch];
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = buffer_read_frag_s16(source, idx);
			y = buffer_write_frag_s16(sink, idx);
			z = fir_32x16(filter, *x << 16);
			*y = sat_int16(Q_SHIFT_RND(z, 31, 15));
			idx += nch;
		}
	}
}

void eq_fir_s24(struct fir_state_32x16 fir[], struct comp_buffer *source,
		struct comp_buffer *sink, int frames, int nch)
{
	struct fir_state_32x16 *filter;
	int32_t *x;
	int32_t *y;
	int32_t z;
	int idx;
	int ch;
	int i;

	for (ch = 0; ch < nch; ch++) {
		filter = &fir[ch];
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = buffer_read_frag_s32(source, idx);
			y = buffer_write_frag_s32(sink, idx);
			z = fir_32x16(filter, *x << 8);
			*y = sat_int24(Q_SHIFT_RND(z, 31, 23));
			idx += nch;
		}
	}
}

void eq_fir_s32(struct fir_state_32x16 fir[], struct comp_buffer *source,
		struct comp_buffer *sink, int frames, int nch)
{
	struct fir_state_32x16 *filter;
	int32_t *x;
	int32_t *y;
	int idx;
	int ch;
	int i;

	for (ch = 0; ch < nch; ch++) {
		filter = &fir[ch];
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = buffer_read_frag_s32(source, idx);
			y = buffer_write_frag_s32(sink, idx);
			*y = fir_32x16(filter, *x);
			idx += nch;
		}
	}
}

#endif
