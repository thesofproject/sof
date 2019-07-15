/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_CLK_H__

#ifndef __PLATFORM_CLK_H__
#define __PLATFORM_CLK_H__

#include <sof/io.h>
#include <sof/shim.h>
#include <stdint.h>

#define CLK_CPU(x)	(x)
#define CLK_SSP		1

#define CPU_DEFAULT_IDX		3
#define SSP_DEFAULT_IDX		0

#define CLK_DEFAULT_CPU_HZ	320000000
#define CLK_MAX_CPU_HZ		320000000

#define NUM_CLOCKS	2

#define NUM_CPU_FREQ	6
#define NUM_SSP_FREQ	1

static inline int clock_platform_set_cpu_freq(uint32_t cpu_freq_enc)
{
	/* set CPU frequency request for CCU */
	io_reg_update_bits(SHIM_BASE + SHIM_CSR, SHIM_CSR_DCS_MASK,
			   cpu_freq_enc);

	return 0;
}

static inline int clock_platform_set_ssp_freq(uint32_t ssp_freq_enc)
{
	return 0;
}

#endif /* __PLATFORM_CLK_H__ */

#else

#error "This file shouldn't be included from outside of sof/clk.h"

#endif /* __SOF_CLK_H__ */
