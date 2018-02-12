/*
 * Copyright (c) 2016, Intel Corporation
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
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __PLATFORM_SHIM_H__
#define __PLATFORM_SHIM_H__

#include <platform/memory.h>
#include <stdint.h>

#define SHIM_CSR		0x00
#define SHIM_ISRX		0x18
#define SHIM_ISRD		0x20
#define SHIM_IMRX		0x28
#define SHIM_IMRD		0x30
#define SHIM_IPCXL		0x38 /* IPC IA -> SST */
#define SHIM_IPCDL		0x40 /* IPC SST -> IA */

#define SHIM_CLKCTL		0x78

#define SHIM_CSR2		0x80
#define SHIM_LTRC		0xE0
#define SHIM_HMDC		0xE8

#define SHIM_SHIM_BEGIN		SHIM_CSR
#define SHIM_SHIM_END		SHIM_HMDC

/* CSR 0x0 */
#define SHIM_CSR_RST		(0x1 << 1)
#define SHIM_CSR_STALL		(0x1 << 10)
#define SHIM_CSR_SDPM0          (0x1 << 11)
#define SHIM_CSR_SDPM1          (0x1 << 12)
#define SHIM_CSR_SFCR0          (0x1 << 27)
#define SHIM_CSR_SFCR1          (0x1 << 28)
#define SHIM_CSR_DCS(x)         (x << 4)
#define SHIM_CSR_DCS_MASK       (0x7 << 4)

/*  ISRX 0x18 */
#define SHIM_ISRX_BUSY		(0x1 << 1)
#define SHIM_ISRX_DONE		(0x1 << 0)

/*  ISRD / ISD */
#define SHIM_ISRD_BUSY		(0x1 << 1)
#define SHIM_ISRD_DONE		(0x1 << 0)

/* IMRX / IMC */
#define SHIM_IMRX_BUSY		(0x1 << 1)
#define SHIM_IMRX_DONE		(0x1 << 0)

/* IMRD / IMD */
#define SHIM_IMRD_DONE		(0x1 << 0)
#define SHIM_IMRD_BUSY		(0x1 << 1)
#define SHIM_IMRD_SSP0		(0x1 << 16)
#define SHIM_IMRD_SSP1		(0x1 << 17)
#define SHIM_IMRD_DMAC0		(0x1 << 21)
#define SHIM_IMRD_DMAC1		(0x1 << 22)
#define SHIM_IMRD_DMAC		(SHIM_IMRD_DMAC0 | SHIM_IMRD_DMAC1)

/*  IPCX / IPCCH */
#define	SHIM_IPCXH_DONE		(0x1 << 30)
#define	SHIM_IPCXH_BUSY		(0x1 << 31)

/*  IPCDH */
#define	SHIM_IPCDH_DONE		(0x1 << 30)
#define	SHIM_IPCDH_BUSY		(0x1 << 31)

/* CLKCTL */
#define SHIM_CLKCTL_SMOS(x)	(x << 24)
#define SHIM_CLKCTL_MASK		(3 << 24)
#define SHIM_CLKCTL_DCPLCG	(1 << 18)
#define SHIM_CLKCTL_SSP1_EN	(1 << 17)
#define SHIM_CLKCTL_SSP0_EN	(1 << 16)

/* CSR2 / CS2 */
#define SHIM_CSR2_SDFD_SSP0	(1 << 1)
#define SHIM_CSR2_SDFD_SSP1	(1 << 2)

/* LTRC */
#define SHIM_LTRC_VAL(x)		(x << 0)

/* HMDC */
#define SHIM_HMDC_HDDA0(x)      (x << 0)
#define SHIM_HMDC_HDDA1(x)      (x << 8)
#define SHIM_HMDC_HDDA_CH_MASK  0xFF
#define SHIM_HMDC_HDDA_E0_ALLCH SHIM_HMDC_HDDA0(SHIM_HMDC_HDDA_CH_MASK)
#define SHIM_HMDC_HDDA_E1_ALLCH SHIM_HMDC_HDDA1(SHIM_HMDC_HDDA_CH_MASK)
#define SHIM_HMDC_HDDA_ALLCH    (SHIM_HMDC_HDDA_E0_ALLCH | SHIM_HMDC_HDDA_E1_ALLCH)

/* PMCS */
#define PCI_PMCS		0x84
#define PCI_PMCS_PS_MASK	0x3

static inline uint32_t shim_read(uint32_t reg)
{
	return *((volatile uint32_t*)(SHIM_BASE + reg));
}

static inline void shim_write(uint32_t reg, uint32_t val)
{
	*((volatile uint32_t*)(SHIM_BASE + reg)) = val;
}

#endif
