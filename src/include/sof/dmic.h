/*
 * Copyright (c) 2017, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author:	Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __INCLUDE_DMIC__
#define __INCLUDE_DMIC__

#include <config.h>
#include <sof/dai.h>
#include <sof/io.h>

#if defined CONFIG_DMIC

/* The microphones create a low frequecy thump sound when clock is enabled.
 * The unmute linear gain ramp chacteristic is defined here.
 * NOTE: Do not set any of these to 0.
 */
#define DMIC_UNMUTE_RAMP_US	1000	/* 1 ms (in microseconds) */
#define DMIC_UNMUTE_CIC		1	/* Unmute CIC at 1 ms */
#define DMIC_UNMUTE_FIR		2	/* Unmute FIR at 2 ms */

#if defined CONFIG_APOLLOLAKE
#define DMIC_HW_VERSION		1
#define DMIC_HW_CONTROLLERS	2
#define DMIC_HW_IOCLK		19200000
#endif

#if defined CONFIG_CANNONLAKE || defined CONFIG_SUECREEK
#define DMIC_HW_VERSION		1
#define DMIC_HW_CONTROLLERS	2
#define DMIC_HW_IOCLK		24000000
#endif

#if defined CONFIG_ICELAKE
#define DMIC_HW_VERSION		1
#define DMIC_HW_CONTROLLERS	2
#define DMIC_HW_IOCLK		38400000
#endif

#endif

#if defined DMIC_HW_VERSION

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

#if DMIC_HW_VERSION == 1
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
#endif

#if DMIC_HW_VERSION == 2
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

/* CIC_CONFIG bits */
#define CIC_CONFIG_CIC_SHIFT(x)		SET_BITS(27, 24, x)
#define CIC_CONFIG_COMB_COUNT(x)	SET_BITS(15, 8, x)

/* MIC_CONTROL bits */
#define MIC_CONTROL_PDM_EN_B_BIT	BIT(1)
#define MIC_CONTROL_PDM_EN_A_BIT	BIT(0)
#define MIC_CONTROL_PDM_CLKDIV(x)	SET_BITS(15, 8, x)
#define MIC_CONTROL_PDM_SKEW(x)		SET_BITS(7, 4, x)
#define MIC_CONTROL_CLK_EDGE(x)		SET_BIT(3, x)
#define MIC_CONTROL_PDM_EN_B(x)		SET_BIT(1, x)
#define MIC_CONTROL_PDM_EN_A(x)		SET_BIT(0, x)

/* FIR_CONTROL_A bits */
#define FIR_CONTROL_A_START_BIT			BIT(7)
#define FIR_CONTROL_A_ARRAY_START_EN_BIT	BIT(6)
#define FIR_CONTROL_A_MUTE_BIT			BIT(1)
#define FIR_CONTROL_A_START(x)			SET_BIT(7, x)
#define FIR_CONTROL_A_ARRAY_START_EN(x)		SET_BIT(6, x)
#define FIR_CONTROL_A_DCCOMP(x)			SET_BIT(4, x)
#define FIR_CONTROL_A_MUTE(x)			SET_BIT(1, x)
#define FIR_CONTROL_A_STEREO(x)			SET_BIT(0, x)

/* FIR_CONFIG_A bits */
#define FIR_CONFIG_A_FIR_DECIMATION(x)		SET_BITS(20, 16, x)
#define FIR_CONFIG_A_FIR_SHIFT(x)		SET_BITS(11, 8, x)
#define FIR_CONFIG_A_FIR_LENGTH(x)		SET_BITS(7, 0, x)

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

/* FIR_CONFIG_B bits */
#define FIR_CONFIG_B_FIR_DECIMATION(x)		SET_BITS(20, 16, x)
#define FIR_CONFIG_B_FIR_SHIFT(x)		SET_BITS(11, 8, x)
#define FIR_CONFIG_B_FIR_LENGTH(x)		SET_BITS(7, 0, x)

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

#define dmic_irq(dmic) \
	dmic->plat_data.irq

/* DMIC private data */
struct dmic_pdata {
	spinlock_t lock; /* Spinlock that's used in registers IO */
	uint16_t fifo_a;
	uint16_t fifo_b;
	uint16_t enable[DMIC_HW_CONTROLLERS];
	uint32_t state;
	completion_t drain_complete;
	struct sof_ipc_dai_config config;
	struct sof_ipc_dai_dmic_params params;
	struct work dmicwork;
	int32_t startcount;
	int32_t gain;
};

extern const struct dai_ops dmic_ops;

#endif /* DMIC_HW_VERSION  */
#endif /* __INCLUDE_DMIC__ */
