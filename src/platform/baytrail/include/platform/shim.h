/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __PLATFORM_SHIM_H__
#define __PLATFORM_SHIM_H__

#include <platform/memory.h>
#include <stdint.h>

#define SHIM_CSR			0x00
#define SHIM_PISR		0x08
#define SHIM_PIMR		0x10
#define SHIM_ISRX		0x18
#define SHIM_ISRD		0x20
#define SHIM_IMRX		0x28
#define SHIM_IMRD		0x30
#define SHIM_IPCX		0x3C /* IPC IA -> SST */
#define SHIM_IPCD		0x44 /* IPC SST -> IA */
#define SHIM_ISRSC		0x48
#define SHIM_ISRLPESC		0x50
#define SHIM_IMRSC		0x58
#define SHIM_IMRLPESC		0x60
#define SHIM_IPCSC		0x68
#define SHIM_IPCLPESC		0x70
#define SHIM_CLKCTL		0x78
#define SHIM_CSR2		0x80
#define SHIM_LTRC		0xE0
#define SHIM_HMDC		0xE8

#define SHIM_SHIM_BEGIN		SHIM_CSR
#define SHIM_SHIM_END		SHIM_HDMC

/* CSR 0x0 */
#define SHIM_CSR_RST		(0x1 << 0)
#define SHIM_CSR_VECTOR_SEL	(0x1 << 1)
#define SHIM_CSR_STALL		(0x1 << 2)
#define SHIM_CSR_PWAITMODE	(0x1 << 3)
#define SHIM_CSR_DCS(x)		(x << 4)
#define SHIM_CSR_DCS_MASK	(0x7 << 4)

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
#define SHIM_IMRD_DMAC0		(0x1 << 21)
#define SHIM_IMRD_DMAC1		(0x1 << 22)
#define SHIM_IMRD_DMAC		(SHIM_IMRD_DMAC0 | SHIM_IMRD_DMAC1)

/*  IPCX / IPCC */
#define	SHIM_IPCX_DONE		(0x1 << 30)
#define	SHIM_IPCX_BUSY		(0x1 << 31)

/*  IPCD */
#define	SHIM_IPCD_DONE		(0x1 << 30)
#define	SHIM_IPCD_BUSY		(0x1 << 31)

/* CLKCTL */
#define SHIM_CLKCTL_SMOS(x)	(x << 24)
#define SHIM_CLKCTL_MASK		(3 << 24)
#define SHIM_CLKCTL_DCPLCG	(1 << 18)
#define SHIM_CLKCTL_SCOE1	(1 << 17)
#define SHIM_CLKCTL_SCOE0	(1 << 16)

static inline uint32_t shim_read(uint32_t reg)
{
	return *((volatile uint32_t*)(SHIM_BASE + reg));
}

static inline uint32_t shim_write(uint32_t reg, uint32_t val)
{
	*((volatile uint32_t*)(SHIM_BASE + reg)) = val;
}


#endif
