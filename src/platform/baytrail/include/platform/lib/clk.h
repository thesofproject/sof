/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_LIB_CLK_H__

#ifndef __PLATFORM_LIB_CLK_H__
#define __PLATFORM_LIB_CLK_H__

#include <sof/drivers/pmc.h>
#include <sof/lib/io.h>
#include <sof/lib/shim.h>
#include <stdint.h>

struct sof;

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

#define NUM_CPU_FREQ	8
#define NUM_SSP_FREQ	2

void platform_clock_init(struct sof *sof);

#endif /* __PLATFORM_LIB_CLK_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/clk.h"

#endif /* __SOF_LIB_CLK_H__ */
