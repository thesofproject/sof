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


static inline uint32_t ipc_read(uint32_t reg)
{
	return *((volatile uint32_t*)(IPC_HOST_BASE + reg));
}

static inline void ipc_write(uint32_t reg, uint32_t val)
{
	*((volatile uint32_t*)(IPC_HOST_BASE + reg)) = val;
}

#define IRQ_CPU_OFFSET	0x40

#define REG_IRQ_IL2MSD(xcpu)	(0x0 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL2MCD(xcpu)	(0x4 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL2MD(xcpu)	(0x8 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL2SD(xcpu)	(0xc + (xcpu * IRQ_CPU_OFFSET))

#define REG_IRQ_IL3MSD(xcpu)	(0x10 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL3MCD(xcpu)	(0x14 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL3MD(xcpu)	(0x18 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL3SD(xcpu)	(0x1c + (xcpu * IRQ_CPU_OFFSET))

#define REG_IRQ_IL4MSD(xcpu)	(0x20 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL4MCD(xcpu)	(0x24 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL4MD(xcpu)	(0x28 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL4SD(xcpu)	(0x2c + (xcpu * IRQ_CPU_OFFSET))

#define REG_IRQ_IL5MSD(xcpu)	(0x30 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL5MCD(xcpu)	(0x34 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL5MD(xcpu)	(0x38 + (xcpu * IRQ_CPU_OFFSET))
#define REG_IRQ_IL5SD(xcpu)	(0x3c + (xcpu * IRQ_CPU_OFFSET))

#define REG_IRQ_IL2RSD		0x100
#define REG_IRQ_IL3RSD		0x104
#define REG_IRQ_IL4RSD		0x108
#define REG_IRQ_IL5RSD		0x10c

#define REG_IRQ_LVL5_LP_GPDMA0_MASK		(0xff << 16)
#define REG_IRQ_LVL5_LP_GPDMA1_MASK		(0xff << 24)

static inline uint32_t irq_read(uint32_t reg)
{
	return *((volatile uint32_t*)(IRQ_BASE + reg));
}

static inline void irq_write(uint32_t reg, uint32_t val)
{
	*((volatile uint32_t*)(IRQ_BASE + reg)) = val;
}


static inline uint32_t shim_read(uint32_t reg)
{
	return *((volatile uint32_t*)(SHIM_BASE + reg));
}

static inline void shim_write(uint32_t reg, uint32_t val)
{
	*((volatile uint32_t*)(SHIM_BASE + reg)) = val;
}

#endif
