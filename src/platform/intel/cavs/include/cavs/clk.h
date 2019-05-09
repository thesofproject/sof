/*
 * Copyright (c) 2019, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
 */

/**
 * \file cavs/clk.h
 * \brief Clk parameters for run-time clock data, common for cAVS platforms.
 */

#ifndef __INCLUDE_CAVS_CLK__
#define __INCLUDE_CAVS_CLK__

#include <sof/cpu.h>
#include <sof/io.h>
#include <platform/shim.h>
#include <cavs/version.h>
#include <cavs/cpu.h>

/** \brief Core(s) settings, up to PLATFORM_CORE_COUNT */
#define CLK_CPU(x)	(x)

/** \brief SSP clock r-t settings are after the core(s) settings */
#define CLK_SSP		PLATFORM_CORE_COUNT

/* SSP clock run-time data is the last one, so total number is ssp idx +1 */

/** \brief Total number of clocks */
#define NUM_CLOCKS	(CLK_SSP + 1)

static inline int clock_platform_set_cpu_freq(uint32_t cpu_freq_enc)
{
	/* set CPU frequency request for CCU */
#if CAVS_VERSION == CAVS_VERSION_1_5
	io_reg_update_bits(SHIM_BASE + SHIM_CLKCTL, SHIM_CLKCTL_HDCS, 0);
#endif
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
