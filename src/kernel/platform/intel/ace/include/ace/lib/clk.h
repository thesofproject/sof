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

struct sof;

/** \brief Total number of clocks */
#define NUM_CLOCKS	CONFIG_CORE_COUNT

extern const struct freq_table *cpu_freq;

void platform_clock_init(struct sof *sof);

#endif /* __ACE_LIB_CLK_H__ */

#else

#error "This file shouldn't be included from outside of platform/lib/clk.h"

#endif /* __PLATFORM_LIB_CLK_H__ */
