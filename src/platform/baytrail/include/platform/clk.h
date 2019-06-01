/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __PLATFORM_CLOCK__
#define __PLATFORM_CLOCK__

#include <platform/pmc.h>
#include <sof/io.h>
#include <platform/shim.h>

#define CLK_CPU(x)	(x)
#define CLK_SSP		1

#define CPU_DEFAULT_IDX		3

#if defined CONFIG_BAYTRAIL
#define SSP_DEFAULT_IDX		1
#elif defined CONFIG_CHERRYTRAIL
#define SSP_DEFAULT_IDX		0
#endif

#define CLK_DEFAULT_CPU_HZ	50000000
#define CLK_MAX_CPU_HZ		343000000

#define NUM_CLOCKS	2

static inline int clock_platform_set_cpu_freq(uint32_t cpu_freq_enc)
{
	/* set CPU frequency request for CCU */
	io_reg_update_bits(SHIM_BASE + SHIM_FR_LAT_REQ, SHIM_FR_LAT_CLK_MASK,
			   cpu_freq_enc);

	/* send freq request to SC */
	return ipc_pmc_send_msg(PMC_SET_LPECLK);
}

static inline int clock_platform_set_ssp_freq(uint32_t ssp_freq_enc)
{
	/* send SSP freq request to SC */
	return ipc_pmc_send_msg(ssp_freq_enc);
}

#endif
