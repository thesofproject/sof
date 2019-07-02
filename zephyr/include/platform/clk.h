/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __PLATFORM_CLOCK__
#define __PLATFORM_CLOCK__

#include <sof/io.h>
#include <platform/shim.h>

#define CLK_CPU(x)	(x)
#define CLK_SSP		1

#define CPU_DEFAULT_IDX		3
#define SSP_DEFAULT_IDX		0

#define CLK_DEFAULT_CPU_HZ	320000000
#define CLK_MAX_CPU_HZ		320000000

#define NUM_CLOCKS	2

static inline int clock_platform_set_cpu_freq(uint32_t cpu_freq_enc)
{
	return 0;
}

static inline int clock_platform_set_ssp_freq(uint32_t ssp_freq_enc)
{
	return 0;
}

#endif

