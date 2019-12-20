/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_DRIVERS_TIMESTAMP_H__
#define __SOF_DRIVERS_TIMESTAMP_H__

/* cAVS1.5 platforms have discontinuous registers space for SSP
 * timestamping while other platforms don't have this.
 */
#if CONFIG_APOLLOLAKE
#define TS_DMIC_LOCAL_TSCTRL	0x000
#define TS_DMIC_LOCAL_OFFS	0x004
#define TS_DMIC_LOCAL_SAMPLE	0x008
#define TS_DMIC_LOCAL_WALCLK	0x010
#define TS_DMIC_TSCC		0x018
#define TS_HDA_LOCAL_TSCTRL	(0x0e0 + 0x000)
#define TS_HDA_LOCAL_OFFS	(0x0e0 + 0x004)
#define TS_HDA_LOCAL_SAMPLE	(0x0e0 + 0x008)
#define TS_HDA_LOCAL_WALCLK	(0x0e0 + 0x010)
#define TS_HDA_TSCC		(0x0e0 + 0x018)
#define TS_I2S_LOCAL_TSCTRL(y)	(0x40 + 0x20 * (y) + 0x000)
#define TS_I2S_LOCAL_OFFS(y)	(0x40 + 0x20 * (y) + 0x004)
#define TS_I2S_LOCAL_SAMPLE(y)	(0x40 + 0x20 * (y) + 0x008)
#define TS_I2S_LOCAL_WALCLK(y)	(0x40 + 0x20 * (y) + 0x010)
#define TS_I2S_TSCC(y)		(0x40 + 0x20 * (y) + 0x018)
#define TS_I2SE_LOCAL_TSCTRL(w)	(0x120 + 0x20 * ((w) - 4) + 0x000)
#define TS_I2SE_LOCAL_OFFS(w)	(0x120 + 0x20 * ((w) - 4) + 0x004)
#define TS_I2SE_LOCAL_SAMPLE(w)	(0x120 + 0x20 * ((w) - 4) + 0x008)
#define TS_I2SE_LOCAL_WALCLK(w)	(0x120 + 0x20 * ((w) - 4) + 0x010)
#define TS_I2SE_TSCC(w)		(0x120 + 0x20 * ((w) - 4) + 0x018)

#elif CONFIG_CANNONLAKE || CONFIG_ICELAKE || CONFIG_SUECREEK || \
	CONFIG_TIGERLAKE

#define TS_DMIC_LOCAL_TSCTRL	0x000
#define TS_DMIC_LOCAL_OFFS      0x004
#define TS_DMIC_LOCAL_SAMPLE	0x008
#define TS_DMIC_LOCAL_WALCLK	0x010
#define TS_DMIC_TSCC		0x018
#define TS_HDA_LOCAL_TSCTRL	(0x0e0 + 0x000)
#define TS_HDA_LOCAL_OFFS	(0x0e0 + 0x004)
#define TS_HDA_LOCAL_SAMPLE	(0x0e0 + 0x008)
#define TS_HDA_LOCAL_WALCLK	(0x0e0 + 0x010)
#define TS_HDA_TSCC		(0x0e0 + 0x018)
#define TS_I2S_LOCAL_TSCTRL(y)	(0x100 + 0x20 * (y) + 0x000)
#define TS_I2S_LOCAL_OFFS(y)	(0x100 + 0x20 * (y) + 0x004)
#define TS_I2S_LOCAL_SAMPLE(y)	(0x100 + 0x20 * (y) + 0x008)
#define TS_I2S_LOCAL_WALCLK(y)	(0x100 + 0x20 * (y) + 0x010)
#define TS_I2S_TSCC(y)		(0x100 + 0x20 * (y) + 0x018)
#endif

#define TS_LOCAL_TSCTRL_NTK_BIT	        BIT(31)
#define TS_LOCAL_TSCTRL_IONTE_BIT	BIT(30)
#define TS_LOCAL_TSCTRL_SIP_BIT	        BIT(8)
#define TS_LOCAL_TSCTRL_HHTSE_BIT	BIT(7)
#define TS_LOCAL_TSCTRL_ODTS_BIT	BIT(5)
#define TS_LOCAL_TSCTRL_CDMAS(x)	SET_BITS(4, 0, x)
#define TS_LOCAL_OFFS_FRM		GET_BITS(15, 12)
#define TS_LOCAL_OFFS_CLK		GET_BITS(11, 0)

#elif CONFIG_BAYTRAIL || CONFIG_CHERRYTRAIL || CONFIG_BROADWELL ||
	CONFIG_HASWELL

/* There's no timestamping support in pre-cAVS platforms. */

#else

#error "Unknown platform"

#endif
