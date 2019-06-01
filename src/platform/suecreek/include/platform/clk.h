/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
 */

#ifndef __PLATFORM_CLOCK__
#define __PLATFORM_CLOCK__

#include <sof/cpu.h>
#include <sof/io.h>
#include <platform/shim.h>

#define CLK_CPU(x)	(x)
#define CLK_SSP		4

#define CPU_DEFAULT_IDX		1
#define SSP_DEFAULT_IDX		0

#define CLK_DEFAULT_CPU_HZ	120000000
#define CLK_MAX_CPU_HZ		400000000

#define NUM_CLOCKS	5

static inline int clock_platform_set_cpu_freq(uint32_t cpu_freq_enc)
{
	/* set CPU frequency request for CCU */
	io_reg_update_bits(SHIM_BASE + SHIM_CLKCTL,
			   SHIM_CLKCTL_DPCS_MASK(cpu_get_id()),
			   cpu_freq_enc);

	return 0;
}

static inline int clock_platform_set_ssp_freq(uint32_t ssp_freq_enc)
{
	return 0;
}

#endif
