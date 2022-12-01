/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#include <stdint.h>

/* For NHLT DMIC configuration parsing */
#define DMIC_HW_CONTROLLERS_MAX	4
#define DMIC_HW_FIFOS_MAX	2

/* TODO: replace hard-coding these */
//#define DMIC_HW_VERSION		1 /* cAVS */
#define DMIC_HW_VERSION		3 /* ACE */
#define DMIC_HW_CONTROLLERS	2
#define DMIC_HW_IOCLK		38400000 /* TGL */
#define DMIC_HW_FIFOS		2

/* OUTCONTROLx IPM bit fields style */
#if DMIC_HW_VERSION == 1 || (DMIC_HW_VERSION == 2 && DMIC_HW_CONTROLLERS <= 2)
#define DMIC_IPM_VER1
#elif DMIC_HW_VERSION == 2 && DMIC_HW_CONTROLLERS > 2
#define DMIC_IPM_VER2
#elif DMIC_HW_VERSION == 3
#define DMIC_IPM_VER3
#else
#error Not supported HW version
#endif

/* bit.h */
#define BIT(b)			(1UL << (b))

#define MASK(b_hi, b_lo)	\
	(((1ULL << ((b_hi) - (b_lo) + 1ULL)) - 1ULL) << (b_lo))
#define SET_BIT(b, x)		(((x) & 1) << (b))
#define SET_BITS(b_hi, b_lo, x)	\
	(((x) & ((1ULL << ((b_hi) - (b_lo) + 1ULL)) - 1ULL)) << (b_lo))
#define GET_BIT(b, x) \
	(((x) & (1ULL << (b))) >> (b))
#define GET_BITS(b_hi, b_lo, x) \
	(((x) & MASK(b_hi, b_lo)) >> (b_lo))

#if defined DMIC_IPM_VER1
/* OUTCONTROL0 bits */
#define OUTCONTROL0_TIE_BIT	BIT(27)
#define OUTCONTROL0_SIP_BIT	BIT(26)
#define OUTCONTROL0_FINIT_BIT	BIT(25)
#define OUTCONTROL0_FCI_BIT	BIT(24)
#define OUTCONTROL0_TIE(x)	SET_BIT(27, x)
#define OUTCONTROL0_SIP(x)	SET_BIT(26, x)
#define OUTCONTROL0_FINIT(x)	SET_BIT(25, x)
#define OUTCONTROL0_FCI(x)	SET_BIT(24, x)
#define OUTCONTROL0_BFTH(x)	SET_BITS(23, 20, x)
#define OUTCONTROL0_OF(x)	SET_BITS(19, 18, x)
#define OUTCONTROL0_IPM(x)	SET_BITS(17, 16, x)
#define OUTCONTROL0_TH(x)	SET_BITS(5, 0, x)

#define OUTCONTROL0_TIE_GET(x)		GET_BIT(27, x)
#define OUTCONTROL0_SIP_GET(x)		GET_BIT(26, x)
#define OUTCONTROL0_FINIT_GET(x)	GET_BIT(25, x)
#define OUTCONTROL0_FCI_GET(x)		GET_BIT(24, x)
#define OUTCONTROL0_BFTH_GET(x)		GET_BITS(23, 20, x)
#define OUTCONTROL0_OF_GET(x)		GET_BITS(19, 18, x)
#define OUTCONTROL0_IPM_GET(x)		GET_BITS(17, 16, x)
#define OUTCONTROL0_TH_GET(x)		GET_BITS(5, 0, x)

/* OUTCONTROL1 bits */
#define OUTCONTROL1_TIE_BIT	BIT(27)
#define OUTCONTROL1_SIP_BIT	BIT(26)
#define OUTCONTROL1_FINIT_BIT	BIT(25)
#define OUTCONTROL1_FCI_BIT	BIT(24)
#define OUTCONTROL1_TIE(x)	SET_BIT(27, x)
#define OUTCONTROL1_SIP(x)	SET_BIT(26, x)
#define OUTCONTROL1_FINIT(x)	SET_BIT(25, x)
#define OUTCONTROL1_FCI(x)	SET_BIT(24, x)
#define OUTCONTROL1_BFTH(x)	SET_BITS(23, 20, x)
#define OUTCONTROL1_OF(x)	SET_BITS(19, 18, x)
#define OUTCONTROL1_IPM(x)	SET_BITS(17, 16, x)
#define OUTCONTROL1_TH(x)	SET_BITS(5, 0, x)

#define OUTCONTROL1_TIE_GET(x)		GET_BIT(27, x)
#define OUTCONTROL1_SIP_GET(x)		GET_BIT(26, x)
#define OUTCONTROL1_FINIT_GET(x)	GET_BIT(25, x)
#define OUTCONTROL1_FCI_GET(x)		GET_BIT(24, x)
#define OUTCONTROL1_BFTH_GET(x)		GET_BITS(23, 20, x)
#define OUTCONTROL1_OF_GET(x)		GET_BITS(19, 18, x)
#define OUTCONTROL1_IPM_GET(x)		GET_BITS(17, 16, x)
#define OUTCONTROL1_TH_GET(x)		GET_BITS(5, 0, x)
#endif

#if defined DMIC_IPM_VER2 || defined DMIC_IPM_VER3
/* OUTCONTROL0 bits */
#define OUTCONTROL0_TIE_BIT			BIT(27)
#define OUTCONTROL0_SIP_BIT			BIT(26)
#define OUTCONTROL0_FINIT_BIT			BIT(25)
#define OUTCONTROL0_FCI_BIT			BIT(24)
#define OUTCONTROL0_TIE(x)			SET_BIT(27, x)
#define OUTCONTROL0_SIP(x)			SET_BIT(26, x)
#define OUTCONTROL0_FINIT(x)			SET_BIT(25, x)
#define OUTCONTROL0_FCI(x)			SET_BIT(24, x)
#define OUTCONTROL0_BFTH(x)			SET_BITS(23, 20, x)
#define OUTCONTROL0_OF(x)			SET_BITS(19, 18, x)
#define OUTCONTROL0_IPM(x)                      SET_BITS(17, 15, x)
#define OUTCONTROL0_IPM_SOURCE_1(x)		SET_BITS(14, 13, x)
#define OUTCONTROL0_IPM_SOURCE_2(x)		SET_BITS(12, 11, x)
#define OUTCONTROL0_IPM_SOURCE_3(x)		SET_BITS(10, 9, x)
#define OUTCONTROL0_IPM_SOURCE_4(x)		SET_BITS(8, 7, x)
#define OUTCONTROL0_TH(x)			SET_BITS(5, 0, x)
#define OUTCONTROL0_TIE_GET(x)			GET_BIT(27, x)
#define OUTCONTROL0_SIP_GET(x)			GET_BIT(26, x)
#define OUTCONTROL0_FINIT_GET(x)		GET_BIT(25, x)
#define OUTCONTROL0_FCI_GET(x)			GET_BIT(24, x)
#define OUTCONTROL0_BFTH_GET(x)			GET_BITS(23, 20, x)
#define OUTCONTROL0_OF_GET(x)			GET_BITS(19, 18, x)
#define OUTCONTROL0_IPM_GET(x)			GET_BITS(17, 15, x)
#define OUTCONTROL0_IPM_SOURCE_1_GET(x)		GET_BITS(14, 13, x)
#define OUTCONTROL0_IPM_SOURCE_2_GET(x)		GET_BITS(12, 11, x)
#define OUTCONTROL0_IPM_SOURCE_3_GET(x)		GET_BITS(10,  9, x)
#define OUTCONTROL0_IPM_SOURCE_4_GET(x)		GET_BITS(8, 7, x)
#define OUTCONTROL0_TH_GET(x)			GET_BITS(5, 0, x)

/* OUTCONTROL1 bits */
#define OUTCONTROL1_TIE_BIT			BIT(27)
#define OUTCONTROL1_SIP_BIT			BIT(26)
#define OUTCONTROL1_FINIT_BIT			BIT(25)
#define OUTCONTROL1_FCI_BIT			BIT(24)
#define OUTCONTROL1_TIE(x)			SET_BIT(27, x)
#define OUTCONTROL1_SIP(x)			SET_BIT(26, x)
#define OUTCONTROL1_FINIT(x)			SET_BIT(25, x)
#define OUTCONTROL1_FCI(x)			SET_BIT(24, x)
#define OUTCONTROL1_BFTH(x)			SET_BITS(23, 20, x)
#define OUTCONTROL1_OF(x)			SET_BITS(19, 18, x)
#define OUTCONTROL1_IPM(x)                      SET_BITS(17, 15, x)
#define OUTCONTROL1_IPM_SOURCE_1(x)		SET_BITS(14, 13, x)
#define OUTCONTROL1_IPM_SOURCE_2(x)		SET_BITS(12, 11, x)
#define OUTCONTROL1_IPM_SOURCE_3(x)		SET_BITS(10, 9, x)
#define OUTCONTROL1_IPM_SOURCE_4(x)		SET_BITS(8, 7, x)
#define OUTCONTROL1_TH(x)			SET_BITS(5, 0, x)
#define OUTCONTROL1_TIE_GET(x)			GET_BIT(27, x)
#define OUTCONTROL1_SIP_GET(x)			GET_BIT(26, x)
#define OUTCONTROL1_FINIT_GET(x)		GET_BIT(25, x)
#define OUTCONTROL1_FCI_GET(x)			GET_BIT(24, x)
#define OUTCONTROL1_BFTH_GET(x)			GET_BITS(23, 20, x)
#define OUTCONTROL1_OF_GET(x)			GET_BITS(19, 18, x)
#define OUTCONTROL1_IPM_GET(x)			GET_BITS(17, 15, x)
#define OUTCONTROL1_IPM_SOURCE_1_GET(x)		GET_BITS(14, 13, x)
#define OUTCONTROL1_IPM_SOURCE_2_GET(x)		GET_BITS(12, 11, x)
#define OUTCONTROL1_IPM_SOURCE_3_GET(x)		GET_BITS(10,  9, x)
#define OUTCONTROL1_IPM_SOURCE_4_GET(x)		GET_BITS(8, 7, x)
#define OUTCONTROL1_TH_GET(x)			GET_BITS(5, 0, x)

#define OUTCONTROLX_IPM_NUMSOURCES		4

#ifdef DMIC_IPM_VER3
#define OUTCONTROL0_IPM_SOURCE_MODE(x)		SET_BIT(6, x)
#define OUTCONTROL0_IPM_SOURCE_MODE_GET(x)	GET_BIT(6, x)
#define OUTCONTROL1_IPM_SOURCE_MODE(x)		SET_BIT(6, x)
#define OUTCONTROL1_IPM_SOURCE_MODE_GET(x)	GET_BIT(6, x)
#endif

#endif

/* OUTSTAT0 bits */
#define OUTSTAT0_AFE_BIT	BIT(31)
#define OUTSTAT0_ASNE_BIT	BIT(29)
#define OUTSTAT0_RFS_BIT	BIT(28)
#define OUTSTAT0_ROR_BIT	BIT(27)
#define OUTSTAT0_FL_MASK	MASK(6, 0)

/* OUTSTAT1 bits */
#define OUTSTAT1_AFE_BIT	BIT(31)
#define OUTSTAT1_ASNE_BIT	BIT(29)
#define OUTSTAT1_RFS_BIT	BIT(28)
#define OUTSTAT1_ROR_BIT	BIT(27)
#define OUTSTAT1_FL_MASK	MASK(6, 0)

/* CIC_CONTROL bits */
#define CIC_CONTROL_SOFT_RESET_BIT	BIT(16)
#define CIC_CONTROL_CIC_START_B_BIT	BIT(15)
#define CIC_CONTROL_CIC_START_A_BIT	BIT(14)
#define CIC_CONTROL_MIC_B_POLARITY_BIT	BIT(3)
#define CIC_CONTROL_MIC_A_POLARITY_BIT	BIT(2)
#define CIC_CONTROL_MIC_MUTE_BIT	BIT(1)

#define CIC_CONTROL_SOFT_RESET(x)	SET_BIT(16, x)
#define CIC_CONTROL_CIC_START_B(x)	SET_BIT(15, x)
#define CIC_CONTROL_CIC_START_A(x)	SET_BIT(14, x)
#define CIC_CONTROL_MIC_B_POLARITY(x)	SET_BIT(3, x)
#define CIC_CONTROL_MIC_A_POLARITY(x)	SET_BIT(2, x)
#define CIC_CONTROL_MIC_MUTE(x)		SET_BIT(1, x)

#define CIC_CONTROL_SOFT_RESET_GET(x)		GET_BIT(16, x)
#define CIC_CONTROL_CIC_START_B_GET(x)		GET_BIT(15, x)
#define CIC_CONTROL_CIC_START_A_GET(x)		GET_BIT(14, x)
#define CIC_CONTROL_MIC_B_POLARITY_GET(x)	GET_BIT(3, x)
#define CIC_CONTROL_MIC_A_POLARITY_GET(x)	GET_BIT(2, x)
#define CIC_CONTROL_MIC_MUTE_GET(x)		GET_BIT(1, x)

#define CIC_CONTROL_STEREO_MODE_BIT	BIT(0)
#define CIC_CONTROL_STEREO_MODE(x)	SET_BIT(0, x)
#define CIC_CONTROL_STEREO_MODE_GET(x)	GET_BIT(0, x)

/* CIC_CONFIG bits */
#define CIC_CONFIG_CIC_SHIFT(x)		SET_BITS(27, 24, x)
#define CIC_CONFIG_COMB_COUNT(x)	SET_BITS(15, 8, x)

/* CIC_CONFIG masks */
#define CIC_CONFIG_CIC_SHIFT_MASK	MASK(27, 24)
#define CIC_CONFIG_COMB_COUNT_MASK	MASK(15, 8)

#define CIC_CONFIG_CIC_SHIFT_GET(x)	GET_BITS(27, 24, x)
#define CIC_CONFIG_COMB_COUNT_GET(x)	GET_BITS(15, 8, x)

/* MIC_CONTROL bits */
#define MIC_CONTROL_PDM_EN_B_BIT	BIT(1)
#define MIC_CONTROL_PDM_EN_A_BIT	BIT(0)
#define MIC_CONTROL_PDM_CLKDIV(x)	SET_BITS(15, 8, x)
#define MIC_CONTROL_PDM_SKEW(x)		SET_BITS(7, 4, x)
#define MIC_CONTROL_CLK_EDGE(x)		SET_BIT(3, x)
#define MIC_CONTROL_PDM_EN_B(x)		SET_BIT(1, x)
#define MIC_CONTROL_PDM_EN_A(x)		SET_BIT(0, x)

/* MIC_CONTROL masks */
#define MIC_CONTROL_PDM_CLKDIV_MASK	MASK(15, 8)

#define GET_BITS(b_hi, b_lo, x) \
	(((x) & MASK(b_hi, b_lo)) >> (b_lo))
#define FIR_CONFIG_A_FIR_DECIMATION_GET(x) GET_BITS(20, 16, x)
#define MIC_CONTROL_PDM_CLKDIV_GET(x) GET_BITS(15, 8, x)
#define CIC_CONFIG_COMB_COUNT_GET(x) GET_BITS(15, 8, x)

#define MIC_CONTROL_PDM_CLKDIV_GET(x)	GET_BITS(15, 8, x)
#define MIC_CONTROL_PDM_SKEW_GET(x)	GET_BITS(7, 4, x)
#define MIC_CONTROL_PDM_CLK_EDGE_GET(x)	GET_BIT(3, x)
#define MIC_CONTROL_PDM_EN_B_GET(x)	GET_BIT(1, x)
#define MIC_CONTROL_PDM_EN_A_GET(x)	GET_BIT(0, x)

/* FIR_CONTROL_A bits */
#define FIR_CONTROL_A_START_BIT			BIT(7)
#define FIR_CONTROL_A_ARRAY_START_EN_BIT	BIT(6)
#define FIR_CONTROL_A_MUTE_BIT			BIT(1)
#define FIR_CONTROL_A_START(x)			SET_BIT(7, x)
#define FIR_CONTROL_A_ARRAY_START_EN(x)		SET_BIT(6, x)
#define FIR_CONTROL_A_DCCOMP(x)			SET_BIT(4, x)
#define FIR_CONTROL_A_MUTE(x)			SET_BIT(1, x)
#define FIR_CONTROL_A_STEREO(x)			SET_BIT(0, x)

#define FIR_CONTROL_A_START_GET(x)		GET_BIT(7, x)
#define FIR_CONTROL_A_ARRAY_START_EN_GET(x)	GET_BIT(6, x)
#define FIR_CONTROL_A_DCCOMP_GET(x)		GET_BIT(4, x)
#define FIR_CONTROL_A_MUTE_GET(x)		GET_BIT(1, x)
#define FIR_CONTROL_A_STEREO_GET(x)		GET_BIT(0, x)

#define FIR_CONTROL_A_RDRFPGE(x)		SET_BIT(30, x)
#define FIR_CONTROL_A_LDRFPGE(x)		SET_BIT(29, x)
#define FIR_CONTROL_A_CRFPGE(x)			SET_BIT(28, x)
#define FIR_CONTROL_A_PERIODIC_START_EN(x)	SET_BIT(5, x)
#define FIR_CONTROL_A_AUTOMUTE(x)		SET_BIT(2, x)
#define FIR_CONTROL_A_RDRFPGE_GET(x)		GET_BIT(30, x)
#define FIR_CONTROL_A_LDRFPGE_GET(x)		GET_BIT(29, x)
#define FIR_CONTROL_A_CRFPGE_GET(x)		GET_BIT(28, x)
#define FIR_CONTROL_A_PERIODIC_START_EN_GET(x)	GET_BIT(5, x)
#define FIR_CONTROL_A_AUTOMUTE_GET(x)		GET_BIT(2, x)

/* FIR_CONFIG_A bits */
#define FIR_CONFIG_A_FIR_DECIMATION(x)		SET_BITS(20, 16, x)
#define FIR_CONFIG_A_FIR_SHIFT(x)		SET_BITS(11, 8, x)
#define FIR_CONFIG_A_FIR_LENGTH(x)		SET_BITS(7, 0, x)

#define FIR_CONFIG_A_FIR_DECIMATION_GET(x)	GET_BITS(20, 16, x)
#define FIR_CONFIG_A_FIR_SHIFT_GET(x)		GET_BITS(11, 8, x)
#define FIR_CONFIG_A_FIR_LENGTH_GET(x)		GET_BITS(7, 0, x)

/* DC offset compensation time constants */
#define DCCOMP_TC0	0
#define DCCOMP_TC1	1
#define DCCOMP_TC2	2
#define DCCOMP_TC3	3
#define DCCOMP_TC4	4
#define DCCOMP_TC5	5
#define DCCOMP_TC6	6
#define DCCOMP_TC7	7

/* DC_OFFSET_LEFT_A bits */
#define DC_OFFSET_LEFT_A_DC_OFFS(x)		SET_BITS(21, 0, x)

/* DC_OFFSET_RIGHT_A bits */
#define DC_OFFSET_RIGHT_A_DC_OFFS(x)		SET_BITS(21, 0, x)

/* OUT_GAIN_LEFT_A bits */
#define OUT_GAIN_LEFT_A_GAIN(x)			SET_BITS(19, 0, x)

/* OUT_GAIN_RIGHT_A bits */
#define OUT_GAIN_RIGHT_A_GAIN(x)		SET_BITS(19, 0, x)

/* FIR_CONTROL_B bits */
#define FIR_CONTROL_B_START_BIT			BIT(7)
#define FIR_CONTROL_B_ARRAY_START_EN_BIT	BIT(6)
#define FIR_CONTROL_B_MUTE_BIT			BIT(1)
#define FIR_CONTROL_B_START(x)			SET_BIT(7, x)
#define FIR_CONTROL_B_ARRAY_START_EN(x)		SET_BIT(6, x)
#define FIR_CONTROL_B_DCCOMP(x)			SET_BIT(4, x)
#define FIR_CONTROL_B_MUTE(x)			SET_BIT(1, x)
#define FIR_CONTROL_B_STEREO(x)			SET_BIT(0, x)

#define FIR_CONTROL_B_START_GET(x)		GET_BIT(7, x)
#define FIR_CONTROL_B_ARRAY_START_EN_GET(x)	GET_BIT(6, x)
#define FIR_CONTROL_B_DCCOMP_GET(x)		GET_BIT(4, x)
#define FIR_CONTROL_B_MUTE_GET(x)		GET_BIT(1, x)
#define FIR_CONTROL_B_STEREO_GET(x)		GET_BIT(0, x)

#define FIR_CONTROL_B_RDRFPGE(x)		SET_BIT(30, x)
#define FIR_CONTROL_B_LDRFPGE(x)		SET_BIT(29, x)
#define FIR_CONTROL_B_CRFPGE(x)			SET_BIT(28, x)
#define FIR_CONTROL_B_PERIODIC_START_EN(x)	SET_BIT(5, x)
#define FIR_CONTROL_B_AUTOMUTE(x)		SET_BIT(2, x)
#define FIR_CONTROL_B_RDRFPGE_GET(x)		GET_BIT(30, x)
#define FIR_CONTROL_B_LDRFPGE_GET(x)		GET_BIT(29, x)
#define FIR_CONTROL_B_CRFPGE_GET(x)		GET_BIT(28, x)
#define FIR_CONTROL_B_PERIODIC_START_EN_GET(x)	GET_BIT(5, x)
#define FIR_CONTROL_B_AUTOMUTE_GET(x)		GET_BIT(2, x)

/* FIR_CONFIG_B bits */
#define FIR_CONFIG_B_FIR_DECIMATION(x)		SET_BITS(20, 16, x)
#define FIR_CONFIG_B_FIR_SHIFT(x)		SET_BITS(11, 8, x)
#define FIR_CONFIG_B_FIR_LENGTH(x)		SET_BITS(7, 0, x)

#define FIR_CONFIG_B_FIR_DECIMATION_GET(x)	GET_BITS(20, 16, x)
#define FIR_CONFIG_B_FIR_SHIFT_GET(x)		GET_BITS(11, 8, x)
#define FIR_CONFIG_B_FIR_LENGTH_GET(x)		GET_BITS(7, 0, x)

/* DC_OFFSET_LEFT_B bits */
#define DC_OFFSET_LEFT_B_DC_OFFS(x)		SET_BITS(21, 0, x)

/* DC_OFFSET_RIGHT_B bits */
#define DC_OFFSET_RIGHT_B_DC_OFFS(x)		SET_BITS(21, 0, x)

/* OUT_GAIN_LEFT_B bits */
#define OUT_GAIN_LEFT_B_GAIN(x)			SET_BITS(19, 0, x)

/* OUT_GAIN_RIGHT_B bits */
#define OUT_GAIN_RIGHT_B_GAIN(x)		SET_BITS(19, 0, x)

/* FIR coefficients */
#define FIR_COEF_A(x)				SET_BITS(19, 0, x)
#define FIR_COEF_B(x)				SET_BITS(19, 0, x)

struct nhlt_dmic_gateway_attributes {
	uint32_t dw;
};

struct nhlt_dmic_ts_group {
	uint32_t ts_group[4];
};

struct nhlt_dmic_clock_on_delay {
	uint32_t clock_on_delay;
};

struct nhlt_dmic_channel_ctrl_mask {
	uint32_t channel_ctrl_mask;
};

struct nhlt_pdm_ctrl_mask {
	uint32_t pdm_ctrl_mask;
};

struct nhlt_pdm_ctrl_cfg {
	uint32_t cic_control;
	uint32_t cic_config;
	uint32_t reserved0;
	uint32_t mic_control;
	uint32_t pdm_sdw_map;
	uint32_t reuse_fir_from_pdm;
	uint32_t reserved1[2];
};

struct nhlt_pdm_ctrl_fir_cfg {
	uint32_t fir_control;
	uint32_t fir_config;
	int32_t dc_offset_left;
	int32_t dc_offset_right;
	int32_t out_gain_left;
	int32_t out_gain_right;
	uint32_t reserved[2];
};

struct nhlt_pdm_fir_coeffs {
	int32_t fir_coeffs[0];
};

/* stream PCM frame format */
enum sof_ipc_frame {
	SOF_IPC_FRAME_S16_LE = 0,
	SOF_IPC_FRAME_S24_4LE,
	SOF_IPC_FRAME_S32_LE,
	SOF_IPC_FRAME_FLOAT,
	/* other formats here */
};

/* DMIC private data */
struct dmic_pdata {
	struct dmic_global_shared *global;	/* Common data for all DMIC DAI instances */
	uint16_t enable[DMIC_HW_CONTROLLERS];	/* Mic 0 and 1 enable bits array for PDMx */
	uint32_t state;				/* Driver component state */
	int32_t startcount;			/* Counter in dmicwork that controls HW unmute */
	int32_t gain_coef;			/* Gain update constant */
	int32_t gain;				/* Gain value to be applied to HW */
	int irq;				/* Interrupt number used */
	enum sof_ipc_frame dai_format;		/* PCM format s32_le etc. */
	int dai_channels;			/* Channels count */
	int dai_rate;				/* Sample rate in Hz */
};

int print_dmic_blob_decode(uint8_t *blob, uint32_t len, int dmic_hw_ver);
