/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __SOF_DRIVERS_DMIC_H__
#define __SOF_DRIVERS_DMIC_H__

#if CONFIG_INTEL_DMIC

/* Let find up to 50 mode candidates to choose from */
#define DMIC_MAX_MODES 50

/* Minimum OSR is always applied for 48 kHz and less sample rates */
#define DMIC_MIN_OSR  50

/* These are used as guideline for configuring > 48 kHz sample rates. The
 * minimum OSR can be relaxed down to 40 (use 3.84 MHz clock for 96 kHz).
 */
#define DMIC_HIGH_RATE_MIN_FS	64000
#define DMIC_HIGH_RATE_OSR_MIN	40

/* HW FIR pipeline needs 5 additional cycles per channel for internal
 * operations. This is used in MAX filter length check.
 */
#define DMIC_FIR_PIPELINE_OVERHEAD 5

/* The microphones create a low frequecy thump sound when clock is enabled.
 * The unmute linear gain ramp chacteristic is defined here.
 * NOTE: Do not set any of these to 0.
 */
#define DMIC_UNMUTE_RAMP_US	1000	/* 1 ms (in microseconds) */
#define DMIC_UNMUTE_CIC		1	/* Unmute CIC at 1 ms */
#define DMIC_UNMUTE_FIR		2	/* Unmute FIR at 2 ms */

#if CONFIG_APOLLOLAKE
#define DMIC_HW_VERSION		1
#define DMIC_HW_CONTROLLERS	2
#define DMIC_HW_FIFOS		2
#endif

#if CONFIG_CANNONLAKE
#define DMIC_HW_VERSION		1
#define DMIC_HW_CONTROLLERS	2
#define DMIC_HW_FIFOS		2
#endif

#if CONFIG_SUECREEK
#define DMIC_HW_VERSION		2
#define DMIC_HW_CONTROLLERS	4
#define DMIC_HW_FIFOS		2
#endif

#if CONFIG_ICELAKE || CONFIG_TIGERLAKE
#define DMIC_HW_VERSION		1
#define DMIC_HW_CONTROLLERS	2
#define DMIC_HW_FIFOS		2
#endif

/* For NHLT DMIC configuration parsing */
#define DMIC_HW_CONTROLLERS_MAX	4
#define DMIC_HW_FIFOS_MAX	2

#endif

#if DMIC_HW_VERSION

#include <ipc/dai-intel.h>
#include <sof/audio/format.h>
#include <rtos/bit.h>
#include <sof/lib/dai.h>
#include <rtos/wait.h>
#include <stdint.h>

/* Parameters used in modes computation */
#define DMIC_HW_BITS_CIC		26
#define DMIC_HW_BITS_FIR_COEF		20
#define DMIC_HW_BITS_FIR_GAIN		20
#define DMIC_HW_BITS_FIR_INPUT		22
#define DMIC_HW_BITS_FIR_OUTPUT		24
#define DMIC_HW_BITS_FIR_INTERNAL	26
#define DMIC_HW_BITS_GAIN_OUTPUT	22
#define DMIC_HW_FIR_LENGTH_MAX		250
#define DMIC_HW_CIC_SHIFT_MIN		-8
#define DMIC_HW_CIC_SHIFT_MAX		4
#define DMIC_HW_FIR_SHIFT_MIN		0
#define DMIC_HW_FIR_SHIFT_MAX		8
#define DMIC_HW_CIC_DECIM_MIN		5
#define DMIC_HW_CIC_DECIM_MAX		31 /* Note: Limited by BITS_CIC */
#define DMIC_HW_FIR_DECIM_MIN		2
#define DMIC_HW_FIR_DECIM_MAX		20 /* Note: Practical upper limit */
#define DMIC_HW_SENS_Q28		Q_CONVERT_FLOAT(1.0, 28) /* Q1.28 */
#define DMIC_HW_PDM_CLK_MIN		100000 /* Note: Practical min value */
#define DMIC_HW_DUTY_MIN		20 /* Note: Practical min value */
#define DMIC_HW_DUTY_MAX		80 /* Note: Practical max value */

/* DMIC register offsets */

/* Global registers */
#define OUTCONTROL0		0x0000
#define OUTSTAT0		0x0004
#define OUTDATA0		0x0008
#define OUTCONTROL1		0x0100
#define OUTSTAT1		0x0104
#define OUTDATA1		0x0108
#define PDM0			0x1000
#define PDM0_COEFFICIENT_A	0x1400
#define PDM0_COEFFICIENT_B	0x1800
#define PDM1			0x2000
#define PDM1_COEFFICIENT_A	0x2400
#define PDM1_COEFFICIENT_B	0x2800
#define PDM2			0x3000
#define PDM2_COEFFICIENT_A	0x3400
#define PDM2_COEFFICIENT_B	0x3800
#define PDM3			0x4000
#define PDM3_COEFFICIENT_A	0x4400
#define PDM3_COEFFICIENT_B	0x4800
#define PDM_COEF_RAM_A_LENGTH	0x0400
#define PDM_COEF_RAM_B_LENGTH	0x0400

/* Local registers in each PDMx */
#define CIC_CONTROL		0x000
#define CIC_CONFIG		0x004
#define MIC_CONTROL		0x00c
#define FIR_CONTROL_A		0x020
#define FIR_CONFIG_A		0x024
#define DC_OFFSET_LEFT_A	0x028
#define DC_OFFSET_RIGHT_A	0x02c
#define OUT_GAIN_LEFT_A		0x030
#define OUT_GAIN_RIGHT_A	0x034
#define FIR_CONTROL_B		0x040
#define FIR_CONFIG_B		0x044
#define DC_OFFSET_LEFT_B	0x048
#define DC_OFFSET_RIGHT_B	0x04c
#define OUT_GAIN_LEFT_B		0x050
#define OUT_GAIN_RIGHT_B	0x054

/* Register bits */

/* OUTCONTROLx IPM bit fields style */
#if DMIC_HW_VERSION == 1 || (DMIC_HW_VERSION == 2 && DMIC_HW_CONTROLLERS <= 2)
#define DMIC_IPM_VER1
#elif DMIC_HW_VERSION == 2 && DMIC_HW_CONTROLLERS > 2
#define DMIC_IPM_VER2
#else
#error Not supported HW version
#endif

#define OUTCONTROL0_BFTH_MAX	4 /* Max depth 16 */

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

#if defined DMIC_IPM_VER2
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
#define OUTCONTROL0_IPM_SOURCE_MODE(x)		SET_BIT(6, x)
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
#define OUTCONTROL0_IPM_SOURCE_MODE_GET(x)	GET_BIT(6, x)
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
#define OUTCONTROL1_IPM_SOURCE_MODE(x)		SET_BIT(6, x)
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
#define OUTCONTROL1_IPM_SOURCE_MODE_GET(x)	GET_BIT(6, x)
#define OUTCONTROL1_TH_GET(x)			GET_BITS(5, 0, x)

#define OUTCONTROLX_IPM_NUMSOURCES		4

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
#define CIC_CONTROL_STEREO_MODE_BIT	BIT(0)

#define CIC_CONTROL_SOFT_RESET(x)	SET_BIT(16, x)
#define CIC_CONTROL_CIC_START_B(x)	SET_BIT(15, x)
#define CIC_CONTROL_CIC_START_A(x)	SET_BIT(14, x)
#define CIC_CONTROL_MIC_B_POLARITY(x)	SET_BIT(3, x)
#define CIC_CONTROL_MIC_A_POLARITY(x)	SET_BIT(2, x)
#define CIC_CONTROL_MIC_MUTE(x)		SET_BIT(1, x)
#define CIC_CONTROL_STEREO_MODE(x)	SET_BIT(0, x)

#define CIC_CONTROL_SOFT_RESET_GET(x)		GET_BIT(16, x)
#define CIC_CONTROL_CIC_START_B_GET(x)		GET_BIT(15, x)
#define CIC_CONTROL_CIC_START_A_GET(x)		GET_BIT(14, x)
#define CIC_CONTROL_MIC_B_POLARITY_GET(x)	GET_BIT(3, x)
#define CIC_CONTROL_MIC_A_POLARITY_GET(x)	GET_BIT(2, x)
#define CIC_CONTROL_MIC_MUTE_GET(x)		GET_BIT(1, x)
#define CIC_CONTROL_STEREO_MODE_GET(x)		GET_BIT(0, x)

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

/* Used for scaling FIR coefficients for HW */
#define DMIC_HW_FIR_COEF_MAX ((1 << (DMIC_HW_BITS_FIR_COEF - 1)) - 1)
#define DMIC_HW_FIR_COEF_Q (DMIC_HW_BITS_FIR_COEF - 1)

/* Internal precision in gains computation, e.g. Q4.28 in int32_t */
#define DMIC_FIR_SCALE_Q 28

/* Used in unmute ramp values calculation */
#define DMIC_HW_FIR_GAIN_MAX ((1 << (DMIC_HW_BITS_FIR_GAIN - 1)) - 1)

/* Hardwired log ramp parameters. The first value is the initial gain in
 * decibels. The default ramp time is provided by 1st order equation
 * ramp time = coef * samplerate + offset. The default ramp is 200 ms for
 * 48 kHz and 400 ms for 16 kHz.
 */
#define LOGRAMP_START_DB Q_CONVERT_FLOAT(-90, DB2LIN_FIXED_INPUT_QY)
#define LOGRAMP_TIME_COEF_Q15 -205 /* dy/dx (16000,400) (48000,200) */
#define LOGRAMP_TIME_OFFS_Q0 500 /* Offset for line slope */

/* Limits for ramp time from topology */
#define LOGRAMP_TIME_MIN_MS 10 /* Min. 10 ms */
#define LOGRAMP_TIME_MAX_MS 1000 /* Max. 1s */

/* Simplify log ramp step calculation equation with this constant term */
#define LOGRAMP_CONST_TERM ((int32_t) \
	((int64_t)-LOGRAMP_START_DB * DMIC_UNMUTE_RAMP_US / 1000))

/* Fractional shift for gain update. Gain format is Q2.30. */
#define Q_SHIFT_GAIN_X_GAIN_COEF \
	(Q_SHIFT_BITS_32(30, DB2LIN_FIXED_OUTPUT_QY, 30))

#define dmic_irq(dmic) dmic->plat_data.irq
#define dmic_irq_name(dmic) dmic->plat_data.irq_name

/* Common data for all DMIC DAI instances */
struct dmic_global_shared {
	struct sof_ipc_dai_dmic_params prm[DMIC_HW_FIFOS];  /* Configuration requests */
	uint32_t active_fifos_mask;	/* Bits (dai->index) are set to indicate active FIFO */
	uint32_t pause_mask;		/* Bits (dai->index) are set to indicate driver pause */
};

/* DMIC private data */
struct dmic_pdata {
	struct dmic_global_shared *global;	/* Common data for all DMIC DAI instances */
	uint16_t enable[DMIC_HW_CONTROLLERS];	/* Mic 0 and 1 enable bits array for PDMx */
	uint32_t state;				/* Driver component state */
	int32_t startcount;			/* Counter that controls HW unmute */
	int32_t gain_coef;			/* Gain update constant */
	int32_t gain;				/* Gain value to be applied to HW */
	int32_t unmute_ramp_time_ms;		/* Unmute ramp time in milliseconds */
	int irq;				/* Interrupt number used */
	enum sof_ipc_frame dai_format;		/* PCM format s32_le etc. */
	int dai_channels;			/* Channels count */
	int dai_rate;				/* Sample rate in Hz */
};

struct decim_modes {
	int16_t clkdiv[DMIC_MAX_MODES];
	int16_t mcic[DMIC_MAX_MODES];
	int16_t mfir[DMIC_MAX_MODES];
	int num_of_modes;
};

struct matched_modes {
	int16_t clkdiv[DMIC_MAX_MODES];
	int16_t mcic[DMIC_MAX_MODES];
	int16_t mfir_a[DMIC_MAX_MODES];
	int16_t mfir_b[DMIC_MAX_MODES];
	int num_of_modes;
};

struct dmic_configuration {
	struct pdm_decim *fir_a;
	struct pdm_decim *fir_b;
	int clkdiv;
	int mcic;
	int mfir_a;
	int mfir_b;
	int cic_shift;
	int fir_a_shift;
	int fir_b_shift;
	int fir_a_length;
	int fir_b_length;
	int32_t fir_a_scale;
	int32_t fir_b_scale;
};

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

int dmic_set_config_computed(struct dai *dai);
int dmic_get_hw_params_computed(struct dai *dai, struct sof_ipc_stream_params *params, int dir);
int dmic_set_config_nhlt(struct dai *dai, void *spec_config);
int dmic_get_hw_params_nhlt(struct dai *dai, struct sof_ipc_stream_params *params, int dir);

extern const struct dai_driver dmic_driver;

static inline int dmic_get_unmute_ramp_from_samplerate(int rate)
{
	int32_t time_ms;

	time_ms = sat_int32(Q_MULTSR_32X32((int64_t)rate, LOGRAMP_TIME_COEF_Q15, 0, 15, 0) +
		LOGRAMP_TIME_OFFS_Q0);
	if (time_ms > LOGRAMP_TIME_MAX_MS)
		return LOGRAMP_TIME_MAX_MS;

	if (time_ms < LOGRAMP_TIME_MIN_MS)
		return LOGRAMP_TIME_MIN_MS;

	return time_ms;
}

#endif /* DMIC_HW_VERSION  */
#endif /* __SOF_DRIVERS_DMIC_H__ */
