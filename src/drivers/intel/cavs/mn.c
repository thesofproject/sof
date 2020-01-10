// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/drivers/mn.h>
#include <sof/drivers/ssp.h>
#include <sof/lib/shim.h>
#include <sof/math/numbers.h>
#include <sof/trace/trace.h>

/* tracing */
#define trace_mn(__e, ...) \
	trace_event(TRACE_CLASS_MN, __e, ##__VA_ARGS__)
#define trace_mn_error(__e, ...) \
	trace_error(TRACE_CLASS_MN, __e, ##__VA_ARGS__)
#define tracev_mn(__e, ...) \
	tracev_event(TRACE_CLASS_MN, __e, ##__VA_ARGS__)

int mn_set_mclk(uint16_t mclk_id, uint32_t mclk_rate)
{
	uint32_t mdivr;
	uint32_t mdivr_val;
	uint32_t mdivc = mn_reg_read(MN_MDIVCTRL);
	int i;
	int clk_index = -1;

	if (mclk_id > 1) {
		trace_mn_error("error: mclk ID (%d) > 1", mclk_id);
		return -EINVAL;
	}

	/* Enable MCLK Divider */
	mdivc |= MN_MDIVCTRL_M_DIV_ENABLE;

	/* searching the smallest possible mclk source */
	for (i = MAX_SSP_FREQ_INDEX; i >= 0; i--) {
		if (mclk_rate > ssp_freq[i].freq)
			break;

		if (ssp_freq[i].freq % mclk_rate == 0)
			clk_index = i;
	}

	if (clk_index >= 0) {
		mdivc |= MCDSS(ssp_freq_sources[clk_index]);
	} else {
		trace_mn_error("error: MCLK %d", mclk_rate);
		return -EINVAL;
	}

	mdivr_val = ssp_freq[clk_index].freq / mclk_rate;

	switch (mdivr_val) {
	case 1:
		mdivr = 0x00000fff; /* bypass divider for MCLK */
		break;
	case 2:
		mdivr = 0x0; /* 1/2 */
		break;
	case 4:
		mdivr = 0x2; /* 1/4 */
		break;
	case 8:
		mdivr = 0x6; /* 1/8 */
		break;
	default:
		trace_mn_error("error: invalid mdivr_val %d", mdivr_val);
		return -EINVAL;
	}

	mn_reg_write(MN_MDIVCTRL, mdivc);
	mn_reg_write(MN_MDIVR(mclk_id), mdivr);

	return 0;
}

/**
 * \brief Finds valid M/(N * SCR) values for given frequencies.
 * \param[in] freq SSP clock frequency.
 * \param[in] bclk Bit clock frequency.
 * \param[out] out_scr_div SCR divisor.
 * \param[out] out_m M value of M/N divider.
 * \param[out] out_n N value of M/N divider.
 * \return true if found suitable values, false otherwise.
 */
static bool find_mn(uint32_t freq, uint32_t bclk,
		    uint32_t *out_scr_div, uint32_t *out_m, uint32_t *out_n)
{
	uint32_t m, n, mn_div;
	uint32_t scr_div;

	/* M/(N * scr_div) has to be less than 1/2 */
	if ((bclk * 2) >= freq)
		return false;

	scr_div = freq / bclk;

	/* odd SCR gives lower duty cycle */
	if (scr_div > 1 && scr_div % 2 != 0)
		--scr_div;

	/* clamp to valid SCR range */
	scr_div = MIN(scr_div, (SSCR0_SCR_MASK >> 8) + 1);

	/* find highest even divisor */
	while (scr_div > 1 && freq % scr_div != 0)
		scr_div -= 2;

	/* compute M/N with smallest dividend and divisor */
	mn_div = gcd(bclk, freq / scr_div);

	m = bclk / mn_div;
	n = freq / scr_div / mn_div;

	/* M/N values can be up to 24 bits */
	if (n & (~0xffffff))
		return false;

	*out_scr_div = scr_div;
	*out_m = m;
	*out_n = n;

	return true;
}

int mn_set_bclk(uint32_t dai_index, uint32_t bclk_rate,
		uint32_t *out_scr_div, bool *out_need_ecs)
{
	uint32_t i2s_m = 1;
	uint32_t i2s_n = 1;
	uint32_t mdivc = mn_reg_read(MN_MDIVCTRL);
	int i;
	int clk_index = -1;

	*out_need_ecs = false;

	/* searching the smallest possible bclk source */
	for (i = MAX_SSP_FREQ_INDEX; i >= 0; i--) {
		if (bclk_rate > ssp_freq[i].freq)
			break;

		if (ssp_freq[i].freq % bclk_rate == 0)
			clk_index = i;
	}

	if (clk_index >= 0) {
		mdivc |= MNDSS(ssp_freq_sources[clk_index]);
		*out_scr_div = ssp_freq[clk_index].freq / bclk_rate;

		/* select M/N output for bclk in case of Audio Cardinal
		 * or PLL Fixed clock.
		 */
		if (ssp_freq_sources[clk_index] != SSP_CLOCK_XTAL_OSCILLATOR)
			*out_need_ecs = true;
	} else {
		/* check if we can get target BCLK with M/N */
		for (i = 0; i <= MAX_SSP_FREQ_INDEX; i++) {
			if (find_mn(ssp_freq[i].freq, bclk_rate,
				    out_scr_div, &i2s_m, &i2s_n)) {
				clk_index = i;
				break;
			}
		}

		if (clk_index < 0) {
			trace_mn_error("error: BCLK %d", bclk_rate);
			return -EINVAL;
		}

		trace_mn("M = %d, N = %d", i2s_m, i2s_n);

		mdivc |= MNDSS(ssp_freq_sources[clk_index]);

		/* M/N requires external clock to be selected */
		*out_need_ecs = true;
	}

	mn_reg_write(MN_MDIVCTRL, mdivc);
	mn_reg_write(MN_MDIV_M_VAL(dai_index), i2s_m);
	mn_reg_write(MN_MDIV_N_VAL(dai_index), i2s_n);

	return 0;
}

void mn_reset_bclk_divider(uint32_t dai_index)
{
	mn_reg_write(MN_MDIV_M_VAL(dai_index), 1);
	mn_reg_write(MN_MDIV_N_VAL(dai_index), 1);
}
