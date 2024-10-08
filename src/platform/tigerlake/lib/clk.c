// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/common.h>
#include <sof/drivers/ssp.h>
#include <rtos/clk.h>

/** \brief Request HP RING Oscillator Clock */
#define SHIM_CLKCTL_RHROSCC	BIT(31)

/** \brief Request WOVCRO Clock */
#define SHIM_CLKCTL_WOV_CRO_REQUEST	BIT(4)

/** \brief Request LP RING Oscillator Clock */
#define SHIM_CLKCTL_RLROSCC	BIT(29)

/** \brief Oscillator Clock Select*/
#define SHIM_CLKCTL_OCS_HP_RING		BIT(2)
#define SHIM_CLKCTL_OCS_LP_RING		0
#define SHIM_CLKCTL_WOVCROSC		BIT(3)

/** \brief LP Memory Clock Select */
#define SHIM_CLKCTL_LMCS_DIV2	0
#define SHIM_CLKCTL_LMCS_DIV4	BIT(1)

/** \brief HP Memory Clock Select */
#define SHIM_CLKCTL_HMCS_DIV2	0
#define SHIM_CLKCTL_HMCS_DIV4	BIT(0)

/** \brief HP RING Oscillator Clock Status */
#define SHIM_CLKSTS_HROSCCS	BIT(31)

/** \brief WOVCRO Clock Status */
#define SHIM_CLKSTS_WOV_CRO	BIT(4)

/** \brief XTAL Oscillator Clock Status */
#define SHIM_CLKSTS_XOSCCS	BIT(30)

/** \brief LP RING Oscillator Clock Status */
#define SHIM_CLKSTS_LROSCCS	BIT(29)

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

STATIC_ASSERT(NUM_CPU_FREQ == ARRAY_SIZE(platform_cpu_freq),
	      invalid_number_of_cpu_frequencies);

const struct freq_table *cpu_freq = platform_cpu_freq;

/* IMPORTANT: array should be filled in increasing order
 * (regarding to .freq field)
 */
static const struct freq_table platform_ssp_freq[] = {
	{ 24576000, 24576 },
	{ 38400000, 38400 },
	{ 96000000, 96000 },
};

static const uint32_t platform_ssp_freq_sources[] = {
	SSP_CLOCK_AUDIO_CARDINAL,
	SSP_CLOCK_XTAL_OSCILLATOR,
	SSP_CLOCK_PLL_FIXED,
};

STATIC_ASSERT(NUM_SSP_FREQ == ARRAY_SIZE(platform_ssp_freq),
	      invalid_number_of_ssp_frequencies);

const struct freq_table *ssp_freq = platform_ssp_freq;
const uint32_t *ssp_freq_sources = platform_ssp_freq_sources;
