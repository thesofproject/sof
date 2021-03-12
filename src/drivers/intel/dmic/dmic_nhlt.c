// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/drivers/dmic.h>
#include <sof/math/numbers.h>
#include <ipc/dai.h>
#include <ipc/dai-intel.h>
#include <stdint.h>

#if CONFIG_INTEL_DMIC_NHLT

#include "nhlt_dmic_cfg.h"

/* Base addresses (in PDM scope) of 2ch PDM controllers and coefficient RAM. */
static const uint32_t base[4] = {PDM0, PDM1, PDM2, PDM3};
static const uint32_t coef_base_a[4] = {PDM0_COEFFICIENT_A, PDM1_COEFFICIENT_A,
					PDM2_COEFFICIENT_A, PDM3_COEFFICIENT_A};
static const uint32_t coef_base_b[4] = {PDM0_COEFFICIENT_B, PDM1_COEFFICIENT_B,
					PDM2_COEFFICIENT_B, PDM3_COEFFICIENT_B};

static int count_bits_set(uint32_t w)
{
	int val = 0;
	int i;

	for (i = 0; i < 32; i++) {
		val += w & 1;
		w = w >> 1;
	}

	return val;
}

static int nhlt_dmic_dai_params_get(struct dai *dai, uint32_t outcontrol[], uint32_t fir_control)
{
	struct dmic_pdata *pdata = dai_get_drvdata(dai);

	switch (OUTCONTROL0_OF_GET(outcontrol[dai->index])) {
	case 0:
	case 1:
		pdata->dai_format = SOF_IPC_FRAME_S16_LE;
		break;
	case 2:
		pdata->dai_format = SOF_IPC_FRAME_S32_LE;
		break;
	default:
		dai_err(dai, "nhlt_dmic_dai_params_get(): Illegal OF bit field");
		return -EINVAL;
	}

	switch (OUTCONTROL0_IPM_GET(outcontrol[dai->index])) {
	case 0:
	case 1:
		if (FIR_CONTROL_A_STEREO_GET(fir_control)) {
			pdata->dai_channels = 2;
			pdata->enable[0] = 0x3; /* Fixme PDM0 MIC A and B */
			pdata->enable[1] = 0x0;	/* Fixme PDM1 */

		} else {
			pdata->dai_channels = 1;
			pdata->enable[0] = 0x1; /* Fixme PDM0 MIC A */
			pdata->enable[1] = 0x0;	/* Fixme PDM1 */
		}
		break;
	case 2:
		if (FIR_CONTROL_A_STEREO_GET(fir_control)) {
			pdata->dai_channels = 4;
			pdata->enable[0] = 0x3; /* PDM0 MIC A and B */
			pdata->enable[1] = 0x3;	/* PDM1 MIC A and B */
		} else {
			dai_info(dai, "nhlt_dmic_dai_params_get(): Illegal 4ch configuration");
			return -EINVAL;
		}
		break;
	default:
		dai_info(dai, "nhlt_dmic_dai_params_get(): Illegal OF bit field");
		return -EINVAL;
	}

	return 0;
}

int dmic_set_config_nhlt(struct dai *dai, struct sof_ipc_dai_config *config)
{
	struct dmic_pdata *pdata = dai_get_drvdata(dai);
	struct nhlt_pdm_ctrl_cfg *pdm_cfg;
	struct nhlt_pdm_ctrl_fir_cfg *fir_cfg;
	struct nhlt_pdm_fir_coeffs *fir_a;
	struct nhlt_pdm_fir_coeffs *fir_b;
	uint32_t out_control[DMIC_HW_FIFOS_MAX];
	uint32_t channel_ctrl_mask;
	uint32_t pdm_ctrl_mask;
	uint32_t val;
	uint32_t fir_control = 0;
	const uint8_t *p = &nhlt_dmic_cfg[0]; /* Temporary blob, will be replaced by IPC */
	int num_fifos = 0;
	int num_pdm;
	int fir_length_a;
	int fir_length_b;
	int n;
	int i;
	int p_mcic = 0;
	int p_mfira = 0;
	int p_mfirb = 0;
	int p_clkdiv = 0;
	int clkdiv;
	int comb_count;
	int fir_decimation, fir_shift, fir_length;
	int bf1, bf2, bf3, bf4, bf5, bf6, bf7, bf8;
	int ret;

	/* Skip not used headers */
	p += sizeof(struct nhlt_ts_group);
	p += sizeof(struct nhlt_clock_on_delay);

	/* Channel_ctlr_mask bits indicate the FIFOs enabled*/
	channel_ctrl_mask = ((struct nhlt_channel_ctrl_mask *)p)->channel_ctrl_mask;
	num_fifos = count_bits_set(channel_ctrl_mask);
	p += sizeof(struct nhlt_channel_ctrl_mask);
	dai_info(dai, "dmic_set_config_nhlt(): channel_ctrl_mask = %d", channel_ctrl_mask);

	/* Get OUTCONTROLx configuration */
	for (n = 0; n < num_fifos; n++) {
		if (n < DMIC_HW_FIFOS_MAX) {
			val = (uint32_t)(*(uint32_t *)p);
			out_control[n] = val;
			bf1 = OUTCONTROL0_TIE_GET(val);
			bf2 = OUTCONTROL0_TIE_GET(val);
			bf3 = OUTCONTROL0_FINIT_GET(val);
			bf4 = OUTCONTROL0_FCI_GET(val);
			bf5 = OUTCONTROL0_BFTH_GET(val);
			bf6 = OUTCONTROL0_OF_GET(val);
			bf7 = OUTCONTROL0_IPM_GET(val);
			bf8 = OUTCONTROL0_TH_GET(val);
			dai_info(dai, "dmic_set_config_nhlt(): OUTCONTROL%d = %08x",
				 n, out_control[n]);
			dai_info(dai, "  tie=%d, sip=%d, finit=%d, fci=%d", bf1, bf2, bf3, bf4);
			dai_info(dai, "  bfth=%d, of=%d, ipm=%d, th=%d", bf5, bf6, bf7, bf8);
		} else {
			dai_err(dai, "dmic_set_config_nhlt(): OUTCONTROL%d is not supported", n);
			return -EINVAL;
		}
		p += sizeof(uint32_t);
	}

	/* Write the FIFO control registers. The clear/set of bits is the same for
	 * all DMIC_HW_VERSION
	 */
	if (dai->index == 0) {
		/* Clear TIE, SIP, FCI, set FINIT, the rest of bits as such */
		val = (out_control[0] &
			(~(OUTCONTROL0_TIE_BIT | OUTCONTROL0_SIP_BIT | OUTCONTROL0_FCI_BIT))) |
			OUTCONTROL0_FINIT_BIT;
		dai_write(dai, OUTCONTROL0, val);
		dai_info(dai, "dmic_set_config_nhlt(): OUTCONTROL0 = %08x", val);
	} else {
		/* Clear TIE, SIP, FCI, set FINIT, the rest of bits as such */
		val = (out_control[1] &
			(~(OUTCONTROL1_TIE_BIT | OUTCONTROL1_SIP_BIT | OUTCONTROL1_FCI_BIT))) |
			OUTCONTROL1_FINIT_BIT;
		dai_write(dai, OUTCONTROL1, val);
		dai_info(dai, "dmic_set_config_nhlt(): OUTCONTROL1 = %08x", val);
	}

	/* Get PDMx registers */
	pdm_ctrl_mask = ((struct nhlt_pdm_ctrl_mask *)p)->pdm_ctrl_mask;
	num_pdm = count_bits_set(pdm_ctrl_mask);
	p += sizeof(struct nhlt_pdm_ctrl_mask);
	dai_dbg(dai, "dmic_set_config_nhlt(): pdm_ctrl_mask = %d", pdm_ctrl_mask);

	for (n = 0; n < num_pdm; n++) {
		dai_info(dai, "dmic_set_config_nhlt(): PDM%d", n);

		/* Get CIC configuration */
		pdm_cfg = (struct nhlt_pdm_ctrl_cfg *)p;
		p += sizeof(struct nhlt_pdm_ctrl_cfg);

		comb_count = CIC_CONFIG_COMB_COUNT_GET(pdm_cfg->cic_config);
		p_mcic = comb_count + 1;
		clkdiv = MIC_CONTROL_PDM_CLKDIV_GET(pdm_cfg->mic_control);
		p_clkdiv = clkdiv + 2;
		if (*(pdata->dmic_active_fifos) == 0) {
			val = pdm_cfg->cic_control;
			bf1 = CIC_CONTROL_SOFT_RESET_GET(val);
			bf2 = CIC_CONTROL_CIC_START_B_GET(val);
			bf3 = CIC_CONTROL_CIC_START_A_GET(val);
			bf4 = CIC_CONTROL_MIC_B_POLARITY_GET(val);
			bf5 = CIC_CONTROL_MIC_A_POLARITY_GET(val);
			bf6 = CIC_CONTROL_MIC_MUTE_GET(val);
			bf7 = CIC_CONTROL_STEREO_MODE_GET(val);
			dai_dbg(dai, "dmic_set_config_nhlt(): CIC_CONTROL = %08x", val);
			dai_dbg(dai, "  soft_reset=%d, cic_start_b=%d, cic_start_a=%d",
				bf1, bf2, bf3);
			dai_dbg(dai, "  mic_b_polarity=%d, mic_a_polarity=%d, mic_mute=%d, stereo_mode=%d",
				bf4, bf5, bf6, bf7);

			/* Clear CIC_START_A and CIC_START_B, set SOF_RESET and MIC_MUTE*/
			val = (val &
				(~(CIC_CONTROL_CIC_START_A_BIT | CIC_CONTROL_CIC_START_A_BIT))) |
				CIC_CONTROL_SOFT_RESET_BIT | CIC_CONTROL_MIC_MUTE_BIT;
			dai_write(dai, base[n] + CIC_CONTROL, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): CIC_CONTROL = %08x", val);

			val = pdm_cfg->cic_config;
			bf1 = CIC_CONFIG_CIC_SHIFT_GET(val);
			dai_dbg(dai, "dmic_set_config_nhlt(): CIC_CONFIG = %08x", val);
			dai_dbg(dai, "  cic_shift=%d, comb_count=%d", bf1, comb_count);

			/* Use CIC_CONFIG as such */
			dai_write(dai, base[n] + CIC_CONFIG, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): CIC_CONFIG = %08x", val);

			val = pdm_cfg->mic_control;
			bf1 = MIC_CONTROL_PDM_SKEW_GET(val);
			bf2 = MIC_CONTROL_PDM_CLK_EDGE_GET(val);
			bf3 = MIC_CONTROL_PDM_EN_B_GET(val);
			bf4 = MIC_CONTROL_PDM_EN_A_GET(val);
			dai_dbg(dai, "dmic_set_config_nhlt(): MIC_CONTROL = %08x", val);
			dai_dbg(dai, "  clkdiv=%d, skew=%d, clk_edge=%d", clkdiv, bf1, bf2);
			dai_dbg(dai, "  en_b=%d, en_a=%d", bf3, bf4);

			/* Clear PDM_EN_A and PDM_EN_B */
			val = val & (~(MIC_CONTROL_PDM_EN_A_BIT | MIC_CONTROL_PDM_EN_B_BIT));
			dai_write(dai, base[n] + MIC_CONTROL, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): MIC_CONTROL = %08x", val);
		}

		/* FIR A */
		fir_cfg = (struct nhlt_pdm_ctrl_fir_cfg *)p;
		p += sizeof(struct nhlt_pdm_ctrl_fir_cfg);
		val = fir_cfg->fir_config;
		fir_length = FIR_CONFIG_A_FIR_LENGTH_GET(val);
		fir_length_a = fir_length + 1; /* Need for parsing */
		fir_decimation = FIR_CONFIG_A_FIR_DECIMATION_GET(val);
		p_mfira = fir_decimation + 1;
		if (dai->index == 0) {
			fir_shift = FIR_CONFIG_A_FIR_SHIFT_GET(val);
			dai_dbg(dai, "dmic_set_config_nhlt(): FIR_CONFIG_A = %08x", val);
			dai_dbg(dai, "  fir_decimation=%d, fir_shift=%d, fir_length=%d",
				fir_decimation, fir_shift, fir_length);

			/* Use FIR_CONFIG_A as such */
			dai_write(dai, base[n] + FIR_CONFIG_A, val);
			dai_dbg(dai, "configure_registers(), FIR_CONFIG_A = %08x", val);

			val = fir_cfg->fir_control;
			bf1 = FIR_CONTROL_A_START_GET(val);
			bf2 = FIR_CONTROL_A_ARRAY_START_EN_GET(val);
			bf3 = FIR_CONTROL_A_DCCOMP_GET(val);
			bf4 = FIR_CONTROL_A_MUTE_GET(val);
			bf5 = FIR_CONTROL_A_STEREO_GET(val);
			dai_dbg(dai, "dmic_set_config_nhlt(): FIR_CONTROL_A = %08x", val);
			dai_dbg(dai, "  start=%d, array_start_en=%d, dccomp=%d", bf1, bf2, bf3);
			dai_dbg(dai, "  mute=%d, stereo=%d", bf4, bf5);

			/* Clear START, set MUTE */
			fir_control = (val & ~FIR_CONTROL_A_START_BIT) | FIR_CONTROL_A_MUTE_BIT;
			dai_write(dai, base[n] + FIR_CONTROL_A, fir_control);
			dai_dbg(dai, "dmic_set_config_nhlt(): FIR_CONTROL_A = %08x", fir_control);

			/* Use DC_OFFSET and GAIN as such */
			val = fir_cfg->dc_offset_left;
			dai_write(dai, base[n] + DC_OFFSET_LEFT_A, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): DC_OFFSET_LEFT_A = %08x", val);

			val = fir_cfg->dc_offset_right;
			dai_write(dai, base[n] + DC_OFFSET_RIGHT_A, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): DC_OFFSET_RIGHT_A = %08x", val);

			val = fir_cfg->out_gain_left;
			dai_write(dai, base[n] + OUT_GAIN_LEFT_A, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): OUT_GAIN_LEFT_A = %08x", val);

			val = fir_cfg->out_gain_right;
			dai_write(dai, base[n] + OUT_GAIN_RIGHT_A, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): OUT_GAIN_RIGHT_A = %08x", val);
		}

		/* FIR B */
		fir_cfg = (struct nhlt_pdm_ctrl_fir_cfg *)p;
		p += sizeof(struct nhlt_pdm_ctrl_fir_cfg);
		val = fir_cfg->fir_config;
		fir_length = FIR_CONFIG_B_FIR_LENGTH_GET(val);
		fir_length_b = fir_length + 1; /* Need for parsing */
		fir_decimation = FIR_CONFIG_B_FIR_DECIMATION_GET(val);
		p_mfirb = fir_decimation + 1;
		if (dai->index == 1) {
			fir_shift = FIR_CONFIG_B_FIR_SHIFT_GET(val);
			dai_dbg(dai, "dmic_set_config_nhlt(): FIR_CONFIG_B = %08x", val);
			dai_dbg(dai, "  fir_decimation=%d, fir_shift=%d, fir_length=%d",
				 fir_decimation, fir_shift, fir_length);

			/* Use FIR_CONFIG_B as such */
			dai_write(dai, base[n] + FIR_CONFIG_B, val);
			dai_dbg(dai, "configure_registers(), FIR_CONFIG_B = %08x", val);

			val = fir_cfg->fir_control;
			bf1 = FIR_CONTROL_B_START_GET(val);
			bf2 = FIR_CONTROL_B_ARRAY_START_EN_GET(val);
			bf3 = FIR_CONTROL_B_DCCOMP_GET(val);
			bf5 = FIR_CONTROL_B_MUTE_GET(val);
			bf6 = FIR_CONTROL_B_STEREO_GET(val);
			dai_dbg(dai, "dmic_set_config_nhlt(): FIR_CONTROL_B = %08x", val);
			dai_dbg(dai, "  start=%d, array_start_en=%d, dccomp=%d", bf1, bf2, bf3);
			dai_dbg(dai, "  mute=%d, stereo=%d", bf5, bf6);

			/* Clear START, set MUTE */
			fir_control = (val & ~FIR_CONTROL_B_START_BIT) | FIR_CONTROL_B_MUTE_BIT;
			dai_write(dai, base[n] + FIR_CONTROL_B, fir_control);
			dai_dbg(dai, "dmic_set_config_nhlt(): FIR_CONTROL_B = %08x", fir_control);

			/* Use DC_OFFSET and GAIN as such */
			val = fir_cfg->dc_offset_left;
			dai_write(dai, base[n] + DC_OFFSET_LEFT_B, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): DC_OFFSET_LEFT_B = %08x", val);

			val = fir_cfg->dc_offset_right;
			dai_write(dai, base[n] + DC_OFFSET_RIGHT_B, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): DC_OFFSET_RIGHT_B = %08x", val);

			val = fir_cfg->out_gain_left;
			dai_write(dai, base[n] + OUT_GAIN_LEFT_B, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): OUT_GAIN_LEFT_B = %08x", val);

			val = fir_cfg->out_gain_right;
			dai_write(dai, base[n] + OUT_GAIN_RIGHT_B, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): OUT_GAIN_RIGHT_B = %08x", val);
		}

		fir_a = (struct nhlt_pdm_fir_coeffs *)p;
		p += sizeof(int32_t) * fir_length_a;
		fir_b = (struct nhlt_pdm_fir_coeffs *)p;
		p += sizeof(int32_t) * fir_length_b;

		if (dai->index == 0) {
			dai_info(dai, "dmic_set_config_nhlt(): clkdiv = %d, mcic = %d, mfir_a = %d, length = %d",
				 p_clkdiv, p_mcic, p_mfira, fir_length_a);
			for (i = 0; i < fir_length_a; i++)
				dai_write(dai, coef_base_a[n] + (i << 2), fir_a->fir_coeffs[i]);
		} else {
			dai_info(dai, "dmic_set_config_nhlt(): clkdiv = %d, mcic = %d, mfir_b = %d, length = %d",
				 p_clkdiv, p_mcic, p_mfirb, fir_length_b);
			for (i = 0; i < fir_length_b; i++)
				dai_write(dai, coef_base_b[n] + (i << 2), fir_b->fir_coeffs[i]);
		}
	}

	ret = nhlt_dmic_dai_params_get(dai, out_control, fir_control);
	if (ret)
		return ret;

	if (dai->index == 0)
		pdata->dai_rate = DMIC_HW_IOCLK / p_clkdiv / p_mcic / p_mfira;
	else
		pdata->dai_rate = DMIC_HW_IOCLK / p_clkdiv / p_mcic / p_mfirb;

	dai_info(dai, "dmic_set_config_nhlt(): rate = %d, channels = %d, format = %d",
		 pdata->dai_rate, pdata->dai_channels, pdata->dai_format);

	return 0;
}

int dmic_get_hw_params_nhlt(struct dai *dai, struct sof_ipc_stream_params  *params, int dir)
{
	struct dmic_pdata *pdata = dai_get_drvdata(dai);

	params->frame_fmt = pdata->dai_format;
	params->channels = pdata->dai_channels;
	params->rate = pdata->dai_rate;
	return 0;
}

#endif
