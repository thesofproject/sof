// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/common.h>
#include <rtos/clk.h>

static const struct freq_table platform_cpu_freq[] = {
	{ 38400000, 38400 },
	{ 120000000, 120000 },
	{ CLK_MAX_CPU_HZ, 400000 },
};

const uint32_t cpu_freq_enc[] = {
	SHIM_CLKCTL_WOVCROSC | SHIM_CLKCTL_WOV_CRO_REQUEST |
		SHIM_CLKCTL_HMCS_DIV2 | SHIM_CLKCTL_LMCS_DIV4,
	SHIM_CLKCTL_RLROSCC | SHIM_CLKCTL_OCS_LP_RING |
		SHIM_CLKCTL_HMCS_DIV2 | SHIM_CLKCTL_LMCS_DIV4,
	SHIM_CLKCTL_RHROSCC | SHIM_CLKCTL_OCS_HP_RING |
		SHIM_CLKCTL_HMCS_DIV2 | SHIM_CLKCTL_LMCS_DIV4,
};

const uint32_t cpu_freq_status_mask[] = {
	SHIM_CLKSTS_WOV_CRO,
	SHIM_CLKSTS_LROSCCS,
	SHIM_CLKSTS_HROSCCS
};

STATIC_ASSERT(ARRAY_SIZE(platform_cpu_freq) == NUM_CPU_FREQ,
	      invalid_number_of_cpu_frequencies);

const struct freq_table *cpu_freq = platform_cpu_freq;
