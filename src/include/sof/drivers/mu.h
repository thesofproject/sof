/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 * Copyright 2020 NXP
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifndef __SOF_DRIVERS_MU_H__
#define __SOF_DRIVERS_MU_H__

#include <rtos/bit.h>
#include <rtos/clk.h>
#include <stdint.h>

enum imx_mu_type {
	IMX_MU_V1,
	IMX_MU_V2,
};

#ifdef CONFIG_IMX8ULP
#define IMX_MU_VERSION IMX_MU_V2
#else
#define IMX_MU_VERSION IMX_MU_V1
#endif

enum imx_mu_xcr {
	IMX_MU_GIER = 0x110,
	IMX_MU_GCR = 0x114,
	IMX_MU_TCR = 0x120,
	IMX_MU_RCR = 0x128,
};

enum imx_mu_xsr {
	IMX_MU_SR = 0x0c,
	IMX_MU_GSR = 0x118,
	IMX_MU_TSR = 0x124,
	IMX_MU_RSR = 0x12c,
};

#ifdef CONFIG_IMX8ULP
/* Transmit Register */
#define IMX_MU_xTRn(x)         (0x200 + 4 * (x))
/* Receive Register */
#define IMX_MU_xRRn(x)         (0x280 + 4 * (x))

#else
/* Transmit Register */
#define IMX_MU_xTRn(x)		(0x00 + 4 * (x))
/* Receive Register */
#define IMX_MU_xRRn(x)		(0x10 + 4 * (x))
#endif

/* Status Register */
#define IMX_MU_xSR(type, index)		(type == IMX_MU_V2 ? index : 0x20)
#define IMX_MU_xSR_GIPn(type, x)	(type == IMX_MU_V2 ? BIT(x) : BIT(28 + (3 - (x))))
#define IMX_MU_xSR_RFn(type, x)		(type == IMX_MU_V2 ? BIT(x) : BIT(24 + (3 - (x))))
#define IMX_MU_xSR_TEn(type, x)		(type == IMX_MU_V2 ? BIT(x) : BIT(20 + (3 - (x))))
#define IMX_MU_xSR_BRDIP	BIT(9)

/* Control Register */
#define IMX_MU_xCR(type, index)		(type == IMX_MU_V2 ? index : 0x24)
/* General Purpose Interrupt Enable */
#define IMX_MU_xCR_GIEn(type, x)	(type == IMX_MU_V2 ? BIT(x) : BIT(28 + (3 - (x))))
/* Receive Interrupt Enable */
#define IMX_MU_xCR_RIEn(type, x)	(type == IMX_MU_V2 ? BIT(x) : BIT(24 + (3 - (x))))
/* Transmit Interrupt Enable */
#define IMX_MU_xCR_TIEn(type, x)	(type == IMX_MU_V2 ? BIT(x) : BIT(20 + (3 - (x))))
/* General Purpose Interrupt Request */
#define IMX_MU_xCR_GIRn(type, x)	(type == IMX_MU_V2 ? BIT(x) : BIT(16 + (3 - (x))))

static inline uint32_t imx_mu_read(uint32_t reg)
{
	return *((volatile uint32_t*)(MU_BASE + reg));
}

static inline void imx_mu_write(uint32_t val, uint32_t reg)
{
	*((volatile uint32_t*)(MU_BASE + reg)) = val;
}

static inline uint32_t imx_mu_xcr_rmw(int type, int idx, uint32_t set, uint32_t clr)
{
	volatile uint32_t val;

	val = imx_mu_read(IMX_MU_xCR(type, idx));
	val &= ~clr;
	val |= set;
	imx_mu_write(val, IMX_MU_xCR(type, idx));

	return val;
}

static inline uint32_t imx_mu_xsr_rmw(int type, int idx, uint32_t set, uint32_t clr)
{
	volatile uint32_t val;

	val = imx_mu_read(IMX_MU_xSR(type, idx));
	val &= ~clr;
	val |= set;
	imx_mu_write(val, IMX_MU_xSR(type, idx));

	return val;
}

#endif /* __SOF_DRIVERS_MU_H__ */
