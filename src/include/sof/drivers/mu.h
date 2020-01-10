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

#include <sof/bit.h>
#include <stdint.h>

/* Transmit Register */
#define IMX_MU_xTRn(x)		(0x00 + 4 * (x))
/* Receive Register */
#define IMX_MU_xRRn(x)		(0x10 + 4 * (x))
/* Status Register */
#define IMX_MU_xSR		0x20
#define IMX_MU_xSR_GIPn(x)	BIT(28 + (3 - (x)))
#define IMX_MU_xSR_RFn(x)	BIT(24 + (3 - (x)))
#define IMX_MU_xSR_TEn(x)	BIT(20 + (3 - (x)))
#define IMX_MU_xSR_BRDIP	BIT(9)

/* Control Register */
#define IMX_MU_xCR		0x24
/* General Purpose Interrupt Enable */
#define IMX_MU_xCR_GIEn(x)	BIT(28 + (3 - (x)))
/* Receive Interrupt Enable */
#define IMX_MU_xCR_RIEn(x)	BIT(24 + (3 - (x)))
/* Transmit Interrupt Enable */
#define IMX_MU_xCR_TIEn(x)	BIT(20 + (3 - (x)))
/* General Purpose Interrupt Request */
#define IMX_MU_xCR_GIRn(x)	BIT(16 + (3 - (x)))

static inline uint32_t imx_mu_read(uint32_t reg)
{
	return *((volatile uint32_t*)(MU_BASE + reg));
}

static inline void imx_mu_write(uint32_t val, uint32_t reg)
{
	*((volatile uint32_t*)(MU_BASE + reg)) = val;
}

static inline uint32_t imx_mu_xcr_rmw(uint32_t set, uint32_t clr)
{
	volatile uint32_t val;

	val = imx_mu_read(IMX_MU_xCR);
	val &= ~clr;
	val |= set;
	imx_mu_write(val, IMX_MU_xCR);

	return val;
}

static inline uint32_t imx_mu_xsr_rmw(uint32_t set, uint32_t clr)
{
	volatile uint32_t val;

	val = imx_mu_read(IMX_MU_xSR);
	val &= ~clr;
	val |= set;
	imx_mu_write(val, IMX_MU_xSR);

	return val;
}

#endif /* __SOF_DRIVERS_MU_H__ */
