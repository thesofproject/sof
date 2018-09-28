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
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
 */

#ifndef __PLATFORM_SHIM_H__
#define __PLATFORM_SHIM_H__

#include <platform/memory.h>

#ifndef ASSEMBLY
#include <stdint.h>
#endif

/* DSP IPC for Host Registers */
#define IPC_DIPCTDR		0x00
#define IPC_DIPCTDA		0x04
#define IPC_DIPCTDD		0x08
#define IPC_DIPCIDR		0x10
#define IPC_DIPCIDA		0x14
#define IPC_DIPCIDD		0x18
#define IPC_DIPCCTL		0x28


/* DIPCTDR */
#define IPC_DIPCTDR_BUSY	(1 << 31)
#define IPC_DIPCTDR_MSG_MASK	0x7FFFFFFF

/* DIPCTDA */
#define IPC_DIPCTDA_BUSY	(1 << 31)
#define IPC_DIPCTDA_MSG_MASK	0x7FFFFFFF

/* DIPCTE */
#define IPC_DIPCTE_MSG_MASK	0x3FFFFFFF

/* DIPCIDA*/
#define IPC_DIPCIDA_DONE	(1 << 31)

/* DIPCIDR */
#define IPC_DIPCIDR_BUSY	(1 << 31)
#define IPC_DIPCIDR_MSG_MASK	0x7FFFFFFF

/* DIPCIE */
#define IPC_DIPCIE_DONE		(1 << 31)
#define IPC_DIPCIE_MSG_MASK	0x3FFFFFFF

/* DIPCCTL */
#define IPC_DIPCCTL_IPCIDIE	(1 << 1)
#define IPC_DIPCCTL_IPCTBIE	(1 << 0)

#define IPC_DSP_OFFSET		0x10

/* DSP IPC for intra DSP communication */
#define IPC_IDCTFC(x)		(0x0 + x * IPC_DSP_OFFSET)
#define IPC_IDCTEFC(x)		(0x4 + x * IPC_DSP_OFFSET)
#define IPC_IDCITC(x)		(0x8 + x * IPC_DSP_OFFSET)
#define IPC_IDCIETC(x)		(0xc + x * IPC_DSP_OFFSET)
#define IPC_IDCCTL		0x50

/* IDCTFC */
#define IPC_IDCTFC_BUSY		(1 << 31)
#define IPC_IDCTFC_MSG_MASK	0x7FFFFFFF

/* IDCTEFC */
#define IPC_IDCTEFC_MSG_MASK	0x3FFFFFFF

/* IDCITC */
#define IPC_IDCITC_BUSY		(1 << 31)
#define IPC_IDCITC_MSG_MASK	0x7FFFFFFF

/* IDCIETC */
#define IPC_IDCIETC_DONE	(1 << 30)
#define IPC_IDCIETC_MSG_MASK	0x3FFFFFFF

/* IDCCTL */
#define IPC_IDCCTL_IDCIDIE(x)	(0x100 << (x))
#define IPC_IDCCTL_IDCTBIE(x)	(0x1 << (x))

#define IRQ_CPU_OFFSET	0x40

#define REG_IRQ_IL2MSD(xcpu)	(0x0 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL2MCD(xcpu)	(0x4 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL2MD(xcpu)	(0x8 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL2SD(xcpu)	(0xc + (xcpu * IRQ_CPU_OFFSET))

/* all mask valid bits */
#define REG_IRQ_IL2MD_ALL		0x03F181F0

#define REG_IRQ_IL3MSD(xcpu)	(0x10 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL3MCD(xcpu)	(0x14 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL3MD(xcpu)	(0x18 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL3SD(xcpu)	(0x1c + (xcpu * IRQ_CPU_OFFSET))

/* all mask valid bits */
#define REG_IRQ_IL3MD_ALL		0x807F81FF

#define REG_IRQ_IL4MSD(xcpu)	(0x20 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL4MCD(xcpu)	(0x24 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL4MD(xcpu)	(0x28 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL4SD(xcpu)	(0x2c + (xcpu * IRQ_CPU_OFFSET))

/* all mask valid bits */
#define REG_IRQ_IL4MD_ALL		0x807F81FF

#define REG_IRQ_IL5MSD(xcpu)	(0x30 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL5MCD(xcpu)	(0x34 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL5MD(xcpu)	(0x38 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL5SD(xcpu)	(0x3c + (xcpu * IRQ_CPU_OFFSET))

/* all mask valid bits */
#define REG_IRQ_IL5MD_ALL		0xFFFFC0CF

#define REG_IRQ_IL2RSD		0x100
#define REG_IRQ_IL3RSD		0x104
#define REG_IRQ_IL4RSD		0x108
#define REG_IRQ_IL5RSD		0x10c

#define REG_IRQ_LVL5_LP_GPDMA0_MASK		(0xff << 16)
#define REG_IRQ_LVL5_LP_GPDMA1_MASK		(0xff << 24)

/* DSP Shim Registers */
#define SHIM_DSPWC		0x20 /* DSP Wall Clock */
#define SHIM_DSPWCTCS		0x28 /* DSP Wall Clock Timer Control & Status */
#define SHIM_DSPWCT0C		0x30 /* DSP Wall Clock Timer 0 Compare */
#define SHIM_DSPWCT1C		0x38 /* DSP Wall Clock Timer 1 Compare */

#define SHIM_DSPWCTCS_T1T	(0x1 << 5) /* Timer 1 triggered */
#define SHIM_DSPWCTCS_T0T	(0x1 << 4) /* Timer 0 triggered */
#define SHIM_DSPWCTCS_T1A	(0x1 << 1) /* Timer 1 armed */
#define SHIM_DSPWCTCS_T0A	(0x1 << 0) /* Timer 0 armed */

#define SHIM_CLKCTL		0x78
#define SHIM_CLKSTS		0x7C

#define SHIM_PWRCTL		0x90
#define SHIM_PWRSTS		0x92
#define SHIM_PWRCTL_TCPDSP0PG	(0x1 << 0)

/* LP GPDMA Force Dynamic Clock Gating bits, 0--enable */
#define SHIM_CLKCTL_LPGPDMAFDCGB(x)	(0x1 << (26 + x))
#define SHIM_CLKCTL_TCPLCG(x)		(0x1 << (16 + x))

/* Core clock PLL divisor */
#define SHIM_CLKCTL_DPCS_MASK(x)	(0x1 << 2)

/* Prevent Audio PLL Shutdown */
#define SHIM_CLKCTL_TCPAPLLS	(0x1 << 7)

/* 0--from PLL, 1--from oscillator */
#define SHIM_CLKCTL_HDCS	(0x1 << 4)

/* Oscillator select */
#define SHIM_CLKCTL_HDOCS	(0x1 << 2)

/* HP memory clock PLL divisor */
#define SHIM_CLKCTL_HPMPCS	(0x1 << 0)

#define GPDMA_CLKCTL(x)		(0x78404 + x*0x100)
#define GPDMA_FDCGB		(0x1 << 0)

#define L2LMCAP			0x71D00
#define L2MPAT			0x71D04

#define HSPGCTL0		0x71D10
#define HSRMCTL0		0x71D14
#define HSPGISTS0		0x71D18

#define HSPGCTL1		0x71D20
#define HSRMCTL1		0x71D24
#define HSPGISTS1		0x71D28

#define LSPGCTL			0x71D50
#define LSRMCTL			0x71D54
#define LSPGISTS		0x71D58

#define SHIM_L2_MECS		(SHIM_BASE + 0xd0)

#define SHIM_LDOCTL		0xA4
#define SHIM_HPMEM_POWER_ON	(0x3 << 0)
#define SHIM_LPMEM_POWER_BYPASS	(0x1 << 0)

#define DSP_INIT_LPGPDMA(x)	(0x71A60 + (2*x))
#define LPGPDMA_CTLOSEL_FLAG	(1 << 15)
#define LPGPDMA_CHOSEL_FLAG	(0xFF)

#define DSP_INIT_IOPO	0x71A68
#define IOPO_DMIC_FLAG		(1 << 0)
#define IOPO_I2S_FLAG		(7 << 8)

#define DSP_INIT_GENO	0x71A6C
#define GENO_MDIVOSEL		(1 << 1)
#define GENO_DIOPTOSEL		(1 << 2)

#define DSP_INIT_ALHO	0x71A70
#define ALHO_ASO_FLAG		(1 << 0)
#define ALHO_CSO_FLAG		(1 << 1)
#define ALHO_CFO_FLAG		(1 << 2)

#define SHIM_SVCFG			0xF4
#define SHIM_SVCFG_FORCE_L1_EXIT	(0x1 << 1)

/* host windows */
#define DMWBA(x)		(HOST_WIN_BASE(x) + 0x0)
#define DMWLO(x)		(HOST_WIN_BASE(x) + 0x4)

#define DMWBA_ENABLE		(1 << 0)
#define DMWBA_READONLY		(1 << 1)

#ifndef ASSEMBLY

static inline uint16_t shim_read16(uint16_t reg)
{
	return *((volatile uint16_t*)(SHIM_BASE + reg));
}

static inline void shim_write16(uint16_t reg, uint16_t val)
{
	*((volatile uint16_t*)(SHIM_BASE + reg)) = val;
}

static inline uint32_t shim_read(uint32_t reg)
{
	return *((volatile uint32_t*)(SHIM_BASE + reg));
}

static inline void shim_write(uint32_t reg, uint32_t val)
{
	*((volatile uint32_t*)(SHIM_BASE + reg)) = val;
}

static inline uint64_t shim_read64(uint32_t reg)
{
	return *((volatile uint64_t*)(SHIM_BASE + reg));
}

static inline void shim_write64(uint32_t reg, uint64_t val)
{
	*((volatile uint64_t*)(SHIM_BASE + reg)) = val;
}

// TODO: this should be BUILD_MAILBOX
#if !defined CONFIG_SUECREEK
static inline uint32_t sw_reg_read(uint32_t reg)
{
	return *((volatile uint32_t*)((SRAM_SW_REG_BASE -
		SRAM_ALIAS_OFFSET) + reg));
}

static inline void sw_reg_write(uint32_t reg, uint32_t val)
{
	*((volatile uint32_t*)((SRAM_SW_REG_BASE -
		SRAM_ALIAS_OFFSET) + reg)) = val;
}
#endif

static inline uint32_t mn_reg_read(uint32_t reg)
{
	return *((volatile uint32_t*)(MN_BASE + reg));
}

static inline void mn_reg_write(uint32_t reg, uint32_t val)
{
	*((volatile uint32_t*)(MN_BASE + reg)) = val;
}

static inline uint32_t irq_read(uint32_t reg)
{
	return *((volatile uint32_t*)(IRQ_BASE + reg));
}

static inline void irq_write(uint32_t reg, uint32_t val)
{
	*((volatile uint32_t*)(IRQ_BASE + reg)) = val;
}

static inline uint32_t ipc_read(uint32_t reg)
{
	return *((volatile uint32_t*)(IPC_HOST_BASE + reg));
}

static inline void ipc_write(uint32_t reg, uint32_t val)
{
	*((volatile uint32_t*)(IPC_HOST_BASE + reg)) = val;
}

static inline uint32_t idc_read(uint32_t reg, uint32_t core_id)
{
	return *((volatile uint32_t*)(IPC_DSP_BASE(core_id) + reg));
}

static inline void idc_write(uint32_t reg, uint32_t core_id, uint32_t val)
{
	*((volatile uint32_t*)(IPC_DSP_BASE(core_id) + reg)) = val;
}
#endif

#endif
