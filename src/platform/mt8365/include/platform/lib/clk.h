/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Andrew Perepech <andrew.perepech@mediatek.com>
 */

#ifdef __SOF_LIB_CLK_H__

#ifndef __PLATFORM_LIB_CLK_H__
#define __PLATFORM_LIB_CLK_H__

#include <stdint.h>

struct sof;

#define CPU_DEFAULT_IDX             4

#define CLK_CPU(x)                  (x)
#define CLK_DEFAULT_CPU_HZ          600000000
#define CLK_MAX_CPU_HZ              600000000
#define CLK_SUSPEND_CPU_HZ          26000000
#define NUM_CLOCKS                  1
#define NUM_CPU_FREQ                5

void platform_clock_init(struct sof *sof);

#define REG_APMIXDSYS_BASE     0x1000C000
#define REG_TOPCKGEN_BASE      0x10000000

#define DSPPLL_CON0            (REG_APMIXDSYS_BASE + 0x390)
#define DSPPLL_CON1            (REG_APMIXDSYS_BASE + 0x394)
#define DSPPLL_CON2            (REG_APMIXDSYS_BASE + 0x398)
#define DSPPLL_CON3            (REG_APMIXDSYS_BASE + 0x39C)

#define ULPLL_CON0             (REG_APMIXDSYS_BASE + 0x3B0)
#define ULPLL_CON1             (REG_APMIXDSYS_BASE + 0x3B4)

#define PLL_BASE_EN            BIT(0)
#define PLL_PWR_ON             BIT(0)
#define PLL_ISO_EN             BIT(1)

#define DSPPLL_312MHZ          0
#define DSPPLL_400MHZ          1
#define DSPPLL_600MHZ          2

#define CLK_MODE               (REG_TOPCKGEN_BASE + 0x0)
#define CLK_CFG_UPDATE1        (REG_TOPCKGEN_BASE + 0x8)
#define CLK_CFG_8              (REG_TOPCKGEN_BASE + 0xC0)
#define CLK_CFG_8_SET          (REG_TOPCKGEN_BASE + 0xC4)
#define CLK_CFG_8_CLR          (REG_TOPCKGEN_BASE + 0xC8)

#define CLK_SCP_CFG_1          (REG_TOPCKGEN_BASE + 0x204)

#define CLK_DSP_SEL_26M               0
#define CLK_DSP_SEL_26M_D_2           1
#define CLK_DSP_SEL_DSPPLL            2
#define CLK_DSP_SEL_DSPPLL_D_2        3
#define CLK_DSP_SEL_DSPPLL_D_4        4
#define CLK_DSP_SEL_DSPPLL_D_8        5

#define CLK_TOPCKGEN_SEL_PLLGP_26M    1
#define CLK_TOPCKGEN_SEL_ULPLL_26M    2
#define CLK_TOPCKGEN_SEL_GPIO_26M     4

enum mux_id_t {
	MUX_CLK_DSP_SEL = 0,
	MUX_CLK_TOPCKGEN_26M_SEL,
	HIFI4DSP_MUX_NUM,
};

enum mux_26m_t {
	DCXO_26 = 0,
	ULPLL_26M,
};

enum DSP_HW_DSP_CLK {
	DSP_CLK_13M = 0,
	DSP_CLK_26M,
	DSP_CLK_PLL_312M,
	DSP_CLK_PLL_400M,
	DSP_CLK_PLL_600M,
};
#endif /* __PLATFORM_LIB_CLK_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/clk.h"

#endif /* __SOF_LIB_CLK_H__ */
