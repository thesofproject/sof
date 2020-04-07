/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_AUDIO_EQ_IIR_IIR_H__
#define __SOF_AUDIO_EQ_IIR_IIR_H__

#include <stddef.h>
#include <stdint.h>
#include <sof/audio/format.h>

struct sof_eq_iir_header_df2t;

/* Get platforms configuration */
#include <config.h>

/* If next defines are set to 1 the EQ is configured automatically. Setting
 * to zero temporarily is useful is for testing needs.
 * Setting EQ_FIR_AUTOARCH to 0 allows to manually set the code variant.
 */
#define IIR_AUTOARCH    1

/* Force manually some code variant when IIR_AUTOARCH is set to zero. These
 * are useful in code debugging.
 */
#if IIR_AUTOARCH == 0
#define IIR_GENERIC	1
#define IIR_HIFI3	0
#endif

/* Select optimized code variant when xt-xcc compiler is used */
#if IIR_AUTOARCH == 1
#if defined __XCC__
#include <xtensa/config/core-isa.h>
#if XCHAL_HAVE_HIFI3 == 1
#define IIR_GENERIC	0
#define IIR_HIFI3	1
#else
#define IIR_GENERIC	1
#define IIR_HIFI3	0
#endif /* XCHAL_HAVE_HIFI3 */
#else
/* GCC */
#define IIR_GENERIC	1
#define IIR_HIFI3	0
#endif /* __XCC__ */
#endif /* IIR_AUTOARCH */

#define IIR_DF2T_NUM_DELAYS 2

struct iir_state_df2t {
	unsigned int biquads; /* Number of IIR 2nd order sections total */
	unsigned int biquads_in_series; /* Number of IIR 2nd order sections
					 * in series.
					 */
	int32_t *coef; /* Pointer to IIR coefficients */
	int64_t *delay; /* Pointer to IIR delay line */
};

/*
 * \brief Returns the output of a biquad.
 *
 * 32 bit data, 32 bit coefficients and 64 bit state variables
 * \param in input to the biquad Q1.31
 * \param coef coefficients of the biquad Q2.30
 * \param delay delay of the biquads Q3.61
 */
static inline int32_t iir_process_biquad(int32_t in, int32_t *coef,
					 int64_t *delay)
{
	int32_t tmp;
	int64_t acc;

	/* Compute output: Delay is Q3.61
	 * Q2.30 x Q1.31 -> Q3.61
	 * Shift Q3.61 to Q3.31 with rounding
	 */
	acc = ((int64_t)coef[4]) * in + delay[0];
	tmp = (int32_t)Q_SHIFT_RND(acc, 61, 31);

	/* Compute first delay */
	acc = delay[1];
	acc += ((int64_t)coef[3]) * in; /* Coef  b1 */
	acc += ((int64_t)coef[1]) * tmp; /* Coef a1 */
	delay[0] = acc;

	/* Compute second delay */
	acc = ((int64_t)coef[2]) * in; /* Coef  b2 */
	acc += ((int64_t)coef[0]) * tmp; /* Coef a2 */
	delay[1] = acc;

	/* Apply gain Q2.14 x Q1.31 -> Q3.45 */
	acc = ((int64_t)coef[6]) * tmp; /* Gain */

	/* Apply biquad output shift right parameter
	 * simultaneously with Q3.45 to Q3.31 conversion. Then
	 * saturate to 32 bits Q1.31 and prepare for next
	 * biquad.
	 */
	acc = Q_SHIFT_RND(acc, 45 + coef[5], 31);
	return sat_int32(acc);
}

int32_t iir_df2t(struct iir_state_df2t *iir, int32_t x);

int iir_init_coef_df2t(struct iir_state_df2t *iir,
		       struct sof_eq_iir_header_df2t *config);

int iir_delay_size_df2t(struct sof_eq_iir_header_df2t *config);

void iir_init_delay_df2t(struct iir_state_df2t *iir, int64_t **delay);

void iir_mute_df2t(struct iir_state_df2t *iir);

void iir_unmute_df2t(struct iir_state_df2t *iir);

void iir_reset_df2t(struct iir_state_df2t *iir);

#endif /* __SOF_AUDIO_EQ_IIR_IIR_H__ */
