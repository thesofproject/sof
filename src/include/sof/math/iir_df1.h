/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Andrula Song <andrula.song@intel.com>
 */

#ifndef __SOF_MATH_IIR_DF1_H__
#define __SOF_MATH_IIR_DF1_H__

#include <stddef.h>
#include <stdint.h>
#include <sof/common.h>

#define IIR_DF1_NUM_STATE 4
#define SOF_IIR_DF1_4TH_NUM_BIQUADS 2

struct iir_state_df1 {
	unsigned int biquads; /* Number of IIR 2nd order sections total */
	unsigned int biquads_in_series; /* Number of IIR 2nd order sections
					 * in series.
					 */
	int32_t *coef; /* Pointer to IIR coefficients */
	int32_t *delay; /* Pointer to IIR delay line */
};

struct sof_eq_iir_header;

int iir_init_coef_df1(struct iir_state_df1 *iir,
		      struct sof_eq_iir_header *config);

int iir_delay_size_df1(struct sof_eq_iir_header *config);

void iir_init_delay_df1(struct iir_state_df1 *iir, int32_t **state);

void iir_reset_df1(struct iir_state_df1 *iir);

/**
 * Calculate IIR filter consisting of biquads
 * @param iir	IIR state with configured biquad coefficients and delay lines data
 * @param x	Single s32 Q1.31 format input sample
 * @return	Single s32 Q1.31 format output samples
 */
int32_t iir_df1(struct iir_state_df1 *iir, int32_t x);

/**
 * Calculate IIR filter consisting of biquads, special simplified version for
 * 4th order filter with two biquads in series. Note: There are no checks for
 * iir struct members.
 * @param iir	IIR state with configured biquad coefficients and delay lines data
 * @param x	Single s32 Q1.31 format input sample
 * @return	Single s32 Q1.31 format output samples
 */
int32_t iir_df1_4th(struct iir_state_df1 *iir, int32_t x);

/* Inline functions */
#if SOF_USE_MIN_HIFI(3, FILTER)
#include "iir_df1_hifi3.h"
#else
#include "iir_df1_generic.h"
#endif

#endif
