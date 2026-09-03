/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __USER_FIR_H__
#define __USER_FIR_H__

#include <stdint.h>

#define SOF_FIR_MAX_LENGTH 256 /* Max length for individual filter */

struct sof_fir_coef_data {
	int16_t length; /* Number of FIR taps */
	int16_t out_shift; /* Amount of right shifts at output */

	/* reserved */
	uint32_t reserved[4];

	int16_t coef[]; /* FIR coefficients */
} __attribute__((packed));

/* In the struct above there's two 16 bit words (length, shift) and four
 * reserved 32 bit words before the actual FIR coefficients. This information
 * is used in parsing of the configuration blob.
 */
#define SOF_FIR_COEF_NHEADER \
	(sizeof(struct sof_fir_coef_data) / sizeof(int16_t))

#endif /* __USER_FIR_H__ */
