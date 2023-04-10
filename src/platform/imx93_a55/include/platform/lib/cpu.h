/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2023 NXP
 */

#ifdef __SOF_LIB_CPU_H__

#ifndef __PLATFORM_LIB_CPU_H__
#define __PLATFORM_LIB_CPU_H__

/* note: although the core assigned to the inmate
 * might not have the ID = 0 according to Linux,
 * Zephyr will still index its cores starting from 0.
 */
#define PLATFORM_PRIMARY_CORE_ID 0

#endif /* __PLATFORM_LIB_CPU_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/cpu.h"

#endif /* __SOF_LIB_CPU_H__ */
