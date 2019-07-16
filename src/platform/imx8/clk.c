// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <sof/common.h>
#include <sof/lib/clk.h>

static struct freq_table platform_cpu_freq[] = {
	{ 666000000, 666000, 0x0 }, /* default */
};

STATIC_ASSERT(NUM_CPU_FREQ == ARRAY_SIZE(platform_cpu_freq),
	      invalid_number_of_cpu_frequencies);

struct freq_table *cpu_freq = platform_cpu_freq;

/* TODO: abstract SSP clk based on platform and remove this from here! */
static struct freq_table platform_ssp_freq[] = {
	{ 19200000, 19200, 19 }, /* default */
	{ 25000000, 25000, 12 },
};

STATIC_ASSERT(NUM_SSP_FREQ == ARRAY_SIZE(platform_ssp_freq),
	      invalid_number_of_ssp_frequencies);

struct freq_table *ssp_freq = platform_ssp_freq;
