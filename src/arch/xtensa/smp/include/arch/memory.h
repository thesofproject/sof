/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file arch/xtensa/smp/include/arch/memory.h
 * \brief Xtensa SMP memory header file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __ARCH_MEMORY_H__
#define __ARCH_MEMORY_H__

#include <platform/cpu.h>

/** \brief Stack size. */
#define ARCH_STACK_SIZE		0x1000

/** \brief Stack total size. */
#define ARCH_STACK_TOTAL_SIZE	(PLATFORM_CORE_COUNT * ARCH_STACK_SIZE)

#endif
