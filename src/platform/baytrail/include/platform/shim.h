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

#define SHIM_CSR		0x00
#define SHIM_PISR		0x08
#define SHIM_PIMR		0x10
#define SHIM_ISRX		0x18
#define SHIM_ISRD		0x20
#define SHIM_IMRX		0x28
#define SHIM_IMRD		0x30
#define SHIM_IPCXL		0x38 /* IPC IA -> SST */
#define SHIM_IPCXH		0x3c /* IPC IA -> SST */
#define SHIM_IPCDL		0x40 /* IPC SST -> IA */
#define SHIM_IPCDH		0x44 /* IPC SST -> IA */
#define SHIM_ISRSC		0x48
#define SHIM_ISRLPESC		0x50
#define SHIM_IMRSCL		0x58
#define SHIM_IMRSCH		0x5c
#define SHIM_IMRLPESC		0x60
#define SHIM_IPCSCL		0x68
#define SHIM_IPCSCH		0x6c
#define SHIM_IPCLPESCL		0x70
#define SHIM_IPCLPESCH		0x74
#define SHIM_CLKCTL		0x78
#define SHIM_FR_LAT_REQ		0x80
#define SHIM_MISC		0x88
#define SHIM_EXT_TIMER_CNTLL	0xC0
#define SHIM_EXT_TIMER_CNTLH	0xC4
#define SHIM_EXT_TIMER_STAT	0xC8
#define SHIM_SSP0_DIVL		0xE8
#define SHIM_SSP0_DIVH		0xEC
#define SHIM_SSP1_DIVL		0xF0
#define SHIM_SSP1_DIVH		0xF4
#define SHIM_SSP2_DIVL		0xF8
#define SHIM_SSP2_DIVH		0xFC

#define SHIM_SHIM_BEGIN		SHIM_CSR
#define SHIM_SHIM_END		SHIM_HDMC

/* CSR 0x0 */
#define SHIM_CSR_RST		(0x1 << 0)
#define SHIM_CSR_VECTOR_SEL	(0x1 << 1)
#define SHIM_CSR_STALL		(0x1 << 2)
#define SHIM_CSR_PWAITMODE	(0x1 << 3)

/* PISR */
#define SHIM_PISR_EXT_TIMER	(1 << 10)

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

/*  IPCX / IPCCH */
#define	SHIM_IPCXH_DONE		(0x1 << 30)
#define	SHIM_IPCXH_BUSY		(0x1 << 31)

/*  IPCDH */
#define	SHIM_IPCDH_DONE		(0x1 << 30)
#define	SHIM_IPCDH_BUSY		(0x1 << 31)

/* ISRLPESC */
#define SHIM_ISRLPESC_DONE	(0x1 << 0)
#define SHIM_ISRLPESC_BUSY	(0x1 << 1)

/* IMRLPESC */
#define	SHIM_IMRLPESC_BUSY	(0x1 << 1)
#define	SHIM_IMRLPESC_DONE	(0x1 << 0)

/* IPCSCH */
#define SHIM_IPCSCH_DONE	(0x1 << 30)
#define SHIM_IPCSCH_BUSY	(0x1 << 31)

/* IPCLPESCH */
#define SHIM_IPCLPESCH_DONE	(0x1 << 30)
#define SHIM_IPCLPESCH_BUSY	(0x1 << 31)

/* CLKCTL */
#define SHIM_CLKCTL_SSP2_EN	(1 << 18)
#define SHIM_CLKCTL_SSP1_EN	(1 << 17)
#define SHIM_CLKCTL_SSP0_EN	(1 << 16)
#define SHIM_CLKCTL_FRCHNGGO	(1 << 5)
#define SHIM_CLKCTL_FRCHNGACK	(1 << 4)

/* SHIM_FR_LAT_REQ */
#define SHIM_FR_LAT_CLK_MASK	0x7

/* ext timer */
#define SHIM_EXT_TIMER_RUN	(1 << 31)
#define SHIM_EXT_TIMER_CLEAR	(1 << 30)

static inline uint32_t shim_read(uint32_t reg)
{
	return *((volatile uint32_t*)(SHIM_BASE + reg));
}

static inline void shim_write(uint32_t reg, uint32_t val)
{
	*((volatile uint32_t*)(SHIM_BASE + reg)) = val;
}

#endif
