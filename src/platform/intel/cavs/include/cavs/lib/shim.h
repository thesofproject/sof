/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifdef __PLATFORM_LIB_SHIM_H__

#ifndef __CAVS_LIB_SHIM_H__
#define __CAVS_LIB_SHIM_H__

#ifndef ASSEMBLY

#include <sof/lib/memory.h>
#include <stdint.h>

static inline uint16_t shim_read16(uint32_t reg)
{
	return *((volatile uint16_t*)(SHIM_BASE + reg));
}

static inline void shim_write16(uint32_t reg, uint16_t val)
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

#if !CONFIG_SUECREEK

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

#endif /* !CONFIG_SUECREEK */

static inline uint32_t mn_reg_read(uint32_t reg, uint32_t id)
{
	return *((volatile uint32_t*)(MN_BASE + reg));
}

static inline void mn_reg_write(uint32_t reg, uint32_t id, uint32_t val)
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

#endif /* !ASSEMBLY */

#endif /* __CAVS_LIB_SHIM_H__ */

#else

#error "This file shouldn't be included from outside of platform/lib/shim.h"

#endif /* __PLATFORM_LIB_SHIM_H__ */
