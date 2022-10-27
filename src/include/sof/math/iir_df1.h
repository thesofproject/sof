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

#define IIR_DF1_NUM_STATE 4

#if defined __XCC__
#include <xtensa/config/core-isa.h>
#if XCHAL_HAVE_HIFI3
#define IIR_DF1_GENERIC	0
#define IIR_DF1_HIFI3	1
#else
#define IIR_DF1_GENERIC	1
#define IIR_DF1_HIFI3	0
#endif /* XCHAL_HAVE_HIFI3 */
#else
/* GCC */
#define IIR_DF1_GENERIC	1
#define IIR_DF1_HIFI3	0
#endif

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

int32_t iir_df1(struct iir_state_df1 *iir, int32_t x);

/* Inline functions */
#if IIR_DF1_HIFI3
#include "iir_df1_hifi3.h"
#else
#include "iir_df1_generic.h"
#endif

#endif
