// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Jaska Uimonen <jaska.uimonen@linux.intel.com>

#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include "dmic.h"

static int nhlt_dmic_dai_params_get_ver1(struct dmic_pdata *dmic, int dai_index,
					 uint32_t *outcontrol, struct nhlt_pdm_ctrl_cfg **pdm_cfg,
					 struct nhlt_pdm_ctrl_fir_cfg **fir_cfg)
{
	int fir_stereo = FIR_CONTROL_A_STEREO_GET(fir_cfg[0]->fir_control);
	int mic_swap;

	switch (OUTCONTROL0_OF_GET(outcontrol[dai_index])) {
	case 0:
	case 1:
		dmic->dai_format = SOF_IPC_FRAME_S16_LE;
		break;
	case 2:
		dmic->dai_format = SOF_IPC_FRAME_S32_LE;
		break;
	default:
		fprintf(stderr, "nhlt_dmic_dai_params_get(): Illegal OF bit field\n");
		return -EINVAL;
	}

	switch (OUTCONTROL0_IPM_GET(outcontrol[dai_index])) {
	case 0:
		if (fir_stereo) {
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
		if (fir_stereo) {
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
		if (fir_stereo) {
			dmic->dai_channels = 4;
			dmic->enable[0] = 0x3; /* PDM0 MIC A and B */
			dmic->enable[1] = 0x3;	/* PDM1 MIC A and B */
		} else {
			fprintf(stderr, "nhlt_dmic_dai_params_get(): Illegal 4ch configuration\n");
			return -EINVAL;
		}
		break;
	default:
		fprintf(stderr, "nhlt_dmic_dai_params_get(): Illegal OF bit field\n");
		return -EINVAL;
	}

	return 0;
}

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

static int nhlt_dmic_dai_params_get_ver3(struct dmic_pdata *dmic, int dai_index,
					 uint32_t *outcontrol, struct nhlt_pdm_ctrl_cfg **pdm_cfg,
					 struct nhlt_pdm_ctrl_fir_cfg **fir_cfg)
{
	uint32_t outcontrol_val = outcontrol[dai_index];
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
		fprintf(stderr, "nhlt_dmic_dai_params_get(): Illegal OF bit field\n");
		return -EINVAL;
	}

	num_pdm = OUTCONTROL0_IPM_GET(outcontrol_val);
	if (num_pdm > DMIC_HW_CONTROLLERS) {
		fprintf(stderr, "nhlt_dmic_dai_params_get(): Illegal IPM PDM controllers count\n");
		return -EINVAL;
	}

	stereo_pdm = OUTCONTROL0_IPM_SOURCE_MODE_GET(outcontrol_val);

	dmic->dai_channels = (stereo_pdm + 1) * num_pdm;
	for (n = 0; n < DMIC_HW_CONTROLLERS; n++)
		dmic->enable[n] = 0;

	n = 0;
	source_pdm = OUTCONTROL0_IPM_SOURCE_1_GET(outcontrol_val);
	ret = ipm_source_to_enable(dmic, pdm_cfg, &n, num_pdm, stereo_pdm, source_pdm);
	if (ret) {
		fprintf(stderr, "nhlt_dmic_dai_params_get(): Illegal IPM_SOURCE_1\n");
		return -EINVAL;
	}

	source_pdm = OUTCONTROL0_IPM_SOURCE_2_GET(outcontrol_val);
	ret = ipm_source_to_enable(dmic, pdm_cfg, &n, num_pdm, stereo_pdm, source_pdm);
	if (ret) {
		fprintf(stderr, "nhlt_dmic_dai_params_get(): Illegal IPM_SOURCE_2\n");
		return -EINVAL;
	}

	source_pdm = OUTCONTROL0_IPM_SOURCE_3_GET(outcontrol_val);
	ret = ipm_source_to_enable(dmic, pdm_cfg, &n, num_pdm, stereo_pdm, source_pdm);
	if (ret) {
		fprintf(stderr, "nhlt_dmic_dai_params_get(): Illegal IPM_SOURCE_3\n");
		return -EINVAL;
	}

	source_pdm = OUTCONTROL0_IPM_SOURCE_4_GET(outcontrol_val);
	ret = ipm_source_to_enable(dmic, pdm_cfg, &n, num_pdm, stereo_pdm, source_pdm);
	if (ret) {
		fprintf(stderr, "nhlt_dmic_dai_params_get(): Illegal IPM_SOURCE_4\n");
		return -EINVAL;
	}

	return 0;
}

int count_set_bits(uint32_t w)
{
	int i;
	int n = 0;

	for (i = 0; i < 32; i++)
		n += ((w >> i) & 1);

	return n;
}

int print_dmic_blob_decode(uint8_t *blob, uint32_t len, int dmic_hw_ver)
{
	struct nhlt_pdm_ctrl_cfg *pdm_cfg[DMIC_HW_CONTROLLERS_MAX];
	struct nhlt_pdm_ctrl_fir_cfg *fir_cfg_a[DMIC_HW_CONTROLLERS_MAX];
	struct nhlt_pdm_ctrl_fir_cfg *fir_cfg_b[DMIC_HW_CONTROLLERS_MAX];
	struct nhlt_pdm_fir_coeffs *fir_a[DMIC_HW_CONTROLLERS_MAX] = {NULL};
	struct nhlt_pdm_fir_coeffs *fir_b[DMIC_HW_CONTROLLERS_MAX];
	struct dmic_pdata dmic[DMIC_HW_FIFOS_MAX];
	uint32_t out_control[DMIC_HW_FIFOS_MAX];
	uint32_t channel_ctrl_mask;
	uint32_t pdm_ctrl_mask;
	uint32_t ref;
	uint32_t val;
	const uint8_t *p = (uint8_t *)blob;
	int num_fifos;
	int num_pdm;
	int fir_length_a;
	int fir_length_b;
	int n;
	int rate_div_a, rate_div_b;
	int clk_div;
	int comb_count;
	int fir_decimation, fir_shift, fir_length;
	int bf1, bf2, bf3, bf4, bf5, bf6, bf7, bf8;
	int bf9, bf10, bf11, bf12;
	int bf13;

	int ret;
	int p_mcic = 0;
	int p_mfira = 0;
	int p_mfirb = 0;
	int p_clkdiv = 0;

	/* Skip not used headers */
	p += sizeof(struct nhlt_dmic_gateway_attributes);
	p += sizeof(struct nhlt_dmic_ts_group);
	p += sizeof(struct nhlt_dmic_clock_on_delay);

	/* Channel_ctlr_mask bits indicate the FIFOs enabled*/
	channel_ctrl_mask = ((struct nhlt_dmic_channel_ctrl_mask *)p)->channel_ctrl_mask;
	num_fifos = count_set_bits(channel_ctrl_mask); /* Count set bits */
	p += sizeof(struct nhlt_dmic_channel_ctrl_mask);
	printf("\n");
	printf("channel_ctrl_mask = %d\n", channel_ctrl_mask);

	/* Get OUTCONTROLx configuration */
	if (num_fifos < 1 || num_fifos > DMIC_HW_FIFOS_MAX) {
		fprintf(stderr, "Error: illegal number of FIFOs %d\n", num_fifos);
		return -EINVAL;
	}

	for (n = 0; n < num_fifos; n++) {
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
		printf("OUTCONTROL%d = %08x\n", n, out_control[n]);
		printf("  tie=%d, sip=%d, finit=%d, fci=%d, bfth=%d, of=%d, ipm=%d, th=%d",
		       bf1, bf2, bf3, bf4, bf5, bf6, bf7, bf8);

		if (dmic_hw_ver == 1) {
			printf("\n");
			ref = OUTCONTROL0_TIE(bf1) | OUTCONTROL0_SIP(bf2) | OUTCONTROL0_FINIT(bf3) |
				OUTCONTROL0_FCI(bf4) | OUTCONTROL0_BFTH(bf5) | OUTCONTROL0_OF(bf6) |
				OUTCONTROL0_IPM(bf7) | OUTCONTROL0_TH(bf8);
		} else {
			bf9 = OUTCONTROL0_IPM_SOURCE_1_GET(val);
			bf10 = OUTCONTROL0_IPM_SOURCE_2_GET(val);
			bf11 = OUTCONTROL0_IPM_SOURCE_3_GET(val);
			bf12 = OUTCONTROL0_IPM_SOURCE_4_GET(val);
			printf(", ipms1=%d, ipms2=%d, ipms3=%d, ipms4=%d",
			       bf9, bf10, bf11, bf12);
			ref = OUTCONTROL0_TIE(bf1) | OUTCONTROL0_SIP(bf2) | OUTCONTROL0_FINIT(bf3) |
				OUTCONTROL0_FCI(bf4) | OUTCONTROL0_BFTH(bf5) | OUTCONTROL0_OF(bf6) |
				OUTCONTROL0_IPM(bf7) | OUTCONTROL0_IPM_SOURCE_1(bf9) |
				OUTCONTROL0_IPM_SOURCE_2(bf10) | OUTCONTROL0_IPM_SOURCE_3(bf11) |
				OUTCONTROL0_IPM_SOURCE_4(bf12) | OUTCONTROL0_TH(bf8);
			bf13 = OUTCONTROL0_IPM_SOURCE_MODE_GET(val);
			ref |= OUTCONTROL0_IPM_SOURCE_MODE(bf13);
			printf(", ipmsm=%d", bf13);
		}
		printf("\n");

		if (ref != val) {
			fprintf(stderr, "Error: illegal OUTCONTROL%d = 0x%08x\n",
				n, val);
			ret = -EINVAL;
		}

		p += sizeof(uint32_t);
	}

	/* Get PDMx registers */
	pdm_ctrl_mask = ((struct nhlt_pdm_ctrl_mask *)p)->pdm_ctrl_mask;
	num_pdm = count_set_bits(pdm_ctrl_mask); /* Count set bits */
	p += sizeof(struct nhlt_pdm_ctrl_mask);
	printf("pdm_ctrl_mask = %d\n", pdm_ctrl_mask);
	if (num_pdm < 1 || num_pdm > DMIC_HW_CONTROLLERS) {
		fprintf(stderr, "Error: illegal number of PDMs %d\n", num_pdm);
		return -EINVAL;
	}

	for (n = 0; n < num_pdm; n++) {
		printf("\nPDM%d\n", n);

		/* Get CIC configuration */
		pdm_cfg[n] = (struct nhlt_pdm_ctrl_cfg *)p;
		p += sizeof(struct nhlt_pdm_ctrl_cfg);

		comb_count = CIC_CONFIG_COMB_COUNT_GET(pdm_cfg[n]->cic_config);
		if (comb_count > 0)
			p_mcic = comb_count + 1;

		clk_div = MIC_CONTROL_PDM_CLKDIV_GET(pdm_cfg[n]->mic_control);
		if (clk_div > 0)
			p_clkdiv = clk_div + 2;

		val = pdm_cfg[0]->cic_control;
		bf1 = CIC_CONTROL_SOFT_RESET_GET(val);
		bf2 = CIC_CONTROL_CIC_START_B_GET(val);
		bf3 = CIC_CONTROL_CIC_START_A_GET(val);
		bf4 = CIC_CONTROL_MIC_B_POLARITY_GET(val);
		bf5 = CIC_CONTROL_MIC_A_POLARITY_GET(val);
		bf6 = CIC_CONTROL_MIC_MUTE_GET(val);
		printf("CIC_CONTROL = 0x%08x\n", val);
		printf("  soft_reset=%d, cic_start_b=%d, cic_start_a=%d", bf1, bf2, bf3);
		printf(" mic_b_polarity=%d, mic_a_polarity=%d, mic_mute=%d", bf4, bf5, bf6);
		ref = CIC_CONTROL_SOFT_RESET(bf1) | CIC_CONTROL_CIC_START_B(bf2) |
			CIC_CONTROL_CIC_START_A(bf3) | CIC_CONTROL_MIC_B_POLARITY(bf4) |
			CIC_CONTROL_MIC_A_POLARITY(bf5) | CIC_CONTROL_MIC_MUTE(bf6);

		if (dmic_hw_ver == 1) {
			bf7 = CIC_CONTROL_STEREO_MODE_GET(val);
			ref |= CIC_CONTROL_STEREO_MODE(bf7);
			printf(", stereo_mode=%d", bf7);
		}

		printf("\n");
		if (ref != val) {
			fprintf(stderr, "Error: illegal CIC_CONTROL = 0x%08x\n", val);
			ret = -EINVAL;
		}

		val = pdm_cfg[n]->cic_config;
		bf1 = CIC_CONFIG_CIC_SHIFT_GET(val);
		printf("CIC_CONFIG = 0x%08x\n", val);
		printf("  cic_shift=%d, comb_count=%d\n", bf1, comb_count);

		val = pdm_cfg[n]->mic_control;
		bf1 = MIC_CONTROL_PDM_SKEW_GET(val);
		bf2 = MIC_CONTROL_PDM_CLK_EDGE_GET(val);
		bf3 = MIC_CONTROL_PDM_EN_B_GET(val);
		bf4 = MIC_CONTROL_PDM_EN_A_GET(val);
		printf("MIC_CONTROL = 0x%08x\n", val);
		printf("  clkdiv=%d, skew=%d, clk_edge=%d, ", clk_div, bf1, bf2);
		printf("en_b=%d, en_a=%d\n", bf3, bf4);

		/* FIR A */
		fir_cfg_a[n] = (struct nhlt_pdm_ctrl_fir_cfg *)p;
		p += sizeof(struct nhlt_pdm_ctrl_fir_cfg);
		val = fir_cfg_a[n]->fir_config;
		fir_length = FIR_CONFIG_A_FIR_LENGTH_GET(val);
		fir_length_a = fir_length + 1; /* Need for parsing */
		fir_decimation = FIR_CONFIG_A_FIR_DECIMATION_GET(val);
		if (fir_decimation > 0)
			p_mfira = fir_decimation + 1;

		fir_shift = FIR_CONFIG_A_FIR_SHIFT_GET(val);
		printf("FIR_CONFIG_A = 0x%08x\n", val);
		printf("  fir_decimation=%d, fir_shift=%d, fir_length=%d\n",
		       fir_decimation, fir_shift, fir_length);

		val = fir_cfg_a[n]->fir_control;
		bf1 = FIR_CONTROL_A_START_GET(val);
		bf2 = FIR_CONTROL_A_ARRAY_START_EN_GET(val);
		bf3 = FIR_CONTROL_A_DCCOMP_GET(val);
		bf4 = FIR_CONTROL_A_MUTE_GET(val);
		bf5 = FIR_CONTROL_A_STEREO_GET(val);
		printf("FIR_CONTROL_A = 0x%08x\n", val);
		printf("  start=%d, array_start_en=%d, dccomp=%d, mute=%d, stereo=%d",
		       bf1, bf2, bf3, bf4, bf5);
		ref = FIR_CONTROL_A_START(bf1) | FIR_CONTROL_A_ARRAY_START_EN(bf2) |
			FIR_CONTROL_A_DCCOMP(bf3) | FIR_CONTROL_A_MUTE(bf4) |
			FIR_CONTROL_A_STEREO(bf5);

		if (dmic_hw_ver != 1) {
			bf7 = FIR_CONTROL_A_RDRFPGE_GET(val);
			bf8 = FIR_CONTROL_A_LDRFPGE_GET(val);
			bf9 = FIR_CONTROL_A_CRFPGE_GET(val);
			bf10 = FIR_CONTROL_A_PERIODIC_START_EN_GET(val);
			bf11 = FIR_CONTROL_A_AUTOMUTE_GET(val);
			printf(", rdrfpge=%d, ldrfpge=%d, crfpge=%d", bf7, bf8, bf8);
			printf(", periodic_start_en=%d, automute=%d", bf10, bf11);
			ref |=  FIR_CONTROL_A_RDRFPGE(bf7) | FIR_CONTROL_A_LDRFPGE(bf8) |
				FIR_CONTROL_A_CRFPGE(bf9) | FIR_CONTROL_A_PERIODIC_START_EN(bf10) |
				FIR_CONTROL_A_AUTOMUTE(bf11);
		}

		printf("\n");
		if (ref != val) {
			fprintf(stderr, "Error: illegal FIR_CONTROL = 0x%08x\n",
				val);
			ret = -EINVAL;
		}

		/* Use DC_OFFSET and GAIN as such */
		val = fir_cfg_a[n]->dc_offset_left;
		printf("DC_OFFSET_LEFT_A = 0x%08x\n", val);

		val = fir_cfg_a[n]->dc_offset_right;
		printf("DC_OFFSET_RIGHT_A = 0x%08x\n", val);

		val = fir_cfg_a[n]->out_gain_left;
		printf("OUT_GAIN_LEFT_A = 0x%08x\n", val);

		val = fir_cfg_a[n]->out_gain_right;
		printf("OUT_GAIN_RIGHT_A = 0x%08x\n", val);

		/* FIR B */
		fir_cfg_b[n] = (struct nhlt_pdm_ctrl_fir_cfg *)p;
		p += sizeof(struct nhlt_pdm_ctrl_fir_cfg);
		val = fir_cfg_b[n]->fir_config;
		fir_length = FIR_CONFIG_B_FIR_LENGTH_GET(val);
		fir_length_b = fir_length + 1; /* Need for parsing */
		fir_decimation = FIR_CONFIG_B_FIR_DECIMATION_GET(val);
		if (fir_decimation > 0)
			p_mfirb = fir_decimation + 1;

		fir_shift = FIR_CONFIG_B_FIR_SHIFT_GET(val);
		printf("FIR_CONFIG_B = 0x%08x\n", val);
		printf("  fir_decimation=%d, fir_shift=%d, fir_length=%d\n",
		       fir_decimation, fir_shift, fir_length);

		val = fir_cfg_b[n]->fir_control;
		bf1 = FIR_CONTROL_B_START_GET(val);
		bf2 = FIR_CONTROL_B_ARRAY_START_EN_GET(val);
		bf3 = FIR_CONTROL_B_DCCOMP_GET(val);
		bf5 = FIR_CONTROL_B_MUTE_GET(val);
		bf6 = FIR_CONTROL_B_STEREO_GET(val);
		printf("FIR_CONTROL_B = 0x%08x\n", val);
		printf("  start=%d, array_start_en=%d, dccomp=%d, mute=%d, stereo=%d",
		       bf1, bf2, bf3, bf5, bf6);

		if (dmic_hw_ver != 1) {
			bf7 = FIR_CONTROL_A_RDRFPGE_GET(val);
			bf8 = FIR_CONTROL_A_LDRFPGE_GET(val);
			bf9 = FIR_CONTROL_A_CRFPGE_GET(val);
			bf10 = FIR_CONTROL_A_PERIODIC_START_EN_GET(val);
			bf11 = FIR_CONTROL_A_AUTOMUTE_GET(val);
			printf(", rdrfpge=%d, ldrfpge=%d, crfpge=%d", bf7, bf8, bf8);
			printf(", periodic_start_en=%d, automute=%d", bf10, bf11);
			ref |=  FIR_CONTROL_A_RDRFPGE(bf7) | FIR_CONTROL_A_LDRFPGE(bf8) |
				FIR_CONTROL_A_CRFPGE(bf9) | FIR_CONTROL_A_PERIODIC_START_EN(bf10) |
				FIR_CONTROL_A_AUTOMUTE(bf11);
		}

		printf("\n");

		/* Use DC_OFFSET and GAIN as such */
		val = fir_cfg_b[n]->dc_offset_left;
		printf("DC_OFFSET_LEFT_B = 0x%08x\n", val);

		val = fir_cfg_b[n]->dc_offset_right;
		printf("DC_OFFSET_RIGHT_B = 0x%08x\n", val);

		val = fir_cfg_b[n]->out_gain_left;
		printf("OUT_GAIN_LEFT_B = 0x%08x\n", val);

		val = fir_cfg_b[n]->out_gain_right;
		printf("OUT_GAIN_RIGHT_B = 0x%08x\n", val);

		/* Set up FIR coefficients RAM */
		val = pdm_cfg[n]->reuse_fir_from_pdm;
		printf("reuse_fir_from_pdm = %d\n", val);
		if (val == 0) {
			fir_a[n] = (struct nhlt_pdm_fir_coeffs *)p;
			p += sizeof(int32_t) * fir_length_a;
			fir_b[n] = (struct nhlt_pdm_fir_coeffs *)p;
			p += sizeof(int32_t) * fir_length_b;
		} else {
			val--;
			if (val >= n) {
				fprintf(stderr, "Error: Illegal FIR reuse 0x%x\n", val);
				return -EINVAL;
			}

			if (!fir_a[val]) {
				fprintf(stderr, "Error: PDM%d FIR reuse from %d fail\n",
					n, val);
				ret = -EINVAL;
			}

			fir_a[n] = fir_a[val];
			fir_b[n] = fir_b[val];
		}

		printf("clkdiv = %d, mcic = %d, mfir_a = %d, length_a = %d, ",
		       p_clkdiv, p_mcic, p_mfira, fir_length_a);
		printf("mfir_b = %d, length_b = %d\n", p_mfirb, fir_length_b);
	}

	if (dmic_hw_ver == 1) {
		ret = nhlt_dmic_dai_params_get_ver1(&dmic[0], 0, out_control, pdm_cfg, fir_cfg_a);
		if (ret)
			return ret;
	} else {
		ret = nhlt_dmic_dai_params_get_ver3(&dmic[1], 1, out_control, pdm_cfg, fir_cfg_b);
		if (ret)
			return ret;
	}

	rate_div_a = p_clkdiv * p_mcic * p_mfira;
	rate_div_b = p_clkdiv * p_mcic * p_mfirb;

	if (!rate_div_a || !rate_div_b) {
		fprintf(stderr, "Error: zero clock divide or decimation factor\n");
		return -EINVAL;
	}

	dmic[0].dai_rate = DMIC_HW_IOCLK / rate_div_a;
	dmic[1].dai_rate = DMIC_HW_IOCLK / rate_div_b;

	printf("\n");
	printf("DMIC0: rate = %d, channels = %d, format = %d\n",
	       dmic[0].dai_rate, dmic[0].dai_channels, dmic[0].dai_format);
	printf("DMIC1: rate = %d, channels = %d, format = %d\n",
	       dmic[1].dai_rate, dmic[1].dai_channels, dmic[1].dai_format);
	return ret;
}
