/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 */

/*
 * Header file for xtensa specific defs for GDB.
 */

#ifndef __ARCH_DEBUG_GDB_XTENSA_DEFS_H__
#define __ARCH_DEBUG_GDB_XTENSA_DEFS_H__

#include <xtensa/config/core-isa.h>
#include <xtensa/specreg.h>

#define _AREG0			256

#define STACK_SIZE		1024
#define DEBUG_PC		(EPC + XCHAL_DEBUGLEVEL)
#define DEBUG_EXCSAVE		(EXCSAVE + XCHAL_DEBUGLEVEL)
#define DEBUG_PS		(EPS + XCHAL_DEBUGLEVEL)
#define DEBUG_WINDOWBASE	WINDOWBASE
#define DEBUG_NUM_IBREAK	XCHAL_NUM_IBREAK
#define DEBUG_IBREAKENABLE	IBREAKENABLE
#define DEBUG_IBREAKA		IBREAKA
#define DEBUG_INTENABLE		INTENABLE
#define DEBUG_NUM_AREGS		XCHAL_NUM_AREGS

#endif /* __ARCH_DEBUG_GDB_XTENSA_DEFS_H__ */
