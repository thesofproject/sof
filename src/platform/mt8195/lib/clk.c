// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Mediatek
//
// Author: YC Hung <yc.hung@mediatek.com>

#include <sof/common.h>
#include <rtos/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <rtos/wait.h>
#include <sof/sof.h>
#include <rtos/spinlock.h>

DECLARE_SOF_UUID("clkdrv", clkdrv_uuid, 0x23b12fd5, 0xc2a9, 0x41a8,
		 0xa2, 0xb3, 0x23, 0x1a, 0xb7, 0xdc, 0xdc, 0x70);

DECLARE_TR_CTX(clkdrv_tr, SOF_UUID(clkdrv_uuid), LOG_LEVEL_INFO);

static int dsppll_enable; /* default no adsp clock*/
static int adsp_clock;

/*Use external Ostimer*/
const struct freq_table platform_cpu_freq[] = {
	{ 13000000, 26000},
	{ 26000000, 26000},
	{ 370000000, 26000},
	{ 540000000, 26000},
	{ 720000000, 26000},
};

const uint32_t cpu_freq_enc[] = {
	13000000,
	26000000,
	0x831C7628,
	0x8214C4ED,
	0x821BB13C,
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
	case DSP_CLK_26M:
		ret = CLK_ADSP_SEL_26M;
		break;
	case DSP_CLK_PLL_370M:
	case DSP_CLK_PLL_540M:
	case DSP_CLK_PLL_720M:
		ret = CLK_ADSP_SEL_ADSPPLL;
		break;
	default:
		ret = CLK_ADSP_SEL_26M;
		break;
	}

	return ret;
}

static void clk_dsppll_enable(void)
{
	tr_dbg(&clkdrv_tr, "clk_dsppll_enable\n");

	io_reg_update_bits(AUDIODSP_CK_CG, 0x1 << RG_AUDIODSP_SW_CG, 0x0);
	clk_setl(DSPPLL_CON4, PLL_PWR_ON);
	wait_delay_us(1);
	clk_clrl(DSPPLL_CON4, PLL_ISO_EN);
	wait_delay_us(1);
	clk_setl(DSPPLL_CON0, PLL_EN);
	wait_delay_us(20);
	dsppll_enable = 1;
}

static void clk_dsppll_disable(void)
{
	tr_dbg(&clkdrv_tr, "clk_dsppll_disable\n");

	clk_clrl(DSPPLL_CON0, PLL_EN);
	wait_delay_us(1);
	clk_setl(DSPPLL_CON4, PLL_ISO_EN);
	wait_delay_us(1);
	clk_clrl(DSPPLL_CON4, PLL_PWR_ON);
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
	case MUX_CLK_ADSP_SEL:
		io_reg_update_bits(CLK_CFG_22_CLR, (0xF << 0), (0xF << 0));
		io_reg_update_bits(CLK_CFG_22_SET, (0xF << 0), (value << 0));
		io_reg_write(CLK_CFG_UPDATE2, 1 << CLK_UPDATE_ADSP_CK);

		tr_dbg(&clkdrv_tr, "adspclk_mux=%x, CLK_CFG_22=0x%08x\n",
		       value, io_reg_read(CLK_CFG_22));
		break;
	case MUX_CLK_AUDIO_LOCAL_BUS_SEL:
		io_reg_update_bits(CLK_CFG_28_CLR, (0xF << 16), (0xF << 16));
		io_reg_update_bits(CLK_CFG_28_SET, (0xF << 16), (value << 16));
		io_reg_write(CLK_CFG_UPDATE3, 1 << CLK_UPDATE_AUDIO_LOCAL_BUS_CK);

		tr_dbg(&clkdrv_tr, "audio_local_bus_clk_mux=%x, CLK_CFG_28=0x%08x\n",
		       value, io_reg_read(CLK_CFG_28));
		break;
	default:
		tr_dbg(&clkdrv_tr, "error: unknown mux_id (%d)\n", mux_id);
		break;
	}

	return 0;
}

static int clock_platform_set_cpu_freq(int clock, int freq_idx)
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
			set_mux_sel(MUX_CLK_ADSP_SEL, clk_mux);
			set_mux_sel(MUX_CLK_AUDIO_LOCAL_BUS_SEL,
				    CLK_AUDIO_LOCAL_BUS_SEL_MAINPLL_D_7);
		}
		/* set adsp pll clock */
		io_reg_update_bits(DSPPLL_CON2, 0xffffffff, enc);
	} else {
		/* clk26m */
		if (dsppll_get_enable()) {
			set_mux_sel(MUX_CLK_AUDIO_LOCAL_BUS_SEL, CLK_AUDIO_LOCAL_BUS_SEL_26M);
			set_mux_sel(MUX_CLK_ADSP_SEL, clk_mux);
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
			.set_freq = clock_platform_set_cpu_freq,
		};

		k_spinlock_init(&sof->clocks[i].lock);
	}

	adsp_clock = 0;
	dsppll_enable = 0;
}
