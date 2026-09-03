/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Google LLC. All rights reserved.
 *
 * Author: Pin-chih Lin <johnylin@google.com>
 */
#ifndef __USER_DRC_H__
#define __USER_DRC_H__

#include <stdint.h>

/* Maximum number allowed in configuration blob */
#define SOF_DRC_MAX_SIZE 1024

/* The parameters of the DRC compressor */
struct sof_drc_params {
	/* 1 to enable DRC, 0 to disable it */
	int32_t enabled;

	/* The value above which the compression starts, in dB */
	int32_t db_threshold; /* Q8.24 */

	/* The value above which the knee region starts, in dB */
	int32_t db_knee; /* Q8.24 */

	/* The input/output dB ratio after the knee region */
	int32_t ratio; /* Q8.24 */

	/* The lookahead time for the compressor, in seconds */
	int32_t pre_delay_time; /* Q2.30 */

	/* The input to output change below the threshold is 1:1 */
	int32_t linear_threshold; /* Q2.30 */

	/* Inverse ratio */
	int32_t slope; /* Q2.30 */

	/* Internal parameter for the knee portion of the curve. */
	int32_t K; /* Q12.20 */

	/* Pre-calculated parameters */
	int32_t knee_alpha;     /* Q8.24 */
	int32_t knee_beta;      /* Q8.24 */
	int32_t knee_threshold; /* Q8.24 */
	int32_t ratio_base;     /* Q2.30 */

	int32_t master_linear_gain;             /* Q8.24 */
	int32_t one_over_attack_frames;         /* Q2.30 */
	int32_t sat_release_frames_inv_neg;     /* Q2.30 */
	int32_t sat_release_rate_at_neg_two_db; /* Q2.30 */

	/* The release frames coefficients */
	int32_t kSpacingDb; /* Q32.0 */
	int32_t kA; /* Q20.12 */
	int32_t kB; /* Q20.12 */
	int32_t kC; /* Q20.12 */
	int32_t kD; /* Q20.12 */
	int32_t kE; /* Q20.12 */
} __attribute__((packed));

struct sof_drc_config {
	uint32_t size;

	/* reserved */
	uint32_t reserved[4];

	struct sof_drc_params params;
} __attribute__((packed));

#endif //  __USER_DRC_H__
