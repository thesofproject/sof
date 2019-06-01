/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

/* Format for generated coefficients tables */

struct pdm_decim {
	int decim_factor;
	int length;
	int shift;
	int relative_passband;
	int relative_stopband;
	int passband_ripple;
	int stopband_ripple;
	const int32_t *coef;
};
