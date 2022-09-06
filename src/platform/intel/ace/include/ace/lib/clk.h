/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
 *         Adrian Warecki <adrian.warecki@intel.com>
 */

/**
 * \file ace/lib/clk.h
 * \brief Clk parameters for run-time clock data, common for cAVS platforms.
 */

#ifdef __PLATFORM_LIB_CLK_H__

#ifndef __ACE_LIB_CLK_H__
#define __ACE_LIB_CLK_H__

#include <ace/version.h>
#include <sof/lib/cpu.h>
#include <sof/lib/io.h>
#include <sof/lib/memory.h>
#include <sof/lib/shim.h>
#include <stdint.h>

struct sof;

/** \brief Core(s) settings, up to CONFIG_CORE_COUNT */
#define CLK_CPU(x)	(x)

/** \brief SSP clock r-t settings are after the core(s) settings */
#define CLK_SSP		CONFIG_CORE_COUNT

/* SSP clock run-time data is the last one, so total number is ssp idx +1 */

/** \brief Total number of clocks */
#define NUM_CLOCKS	(CLK_SSP + 1)

extern const struct freq_table *cpu_freq;
extern const uint32_t cpu_freq_enc[];
extern const uint32_t cpu_freq_status_mask[];

void platform_clock_init(struct sof *sof);

void platform_clock_on_waiti(void);
void platform_clock_on_wakeup(void);

#endif /* __ACE_LIB_CLK_H__ */

#else

#error "This file shouldn't be included from outside of platform/lib/clk.h"

#endif /* __PLATFORM_LIB_CLK_H__ */
