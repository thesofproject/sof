// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/common.h>
#include <sof/drivers/ssp.h>
#include <sof/lib/clk.h>

static struct freq_table platform_cpu_freq[] = {
	{ 120000000, 120000 },
	{ CLK_MAX_CPU_HZ, 400000 },
};

uint32_t cpu_freq_enc[] = {
	0x0,
	0x4,
};

STATIC_ASSERT(NUM_CPU_FREQ == ARRAY_SIZE(platform_cpu_freq),
	      invalid_number_of_cpu_frequencies);

struct freq_table *cpu_freq = platform_cpu_freq;

/* IMPORTANT: array should be filled in increasing order
 * (regarding to .freq field)
 */
static struct freq_table platform_ssp_freq[] = {
	{ 19200000, 19200 },
	{ 24000000, 24000 },
};

static uint32_t platform_ssp_freq_sources[] = {
	SSP_CLOCK_XTAL_OSCILLATOR,
	SSP_CLOCK_PLL_FIXED,
};

STATIC_ASSERT(NUM_SSP_FREQ == ARRAY_SIZE(platform_ssp_freq),
	      invalid_number_of_ssp_frequencies);

struct freq_table *ssp_freq = platform_ssp_freq;
uint32_t *ssp_freq_sources = platform_ssp_freq_sources;
