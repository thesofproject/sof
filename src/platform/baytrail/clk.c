// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

#include <sof/clk.h>
#include <sof/common.h>
#include <config.h>

#if CONFIG_BAYTRAIL
static struct freq_table platform_cpu_freq[] = {
	{ 25000000, 25000, 0x0 },
	{ 25000000, 25000, 0x1 },
	{ 50000000, 50000, 0x2 },
	{ 50000000, 50000, 0x3 }, /* default */
	{ 100000000, 100000, 0x4 },
	{ 200000000, 200000, 0x5 },
	{ 267000000, 267000, 0x6 },
	{ 343000000, 343000, 0x7 },
};
#elif CONFIG_CHERRYTRAIL
static struct freq_table platform_cpu_freq[] = {
	{ 19200000, 19200, 0x0 },
	{ 19200000, 19200, 0x1 },
	{ 38400000, 38400, 0x2 },
	{ 50000000, 50000, 0x3 }, /* default */
	{ 100000000, 100000, 0x4 },
	{ 200000000, 200000, 0x5 },
	{ 267000000, 267000, 0x6 },
	{ 343000000, 343000, 0x7 },
};
#endif

STATIC_ASSERT(NUM_CPU_FREQ == ARRAY_SIZE(platform_cpu_freq),
	      invalid_number_of_cpu_frequencies);

struct freq_table *cpu_freq = platform_cpu_freq;

static struct freq_table platform_ssp_freq[] = {
	{ 19200000, 19200, PMC_SET_SSP_19M2 }, /* default */
	{ 25000000, 25000, PMC_SET_SSP_25M },
};

STATIC_ASSERT(NUM_SSP_FREQ == ARRAY_SIZE(platform_ssp_freq),
	      invalid_number_of_ssp_frequencies);

struct freq_table *ssp_freq = platform_ssp_freq;
