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

LOG_MODULE_DECLARE(dmic_dai, CONFIG_SOF_LOG_LEVEL);

/* Base addresses (in PDM scope) of 2ch PDM controllers and coefficient RAM. */
static const uint32_t base[4] = {PDM0, PDM1, PDM2, PDM3};
static const uint32_t coef_base_a[4] = {PDM0_COEFFICIENT_A, PDM1_COEFFICIENT_A,
					PDM2_COEFFICIENT_A, PDM3_COEFFICIENT_A};
static const uint32_t coef_base_b[4] = {PDM0_COEFFICIENT_B, PDM1_COEFFICIENT_B,
					PDM2_COEFFICIENT_B, PDM3_COEFFICIENT_B};

#if defined DMIC_IPM_VER1
static int nhlt_dmic_dai_params_get(struct dai *dai, uint32_t *outcontrol,
				    struct nhlt_pdm_ctrl_cfg **pdm_cfg,
				    struct nhlt_pdm_ctrl_fir_cfg **fir_cfg)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	int fir_stereo[2];
	int mic_swap;

	switch (OUTCONTROL0_OF_GET(outcontrol[dai->index])) {
	case 0:
	case 1:
		dmic->dai_format = SOF_IPC_FRAME_S16_LE;
		break;
	case 2:
		dmic->dai_format = SOF_IPC_FRAME_S32_LE;
		break;
	default:
		dai_err(dai, "nhlt_dmic_dai_params_get(): Illegal OF bit field");
		return -EINVAL;
	}

	switch (OUTCONTROL0_IPM_GET(outcontrol[dai->index])) {
	case 0:
		if (!fir_cfg[0])
			return -EINVAL;

		fir_stereo[0] = FIR_CONTROL_A_STEREO_GET(fir_cfg[0]->fir_control);
		if (fir_stereo[0]) {
			dmic->dai_channels = 2;
			dmic->enable[0] = 0x3; /* PDM0 MIC A and B */
			dmic->enable[1] = 0x0;	/* PDM1 none */

		} else {
			dmic->dai_channels = 1;
			mic_swap = MIC_CONTROL_PDM_CLK_EDGE_GET(pdm_cfg[0]->mic_control);
			dmic->enable[0] = mic_swap ? 0x2 : 0x1; /* PDM0 MIC B or MIC A */
			dmic->enable[1] = 0x0;	/* PDM1 */
		}
		break;
	case 1:
		if (!fir_cfg[1])
			return -EINVAL;

		fir_stereo[1] = FIR_CONTROL_A_STEREO_GET(fir_cfg[1]->fir_control);
		if (fir_stereo[1]) {
			dmic->dai_channels = 2;
			dmic->enable[0] = 0x0; /* PDM0 none */
			dmic->enable[1] = 0x3;	/* PDM1 MIC A and B */

		} else {
			dmic->dai_channels = 1;
			dmic->enable[0] = 0x0; /* PDM0 none */
			mic_swap = MIC_CONTROL_PDM_CLK_EDGE_GET(pdm_cfg[1]->mic_control);
			dmic->enable[1] = mic_swap ? 0x2 : 0x1; /* PDM1 MIC B or MIC A */
		}
		break;
	case 2:
		if (!fir_cfg[0] || !fir_cfg[0])
			return -EINVAL;

		fir_stereo[0] = FIR_CONTROL_A_STEREO_GET(fir_cfg[0]->fir_control);
		fir_stereo[1] = FIR_CONTROL_A_STEREO_GET(fir_cfg[1]->fir_control);
		if (fir_stereo[0] == fir_stereo[1]) {
			dmic->dai_channels = 4;
			dmic->enable[0] = 0x3; /* PDM0 MIC A and B */
			dmic->enable[1] = 0x3;	/* PDM1 MIC A and B */
		} else {
			dai_err(dai, "nhlt_dmic_dai_params_get(): Illegal 4ch configuration");
			return -EINVAL;
		}
		break;
	default:
		dai_err(dai, "nhlt_dmic_dai_params_get(): Illegal OF bit field");
		return -EINVAL;
	}

	return 0;
}
#endif

#if defined DMIC_IPM_VER2

static int ipm_source_to_enable(struct dmic_pdata *dmic, struct nhlt_pdm_ctrl_cfg **pdm_cfg,
				int *count, int pdm_count, int stereo, int source_pdm)
{
	int mic_swap;

	if (source_pdm >= DMIC_HW_CONTROLLERS)
		return -EINVAL;

	if (*count < pdm_count) {
		(*count)++;
		mic_swap = MIC_CONTROL_PDM_CLK_EDGE_GET(pdm_cfg[source_pdm]->mic_control);
		if (stereo)
			dmic->enable[source_pdm] = 0x3; /* PDMi MIC A and B */
		else
			dmic->enable[source_pdm] = mic_swap ? 0x2 : 0x1; /* PDMi MIC B or MIC A */
	}

	return 0;
}

static int nhlt_dmic_dai_params_get(struct dai *dai, uint32_t *outcontrol,
				    struct nhlt_pdm_ctrl_cfg **pdm_cfg,
				    struct nhlt_pdm_ctrl_fir_cfg **fir_cfg)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	uint32_t outcontrol_val = outcontrol[dai->index];
	int num_pdm;
	int source_pdm;
	int ret;
	int n;
	bool stereo_pdm;

	switch (OUTCONTROL0_OF_GET(outcontrol_val)) {
	case 0:
	case 1:
		dmic->dai_format = SOF_IPC_FRAME_S16_LE;
		break;
	case 2:
		dmic->dai_format = SOF_IPC_FRAME_S32_LE;
		break;
	default:
		dai_err(dai, "nhlt_dmic_dai_params_get(): Illegal OF bit field");
		return -EINVAL;
	}

	num_pdm = OUTCONTROL0_IPM_GET(outcontrol_val);
	if (num_pdm > DMIC_HW_CONTROLLERS) {
		dai_err(dai, "nhlt_dmic_dai_params_get(): Illegal IPM PDM controllers count");
		return -EINVAL;
	}

	stereo_pdm = 1;

	dmic->dai_channels = (stereo_pdm + 1) * num_pdm;
	for (n = 0; n < DMIC_HW_CONTROLLERS; n++)
		dmic->enable[n] = 0;

	n = 0;
	source_pdm = OUTCONTROL0_IPM_SOURCE_1_GET(outcontrol_val);
	ret = ipm_source_to_enable(dmic, pdm_cfg, &n, num_pdm, stereo_pdm, source_pdm);
	if (ret) {
		dai_err(dai, "nhlt_dmic_dai_params_get(): Illegal IPM_SOURCE_1");
		return -EINVAL;
	}

	source_pdm = OUTCONTROL0_IPM_SOURCE_2_GET(outcontrol_val);
	ret = ipm_source_to_enable(dmic, pdm_cfg, &n, num_pdm, stereo_pdm, source_pdm);
	if (ret) {
		dai_err(dai, "nhlt_dmic_dai_params_get(): Illegal IPM_SOURCE_2");
		return -EINVAL;
	}

	source_pdm = OUTCONTROL0_IPM_SOURCE_3_GET(outcontrol_val);
	ret = ipm_source_to_enable(dmic, pdm_cfg, &n, num_pdm, stereo_pdm, source_pdm);
	if (ret) {
		dai_err(dai, "nhlt_dmic_dai_params_get(): Illegal IPM_SOURCE_3");
		return -EINVAL;
	}

	source_pdm = OUTCONTROL0_IPM_SOURCE_4_GET(outcontrol_val);
	ret = ipm_source_to_enable(dmic, pdm_cfg, &n, num_pdm, stereo_pdm, source_pdm);
	if (ret) {
		dai_err(dai, "nhlt_dmic_dai_params_get(): Illegal IPM_SOURCE_4");
		return -EINVAL;
	}

	return 0;
}
#endif

int dmic_set_config_nhlt(struct dai *dai, void *spec_config)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);
	struct nhlt_pdm_ctrl_cfg *pdm_cfg[DMIC_HW_CONTROLLERS_MAX];
	struct nhlt_pdm_ctrl_fir_cfg *fir_cfg_a[DMIC_HW_CONTROLLERS_MAX];
	struct nhlt_pdm_ctrl_fir_cfg *fir_cfg_b[DMIC_HW_CONTROLLERS_MAX];
	struct nhlt_pdm_fir_coeffs *fir_a[DMIC_HW_CONTROLLERS_MAX] = {NULL};
	struct nhlt_pdm_fir_coeffs *fir_b[DMIC_HW_CONTROLLERS_MAX];
	uint32_t out_control[DMIC_HW_FIFOS_MAX] = {0};
	uint32_t channel_ctrl_mask;
	uint32_t fir_control;
	uint32_t pdm_ctrl_mask;
	uint32_t ref = 0;
	uint32_t val;
	const uint8_t *p = spec_config;
	int num_fifos;
	int num_pdm;
	int fir_length_a;
	int fir_length_b;
	int n;
	int i;
	int rate_div;
	int clk_div;
	int comb_count;
	int fir_decimation, fir_shift, fir_length;
	int bf1, bf2, bf3, bf4, bf5, bf6, bf7, bf8;
#if defined DMIC_IPM_VER2
	int bf9, bf10, bf11, bf12, bf13;
#endif
	int bfth;
	int ret;
	int p_mcic = 0;
	int p_mfira = 0;
	int p_mfirb = 0;
	int p_clkdiv = 0;

	if (dai->index >= DMIC_HW_FIFOS_MAX) {
		dai_err(dai, "dmic_set_config_nhlt(): illegal DAI index %d", dai->index);
		return -EINVAL;
	}

	/* Skip not used headers */
	p += sizeof(struct nhlt_dmic_gateway_attributes);
	p += sizeof(struct nhlt_dmic_ts_group);
	p += sizeof(struct nhlt_dmic_clock_on_delay);

	/* Channel_ctlr_mask bits indicate the FIFOs enabled*/
	channel_ctrl_mask = ((struct nhlt_dmic_channel_ctrl_mask *)p)->channel_ctrl_mask;
	num_fifos = popcount(channel_ctrl_mask); /* Count set bits */
	p += sizeof(struct nhlt_dmic_channel_ctrl_mask);
	dai_dbg(dai, "dmic_set_config_nhlt(): channel_ctrl_mask = %d", channel_ctrl_mask);

	/* Get OUTCONTROLx configuration */
	if (num_fifos < 1 || num_fifos > DMIC_HW_FIFOS_MAX) {
		dai_err(dai, "dmic_set_config_nhlt(): illegal number of FIFOs %d", num_fifos);
		return -EINVAL;
	}

	for (n = 0; n < DMIC_HW_FIFOS_MAX; n++) {
		if (!(channel_ctrl_mask & (1 << n)))
			continue;

		val = *(uint32_t *)p;
		out_control[n] = val;
		bf1 = OUTCONTROL0_TIE_GET(val);
		bf2 = OUTCONTROL0_SIP_GET(val);
		bf3 = OUTCONTROL0_FINIT_GET(val);
		bf4 = OUTCONTROL0_FCI_GET(val);
		bf5 = OUTCONTROL0_BFTH_GET(val);
		bf6 = OUTCONTROL0_OF_GET(val);
		bf7 = OUTCONTROL0_IPM_GET(val);
		bf8 = OUTCONTROL0_TH_GET(val);
		dai_info(dai, "dmic_set_config_nhlt(): OUTCONTROL%d = %08x", n, out_control[n]);
		dai_info(dai, "  tie=%d, sip=%d, finit=%d, fci=%d", bf1, bf2, bf3, bf4);
		dai_info(dai, "  bfth=%d, of=%d, ipm=%d, th=%d", bf5, bf6, bf7, bf8);
		if (bf5 > OUTCONTROL0_BFTH_MAX) {
			dai_err(dai, "dmic_set_config_nhlt(): illegal BFTH value");
			return -EINVAL;
		}

#if defined DMIC_IPM_VER1
		ref = OUTCONTROL0_TIE(bf1) | OUTCONTROL0_SIP(bf2) | OUTCONTROL0_FINIT(bf3) |
			OUTCONTROL0_FCI(bf4) | OUTCONTROL0_BFTH(bf5) | OUTCONTROL0_OF(bf6) |
			OUTCONTROL0_IPM(bf7) | OUTCONTROL0_TH(bf8);
#elif defined DMIC_IPM_VER2
		bf9 = OUTCONTROL0_IPM_SOURCE_1_GET(val);
		bf10 = OUTCONTROL0_IPM_SOURCE_2_GET(val);
		bf11 = OUTCONTROL0_IPM_SOURCE_3_GET(val);
		bf12 = OUTCONTROL0_IPM_SOURCE_4_GET(val);
		bf13 = OUTCONTROL0_IPM_SOURCE_MODE_GET(val);
		dai_info(dai, "  ipms1=%d, ipms2=%d, ipms3=%d, ipms4=%d", bf9, bf10, bf11, bf12);
		dai_info(dai, "  ipms_mode=%d", bf13);
		ref = OUTCONTROL0_TIE(bf1) | OUTCONTROL0_SIP(bf2) | OUTCONTROL0_FINIT(bf3) |
			OUTCONTROL0_FCI(bf4) | OUTCONTROL0_BFTH(bf5) | OUTCONTROL0_OF(bf6) |
			OUTCONTROL0_IPM(bf7) | OUTCONTROL0_IPM_SOURCE_1(bf9) |
			OUTCONTROL0_IPM_SOURCE_2(bf10) | OUTCONTROL0_IPM_SOURCE_3(bf11) |
			OUTCONTROL0_IPM_SOURCE_4(bf12) | OUTCONTROL0_TH(bf8) |
			OUTCONTROL0_IPM_SOURCE_MODE(bf13);
#endif
		if (ref != val) {
			dai_err(dai, "dmic_set_config_nhlt(): illegal OUTCONTROL%d = 0x%08x",
				n, val);
			return -EINVAL;
		}

		p += sizeof(uint32_t);
	}

	/* Write the FIFO control registers. The clear/set of bits is the same for
	 * all DMIC_HW_VERSION
	 */
	/* Clear TIE, SIP, FCI, set FINIT, the rest of bits as such */
	val = (out_control[dai->index] &
		~(OUTCONTROL0_TIE_BIT | OUTCONTROL0_SIP_BIT | OUTCONTROL0_FCI_BIT)) |
		OUTCONTROL0_FINIT_BIT;
	if (dai->index == 0)
		dai_write(dai, OUTCONTROL0, val);
	else
		dai_write(dai, OUTCONTROL1, val);

	dai_info(dai, "dmic_set_config_nhlt(): OUTCONTROL%d = %08x", dai->index, val);

	/* Pass 2^BFTH to plat_data fifo depth. It will be used later in DMA
	 * configuration
	 */
	bfth = OUTCONTROL0_BFTH_GET(val);
	dai->plat_data.fifo[SOF_IPC_STREAM_CAPTURE].depth = 1 << bfth;

	/* Get PDMx registers */
	pdm_ctrl_mask = ((struct nhlt_pdm_ctrl_mask *)p)->pdm_ctrl_mask;
	num_pdm = popcount(pdm_ctrl_mask); /* Count set bits */
	p += sizeof(struct nhlt_pdm_ctrl_mask);
	dai_dbg(dai, "dmic_set_config_nhlt(): pdm_ctrl_mask = %d", pdm_ctrl_mask);
	if (num_pdm < 1 || num_pdm > DMIC_HW_CONTROLLERS) {
		dai_err(dai, "dmic_set_config_nhlt(): illegal number of PDMs %d", num_pdm);
		return -EINVAL;
	}

	for (n = 0; n < DMIC_HW_CONTROLLERS; n++) {
		fir_cfg_a[n] = NULL;
		fir_cfg_b[n] = NULL;
		if (!(pdm_ctrl_mask & (1 << n)))
			continue;

		dai_dbg(dai, "dmic_set_config_nhlt(): PDM%d", n);

		/* Get CIC configuration */
		pdm_cfg[n] = (struct nhlt_pdm_ctrl_cfg *)p;
		p += sizeof(struct nhlt_pdm_ctrl_cfg);

		comb_count = CIC_CONFIG_COMB_COUNT_GET(pdm_cfg[n]->cic_config);
		p_mcic = comb_count + 1;
		clk_div = MIC_CONTROL_PDM_CLKDIV_GET(pdm_cfg[n]->mic_control);
		p_clkdiv = clk_div + 2;
		if (dmic->global->active_fifos_mask == 0) {
			val = pdm_cfg[n]->cic_control;
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
			dai_dbg(dai, "  mic_b_polarity=%d, mic_a_polarity=%d, mic_mute=%d",
				bf4, bf5, bf6);
			ref = CIC_CONTROL_SOFT_RESET(bf1) | CIC_CONTROL_CIC_START_B(bf2) |
				CIC_CONTROL_CIC_START_A(bf3) | CIC_CONTROL_MIC_B_POLARITY(bf4) |
				CIC_CONTROL_MIC_A_POLARITY(bf5) | CIC_CONTROL_MIC_MUTE(bf6) |
				CIC_CONTROL_STEREO_MODE(bf7);
			dai_dbg(dai, "  stereo_mode=%d", bf7);
			if (ref != val) {
				dai_err(dai, "dmic_set_config_nhlt(): illegal CIC_CONTROL = 0x%08x",
					val);
				return -EINVAL;
			}

			/* Clear CIC_START_A and CIC_START_B, set SOF_RESET and MIC_MUTE*/
			val = (val & ~(CIC_CONTROL_CIC_START_A_BIT | CIC_CONTROL_CIC_START_A_BIT)) |
				CIC_CONTROL_SOFT_RESET_BIT | CIC_CONTROL_MIC_MUTE_BIT;
			dai_write(dai, base[n] + CIC_CONTROL, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): CIC_CONTROL = %08x", val);

			val = pdm_cfg[n]->cic_config;
			bf1 = CIC_CONFIG_CIC_SHIFT_GET(val);
			dai_dbg(dai, "dmic_set_config_nhlt(): CIC_CONFIG = %08x", val);
			dai_dbg(dai, "  cic_shift=%d, comb_count=%d", bf1, comb_count);

			/* Use CIC_CONFIG as such */
			dai_write(dai, base[n] + CIC_CONFIG, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): CIC_CONFIG = %08x", val);

			val = pdm_cfg[n]->mic_control;
			bf1 = MIC_CONTROL_PDM_SKEW_GET(val);
			bf2 = MIC_CONTROL_PDM_CLK_EDGE_GET(val);
			bf3 = MIC_CONTROL_PDM_EN_B_GET(val);
			bf4 = MIC_CONTROL_PDM_EN_A_GET(val);
			dai_dbg(dai, "dmic_set_config_nhlt(): MIC_CONTROL = %08x", val);
			dai_dbg(dai, "  clkdiv=%d, skew=%d, clk_edge=%d", clk_div, bf1, bf2);
			dai_dbg(dai, "  en_b=%d, en_a=%d", bf3, bf4);

			/* Clear PDM_EN_A and PDM_EN_B */
			val &= ~(MIC_CONTROL_PDM_EN_A_BIT | MIC_CONTROL_PDM_EN_B_BIT);
			dai_write(dai, base[n] + MIC_CONTROL, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): MIC_CONTROL = %08x", val);
		}

		/* FIR A */
		fir_cfg_a[n] = (struct nhlt_pdm_ctrl_fir_cfg *)p;
		p += sizeof(struct nhlt_pdm_ctrl_fir_cfg);
		val = fir_cfg_a[n]->fir_config;
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

			val = fir_cfg_a[n]->fir_control;
			bf1 = FIR_CONTROL_A_START_GET(val);
			bf2 = FIR_CONTROL_A_ARRAY_START_EN_GET(val);
			bf3 = FIR_CONTROL_A_DCCOMP_GET(val);
			bf4 = FIR_CONTROL_A_MUTE_GET(val);
			bf5 = FIR_CONTROL_A_STEREO_GET(val);
			dai_dbg(dai, "dmic_set_config_nhlt(): FIR_CONTROL_A = %08x", val);
			dai_dbg(dai, "  start=%d, array_start_en=%d, dccomp=%d", bf1, bf2, bf3);
			dai_dbg(dai, "  mute=%d, stereo=%d", bf4, bf5);
			ref = FIR_CONTROL_A_START(bf1) | FIR_CONTROL_A_ARRAY_START_EN(bf2) |
				FIR_CONTROL_A_DCCOMP(bf3) | FIR_CONTROL_A_MUTE(bf4) |
				FIR_CONTROL_A_STEREO(bf5);

			if (ref != val) {
				dai_err(dai, "dmic_set_config_nhlt(): illegal FIR_CONTROL = 0x%08x",
					val);
				return -EINVAL;
			}

			/* Clear START, set MUTE */
			fir_control = (val & ~FIR_CONTROL_A_START_BIT) | FIR_CONTROL_A_MUTE_BIT;
			dai_write(dai, base[n] + FIR_CONTROL_A, fir_control);
			dai_dbg(dai, "dmic_set_config_nhlt(): FIR_CONTROL_A = %08x", fir_control);

			/* Use DC_OFFSET and GAIN as such */
			val = fir_cfg_a[n]->dc_offset_left;
			dai_write(dai, base[n] + DC_OFFSET_LEFT_A, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): DC_OFFSET_LEFT_A = %08x", val);

			val = fir_cfg_a[n]->dc_offset_right;
			dai_write(dai, base[n] + DC_OFFSET_RIGHT_A, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): DC_OFFSET_RIGHT_A = %08x", val);

			val = fir_cfg_a[n]->out_gain_left;
			dai_write(dai, base[n] + OUT_GAIN_LEFT_A, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): OUT_GAIN_LEFT_A = %08x", val);

			val = fir_cfg_a[n]->out_gain_right;
			dai_write(dai, base[n] + OUT_GAIN_RIGHT_A, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): OUT_GAIN_RIGHT_A = %08x", val);
		}

		/* FIR B */
		fir_cfg_b[n] = (struct nhlt_pdm_ctrl_fir_cfg *)p;
		p += sizeof(struct nhlt_pdm_ctrl_fir_cfg);
		val = fir_cfg_b[n]->fir_config;
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

			val = fir_cfg_b[n]->fir_control;
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
			val = fir_cfg_b[n]->dc_offset_left;
			dai_write(dai, base[n] + DC_OFFSET_LEFT_B, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): DC_OFFSET_LEFT_B = %08x", val);

			val = fir_cfg_b[n]->dc_offset_right;
			dai_write(dai, base[n] + DC_OFFSET_RIGHT_B, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): DC_OFFSET_RIGHT_B = %08x", val);

			val = fir_cfg_b[n]->out_gain_left;
			dai_write(dai, base[n] + OUT_GAIN_LEFT_B, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): OUT_GAIN_LEFT_B = %08x", val);

			val = fir_cfg_b[n]->out_gain_right;
			dai_write(dai, base[n] + OUT_GAIN_RIGHT_B, val);
			dai_dbg(dai, "dmic_set_config_nhlt(): OUT_GAIN_RIGHT_B = %08x", val);
		}

		/* Set up FIR coefficients RAM */
		val = pdm_cfg[n]->reuse_fir_from_pdm;
		if (val == 0) {
			fir_a[n] = (struct nhlt_pdm_fir_coeffs *)p;
			p += sizeof(int32_t) * fir_length_a;
			fir_b[n] = (struct nhlt_pdm_fir_coeffs *)p;
			p += sizeof(int32_t) * fir_length_b;
		} else {
			val--;
			if (val >= n) {
				dai_err(dai, "dmic_set_config_nhlt(): Illegal FIR reuse 0x%x", val);
				return -EINVAL;
			}

			if (!fir_a[val]) {
				dai_err(dai, "dmic_set_config_nhlt(): PDM%d FIR reuse from %d fail",
					n, val);
				return -EINVAL;
			}

			fir_a[n] = fir_a[val];
			fir_b[n] = fir_b[val];
		}

		if (dai->index == 0) {
			dai_info(dai, "dmic_set_config_nhlt(): clkdiv = %d, mcic = %d, mfir_a = %d, length = %d",
				 p_clkdiv, p_mcic, p_mfira, fir_length_a);
			for (i = 0; i < fir_length_a; i++)
				dai_write(dai, coef_base_a[n] + (i << 2), fir_a[n]->fir_coeffs[i]);
		} else {
			dai_info(dai, "dmic_set_config_nhlt(): clkdiv = %d, mcic = %d, mfir_b = %d, length = %d",
				 p_clkdiv, p_mcic, p_mfirb, fir_length_b);
			for (i = 0; i < fir_length_b; i++)
				dai_write(dai, coef_base_b[n] + (i << 2), fir_b[n]->fir_coeffs[i]);
		}
	}

	if (dai->index == 0)
		ret = nhlt_dmic_dai_params_get(dai, out_control, pdm_cfg, fir_cfg_a);
	else
		ret = nhlt_dmic_dai_params_get(dai, out_control, pdm_cfg, fir_cfg_b);

	if (ret)
		return ret;

	if (dai->index == 0)
		rate_div = p_clkdiv * p_mcic * p_mfira;
	else
		rate_div = p_clkdiv * p_mcic * p_mfirb;

	if (!rate_div) {
		dai_err(dai, "dmic_set_config_nhlt(): zero clock divide or decimation factor");
		return -EINVAL;
	}

	dmic->dai_rate = CONFIG_DMIC_HW_IOCLK / rate_div;
	dai_info(dai, "dmic_set_config_nhlt(): rate = %d, channels = %d, format = %d",
		 dmic->dai_rate, dmic->dai_channels, dmic->dai_format);
	return 0;
}

int dmic_get_hw_params_nhlt(struct dai *dai, struct sof_ipc_stream_params  *params, int dir)
{
	struct dmic_pdata *dmic = dai_get_drvdata(dai);

	params->frame_fmt = dmic->dai_format;
	params->channels = dmic->dai_channels;
	params->rate = dmic->dai_rate;
	return 0;
}
