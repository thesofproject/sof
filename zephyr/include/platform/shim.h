/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_LIB_PLATFORM_HOST_SHIM_H__
#define __INCLUDE_LIB_PLATFORM_HOST_SHIM_H__

#include <platform/memory.h>
#include <stdint.h>

/* APL DSP IPC for Host Register */
#define IPC_DIPCT	0x00
#define IPC_DIPCTE	0x04
#define IPC_DIPCI	0x08
#define IPC_DIPCIE	0x0C
#define IPC_DIPCCTL	0x10

/* DIPCT */
#define IPC_DIPCT_BUSY		(1<<31)
#define IPC_DIPCT_MSG_MASK	0x7FFFFFFF

/* DIPCTE */
#define IPC_DIPCTE_MSG_MASK	0x3FFFFFFF

/* DIPCI */
#define IPC_DIPCI_BUSY		(1 << 31)
#define IPC_DIPCI_MSG_MASK	0x7FFFFFFF

/* DIPCIE */
#define IPC_DIPCIE_DONE		(1 << 30)
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

