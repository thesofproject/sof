// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022-2024 Intel Corporation.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/common.h>
#include <rtos/clk.h>

static const struct freq_table platform_cpu_freq[] = {
	{ 38400000, 38400 },
	{ 120000000, 120000 },
	{ CLK_MAX_CPU_HZ, CLK_MAX_CPU_HZ / 1000 }
};

STATIC_ASSERT(ARRAY_SIZE(platform_cpu_freq) == NUM_CPU_FREQ, invalid_number_of_cpu_frequencies);

const struct freq_table *cpu_freq = platform_cpu_freq;
