/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifdef __SOF_CLK_H__

#ifndef __PLATFORM_CLK_H__
#define __PLATFORM_CLK_H__

#include <stdint.h>

#define CLK_CPU(x)	(x)
#define CLK_SSP		1

#define CPU_DEFAULT_IDX		0
#define SSP_DEFAULT_IDX		1

#define CLK_DEFAULT_CPU_HZ	666000000
#define CLK_MAX_CPU_HZ		666000000

#define NUM_CLOCKS	1

#define NUM_CPU_FREQ	1
#define NUM_SSP_FREQ	2

static inline int clock_platform_set_cpu_freq(uint32_t cpu_freq_enc)
{
	return 0;
}

/* FIXME: compile out clock_platform_set_ssp_freq */
static inline int clock_platform_set_ssp_freq(uint32_t ssp_freq_enc)
{
	/* we don't even have SSP */
	return 0;
}

#endif /* __PLATFORM_CLK_H__ */

#else

#error "This file shouldn't be included from outside of sof/clk.h"

#endif /* __SOF_CLK_H__ */
