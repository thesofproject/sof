/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_AUDIO_EQ_FIR_H__
#define __SOF_AUDIO_EQ_FIR_H__

#include <arch/audio/eq_fir/eq_fir.h>
#include <sof/platform.h>
#include <ipc/stream.h>
#include <stddef.h>

struct comp_buffer;
struct comp_data;
struct sof_eq_fir_config;

void set_s16_fir(struct comp_data *cd);

void set_s24_fir(struct comp_data *cd);

void set_s32_fir(struct comp_data *cd);

#if !CONFIG_FIR_ARCH

#include <sof/audio/format.h>
#include <stdint.h>

struct sof_eq_fir_coef_data;

struct fir_state_32x16 {
	int rwi; /* Circular read and write index */
	int length; /* Number of FIR taps */
	int out_shift; /* Amount of right shifts at output */
	int16_t *coef; /* Pointer to FIR coefficients */
	int32_t *delay; /* Pointer to FIR delay line */
};

void fir_reset(struct fir_state_32x16 *fir);

size_t fir_init_coef(struct fir_state_32x16 *fir,
		     struct sof_eq_fir_coef_data *config);

void fir_init_delay(struct fir_state_32x16 *fir, int32_t **data);

void eq_fir_s16(struct fir_state_32x16 *fir, struct comp_buffer *source,
		struct comp_buffer *sink, int frames, int nch);

void eq_fir_s24(struct fir_state_32x16 *fir, struct comp_buffer *source,
		struct comp_buffer *sink, int frames, int nch);

void eq_fir_s32(struct fir_state_32x16 *fir, struct comp_buffer *source,
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

	/* Bypass is set with length set to zero. */
	if (!fir->length)
		return x;

	/* Write sample to delay */
	fir->delay[fir->rwi] = x;

	/* Start FIR calculation. Calculate first number of taps possible to
	 * calculate before circular wrap need.
	 */
	n1 = fir->rwi + 1;
	/* Point to newest sample and advance read index */
	tmp_ri = (fir->rwi)++;
	if (fir->rwi == fir->length)
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
		tmp_ri = fir->length - 1;
		fir_part_32x16(&y, n2, fir->coef, &i, fir->delay, &tmp_ri);
	}
	/* Q9.39 -> Q9.24, saturate to Q8.24 */
	y = sat_int32(y >> (15 + fir->out_shift));

	return (int32_t)y;
}

#endif

struct comp_data {
	struct fir_state_32x16 fir[PLATFORM_MAX_CHANNELS]; /**< filters state */
	struct sof_eq_fir_config *config; /**< pointer to setup blob */
	enum sof_ipc_frame source_format; /**< source frame format */
	enum sof_ipc_frame sink_format;   /**< sink frame format */
	int32_t *fir_delay;		  /**< pointer to allocated RAM */
	size_t fir_delay_size;		  /**< allocated size */
	void (*eq_fir_func_even)(struct fir_state_32x16 fir[],
				 struct comp_buffer *source,
				 struct comp_buffer *sink,
				 int frames, int nch);
	void (*eq_fir_func)(struct fir_state_32x16 fir[],
			    struct comp_buffer *source,
			    struct comp_buffer *sink,
			    int frames, int nch);
};

#endif /* __SOF_AUDIO_EQ_FIR_H__ */
