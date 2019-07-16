/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 */

#ifndef __ARCH_DEBUG_OFFSET_DEFS_H__
#define __ARCH_DEBUG_OFFSET_DEFS_H__

#include <xtensa/config/core-isa.h>

#define REG_OFFSET_EXCCAUSE     0x0
#define REG_OFFSET_EXCVADDR     0x4
#define REG_OFFSET_PS           0x8
#define REG_OFFSET_EPC1         0xc
#define REG_OFFSET_EPC2         0x10
#define REG_OFFSET_EPC3         0x14
#define REG_OFFSET_EPC4         0x18
#define REG_OFFSET_EPC5         0x1c
#define REG_OFFSET_EPC6         0x20
#define REG_OFFSET_EPC7         0x24
#define REG_OFFSET_EPS2         0x28
#define REG_OFFSET_EPS3         0x2c
#define REG_OFFSET_EPS4         0x30
#define REG_OFFSET_EPS5         0x34
#define REG_OFFSET_EPS6         0x38
#define REG_OFFSET_EPS7         0x3c
#define REG_OFFSET_DEPC         0x40
#define REG_OFFSET_INTENABLE    0x44
#define REG_OFFSET_INTERRUPT    0x48
#define REG_OFFSET_SAR          0x4c
#define REG_OFFSET_DEBUGCAUSE   0x50
#define REG_OFFSET_WINDOWBASE   0x54
#define REG_OFFSET_WINDOWSTART  0x58
#define REG_OFFSET_EXCSAVE1     0x5c
#define REG_OFFSET_AR_BEGIN     0x60
#define REG_OFFSET_AR_END       (REG_OFFSET_AR_BEGIN + 4 * XCHAL_NUM_AREGS)

#endif /* __ARCH_DEBUG_OFFSET_DEFS_H__ */
