/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Google LLC. All rights reserved.
 *
 * Author: Pin-chih Lin <johnylin@google.com>
 */

#ifndef __USER_MULTIBAND_DRC_H__
#define __USER_MULTIBAND_DRC_H__

#include <stdint.h>
#include <user/crossover.h>
#include <user/drc.h>
#include <user/eq.h>

/* Maximum number of frequency band for Multiband DRC */
#define SOF_MULTIBAND_DRC_MAX_BANDS SOF_CROSSOVER_MAX_STREAMS

/* Maximum number of Crossover LR4 highpass and lowpass filters */
#define SOF_CROSSOVER_MAX_LR4 ((SOF_CROSSOVER_MAX_STREAMS - 1) * 2)

/* The number of biquads (and biquads in series) of (De)Emphasis Equalizer */
#define SOF_EMP_DEEMP_BIQUADS 2

/* Maximum number allowed of IPC configuration blob size */
#define SOF_MULTIBAND_DRC_MAX_BLOB_SIZE 1024

 /* multiband_drc configuration
  *     Multiband DRC is a single-source-single-sink compound component which
  *     consists of 4 stages: Emphasis Equalizer, Crossover Filter (from 1-band
  *     to 4-band), DRC (per band), and Deemphasis Equalizer of summed stream.
  *
  *     The following graph illustrates a 3-band Multiband DRC component:
  *
  *                                      low
  *                                     o----> DRC0 ----o
  *                                     |               |
  *                           3-WAY     |mid            |
  *     x(n) --> EQ EMP --> CROSSOVER --o----> DRC1 ---(+)--> EQ DEEMP --> y(n)
  *                                     |               |
  *                                     |high           |
  *                                     o----> DRC2 ----o
  *
  *     uint32_t num_bands <= 4
  *         Determines the number of frequency bands, the choice of n-way
  *         Crossover, and the number of DRC components.
  *     uint32_t enable_emp_deemp
  *         1=enable Emphasis and Deemphasis Equalizer; 0=disable (passthrough)
  *     struct sof_eq_iir_biquad_df2t emp_coef[2]
  *         The coefficient data for Emphasis Equalizer, which is a cascade of 2
  *         biquad filters.
  *     struct sof_eq_iir_biquad_df2t deemp_coef[2]
  *         The coefficient data for Deemphasis Equalizer, which is a cascade of
  *         2 biquad filters.
  *     struct sof_eq_iir_biquad_df2t crossover_coef[6]
  *         The coefficient data for Crossover LR4 filters. Please refer
  *         src/include/user/crossover.h for details. Zeros will be filled if
  *         the entries are useless. For example, when 2-way crossover is used:
  *     struct sof_drc_params drc_coef[num_bands]
  *         The parameter data for DRC per band, the number entries of this may
  *         vary. Please refer src/include/user/drc.h for details.
  *
  */
struct sof_multiband_drc_config {
	uint32_t size;
	uint32_t num_bands;
	uint32_t enable_emp_deemp;

	/* reserved */
	uint32_t reserved[8];

	/* config of emphasis eq-iir */
	struct sof_eq_iir_biquad_df2t emp_coef[SOF_EMP_DEEMP_BIQUADS];

	/* config of deemphasis eq-iir */
	struct sof_eq_iir_biquad_df2t deemp_coef[SOF_EMP_DEEMP_BIQUADS];

	/* config of crossover */
	struct sof_eq_iir_biquad_df2t crossover_coef[SOF_CROSSOVER_MAX_LR4];

	/* config of multi-band drc */
	struct sof_drc_params drc_coef[];
};

#endif // __USER_MULTIBAND_DRC_H__
