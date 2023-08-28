/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_MATH_IIR_DF2T_H__
#define __SOF_MATH_IIR_DF2T_H__

#include <stddef.h>
#include <stdint.h>

/* Define SOFM_IIR_DF2T_FORCEARCH 0/3 in build command line or temporarily in
 * this file to override the default auto detection.
 */
#ifdef SOFM_IIR_DF2T_FORCEARCH
#  if SOFM_IIR_DF2T_FORCEARCH == 3
#    define IIR_GENERIC	0
#    define IIR_HIFI3	1
#  elif SOFM_IIR_DF2T_FORCEARCH == 0
#    define IIR_GENERIC	1
#    define IIR_HIFI3	0
#  else
#    error "Unsupported SOFM_IIR_DF2T_FORCEARCH value."
#  endif
#else
#  if defined __XCC__
#    include <xtensa/config/core-isa.h>
#    if XCHAL_HAVE_HIFI3 == 1 || XCHAL_HAVE_HIFI4 == 1
#      define IIR_GENERIC	0
#      define IIR_HIFI3		1
#    else
#      define IIR_GENERIC	1
#      define IIR_HIFI3		0
#    endif /* XCHAL_HAVE_HIFIn */
#  else
#    define IIR_GENERIC		1
#    define IIR_HIFI3		0
#  endif /* __XCC__ */
#endif /* SOFM_IIR_DF2T_FORCEARCH */

#define IIR_DF2T_NUM_DELAYS 2

struct iir_state_df2t {
	unsigned int biquads; /* Number of IIR 2nd order sections total */
	unsigned int biquads_in_series; /* Number of IIR 2nd order sections
					 * in series.
					 */
	int32_t *coef; /* Pointer to IIR coefficients */
	int64_t *delay; /* Pointer to IIR delay line */
};

struct sof_eq_iir_header;

int iir_init_coef_df2t(struct iir_state_df2t *iir,
		       struct sof_eq_iir_header *config);

int iir_delay_size_df2t(struct sof_eq_iir_header *config);

void iir_init_delay_df2t(struct iir_state_df2t *iir, int64_t **delay);

void iir_mute_df2t(struct iir_state_df2t *iir);

void iir_unmute_df2t(struct iir_state_df2t *iir);

void iir_reset_df2t(struct iir_state_df2t *iir);

int32_t iir_df2t(struct iir_state_df2t *iir, int32_t x);

/* Inline functions with or without HiFi3 intrinsics */
#if IIR_HIFI3
#include "iir_df2t_hifi3.h"
#else
#include "iir_df2t_generic.h"
#endif

#endif /* __SOF_MATH_IIR_DF2T_H__ */
