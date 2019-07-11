// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <sof/clk.h>
#include <sof/common.h>

static struct freq_table platform_cpu_freq[] = {
	{ 100000000, 100000, 0x3 },
	{ 200000000, 200000, 0x1 },
	{ CLK_MAX_CPU_HZ, 400000, 0x0 }, /* the default one */
};

STATIC_ASSERT(NUM_CPU_FREQ == ARRAY_SIZE(platform_cpu_freq),
	      invalid_number_of_cpu_frequencies);

struct freq_table *cpu_freq = platform_cpu_freq;

/* IMPORTANT: array should be filled in increasing order
 * (regarding to .freq field)
 */
static struct freq_table platform_ssp_freq[] = {
	{ 19200000, 19200, CLOCK_SSP_XTAL_OSCILLATOR }, /* the default one */
	{ 24576000, 24576, CLOCK_SSP_AUDIO_CARDINAL },
	{ 96000000, 96000, CLOCK_SSP_PLL_FIXED },
};

STATIC_ASSERT(NUM_SSP_FREQ == ARRAY_SIZE(platform_ssp_freq),
	      invalid_number_of_ssp_frequencies);

struct freq_table *ssp_freq = platform_ssp_freq;
