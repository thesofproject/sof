/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 AMD.All rights reserved.
 *
 * Author:      Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
 *              Bala Kishore <balakishore.pati@amd.com>
 */
#ifdef __SOF_LIB_CLK_H__

#ifndef __PLATFORM_LIB_CLK_H__
#define __PLATFORM_LIB_CLK_H__

#include <sof/lib/io.h>
#include <stdint.h>

struct sof;

#define CLK_CPU(x)	(x)

#define CPU_DEFAULT_IDX		0

#define CLK_DEFAULT_CPU_HZ	600000000
#define CLK_MAX_CPU_HZ		600000000

#define NUM_CLOCKS	2

#define NUM_CPU_FREQ	1

#define ACP_MASTER_REG_ACCESS_ADDRESS 0x3400

#define MP1_APERTURE_ID 0x0

#define ACP_FIRST_REG_OFFSET 0x01240000

#define ACP_SMU_MSG_SET_ACLK 0x3


void acp_change_clock_notify(uint32_t	clockFreq);

void platform_clock_init(struct sof *sof);

#endif /* __PLATFORM_LIB_CLK_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/clk.h"

#endif /* __SOF_LIB_CLK_H__ */
