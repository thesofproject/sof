// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <sof/common.h>
#include <sof/lib/clk.h>

static struct freq_table platform_cpu_freq[] = {
	{ 32000000, 32000, 0x6 },
	{ 80000000, 80000, 0x2 },
	{ 160000000, 160000, 0x1 },
	{ 320000000, 320000, 0x4 }, /* default */
	{ 320000000, 320000, 0x0 },
	{ 160000000, 160000, 0x5 },
};

STATIC_ASSERT(NUM_CPU_FREQ == ARRAY_SIZE(platform_cpu_freq),
	      invalid_number_of_cpu_frequencies);

struct freq_table *cpu_freq = platform_cpu_freq;

static struct freq_table platform_ssp_freq[] = {
	{ 24000000, 24000, 0 }, /* default */
};

STATIC_ASSERT(NUM_SSP_FREQ == ARRAY_SIZE(platform_ssp_freq),
	      invalid_number_of_ssp_frequencies);

struct freq_table *ssp_freq = platform_ssp_freq;
