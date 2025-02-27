// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Andrew Perepech <andrew.perepech@mediatek.com>
 */

#include <sof/common.h>
#include <rtos/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <rtos/wait.h>
#include <rtos/sof.h>
#include <sof/trace/trace.h>

SOF_DEFINE_REG_UUID(clkdrv_mt8365);

DECLARE_TR_CTX(clkdrv_tr, SOF_UUID(clkdrv_mt8365_uuid), LOG_LEVEL_INFO);

static int dsppll_enable;  /* default no adsp clock*/
static int adsp_clock;

const struct freq_table platform_cpu_freq[] = {
	{ 13000000, 13000},
	{ 26000000, 13000},
	{ 312000000, 13000},
	{ 400000000, 13000},
	{ 600000000, 13000},
};

const uint32_t cpu_freq_enc[] = {
	13000000,
	26000000,
	0x83180000,
	0x820F6276,
	0x821713B1,
};

STATIC_ASSERT(ARRAY_SIZE(platform_cpu_freq) == NUM_CPU_FREQ,
	      invalid_number_of_cpu_frequencies);

static SHARED_DATA struct clock_info platform_clocks_info[NUM_CLOCKS];

static inline void clk_setl(uint32_t addr, uint32_t val)
{
	io_reg_write(addr, io_reg_read(addr) | (val));
}

static inline void clk_clrl(uint32_t addr, uint32_t val)
{
	io_reg_write(addr, io_reg_read(addr) & ~(val));
}

static inline int dsp_clk_value_convert(int value)
{
	int ret;

	switch (value) {
	case DSP_CLK_13M:
		ret = CLK_DSP_SEL_26M_D_2;
		break;
	case DSP_CLK_26M:
		ret = CLK_DSP_SEL_26M;
		break;
	case DSP_CLK_PLL_312M:
	case DSP_CLK_PLL_400M:
	case DSP_CLK_PLL_600M:
		ret = CLK_DSP_SEL_DSPPLL;
		break;
	default:
		ret = CLK_DSP_SEL_26M;
		break;
	}

	return ret;
}

static void clk_dsppll_enable(void)
{
	tr_dbg(&clkdrv_tr, "clk_dsppll_enable\n");

	clk_setl(DSPPLL_CON3, PLL_PWR_ON);
	wait_delay_us(1);
	clk_clrl(DSPPLL_CON3, PLL_ISO_EN);
	wait_delay_us(1);
	clk_setl(DSPPLL_CON0, PLL_BASE_EN);
	wait_delay_us(20);
	dsppll_enable = 1;

}

static void clk_dsppll_disable(void)
{
	tr_dbg(&clkdrv_tr, "clk_dsppll_disable\n");

	clk_clrl(DSPPLL_CON0, PLL_BASE_EN);
	wait_delay_us(1);
	clk_setl(DSPPLL_CON3, PLL_ISO_EN);
	wait_delay_us(1);
	clk_clrl(DSPPLL_CON3, PLL_PWR_ON);
	dsppll_enable = 0;
}

static int dsppll_get_enable(void)
{
	tr_dbg(&clkdrv_tr, "dsppll_enable=%d.\n", dsppll_enable);

	return dsppll_enable;
}

static int set_mux_sel(enum mux_id_t mux_id, uint32_t value)
{
	switch (mux_id) {
	case MUX_CLK_DSP_SEL:
		io_reg_update_bits(CLK_CFG_8_CLR, (0x7 << 24), (0x7 << 24));
		io_reg_update_bits(CLK_CFG_8_SET, (0x7 << 24), (value << 24));
		io_reg_write(CLK_CFG_UPDATE1, 0x8);

		tr_dbg(&clkdrv_tr, "adspclk_mux=%x, CLK_CFG_8=0x%08x\n",
		       value, io_reg_read(CLK_CFG_8));
		break;
	default:
		tr_dbg(&clkdrv_tr, "error: unknown mux_id (%d)\n", mux_id);
		break;
	}

	return 0;
}

static int clock_platform_set_dsp_freq(int clock, int freq_idx)
{
	uint32_t enc = cpu_freq_enc[freq_idx];
	int clk_mux;
	int adsp_clk_req = platform_cpu_freq[freq_idx].freq;

	if (adsp_clock == adsp_clk_req)
		return 0;

	tr_info(&clkdrv_tr, "clock_platform_set_cpu_freq %d\n", adsp_clk_req);

	/* convert res manager value to driver map */
	clk_mux = dsp_clk_value_convert(freq_idx);

	if (enc > 26000000) {
		/* adsp pll */
		if (!dsppll_get_enable()) {
			clk_dsppll_enable();
			set_mux_sel(MUX_CLK_DSP_SEL, clk_mux);
		}
		/* set adsp pll clock */
		io_reg_update_bits(DSPPLL_CON1, 0xffffffff, enc);
	} else {
		/* clk26m */
		if (dsppll_get_enable()) {
			set_mux_sel(MUX_CLK_DSP_SEL, clk_mux);
			clk_dsppll_disable();
		}
	}

	adsp_clock = adsp_clk_req;

	return 0;
}

void platform_clock_init(struct sof *sof)
{
	int i;

	sof->clocks = platform_shared_get(platform_clocks_info, sizeof(platform_clocks_info));

	for (i = 0; i < CONFIG_CORE_COUNT; i++) {
		sof->clocks[i] = (struct clock_info){
			.freqs_num = NUM_CPU_FREQ,
			.freqs = platform_cpu_freq,
			.default_freq_idx = CPU_DEFAULT_IDX,
			.current_freq_idx = CPU_DEFAULT_IDX,
			.notification_id = NOTIFIER_ID_CPU_FREQ,
			.notification_mask = NOTIFIER_TARGET_CORE_MASK(i),
			.set_freq = clock_platform_set_dsp_freq,
		};
	}

	adsp_clock = 0;
	dsppll_enable = 0;
	clock_set_freq(CLK_CPU(cpu_get_id()), CLK_MAX_CPU_HZ);
}
