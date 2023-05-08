/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

/**
 * \file ace/lib/cpu.h
 * \brief DSP parameters, common for cAVS platforms.
 */

#ifdef __PLATFORM_LIB_CPU_H__

#ifndef __ACE_LIB_CPU_H__
#define __ACE_LIB_CPU_H__

/** \brief Id of primary DSP core */
#define PLATFORM_PRIMARY_CORE_ID	0

/** \brief Maximum CPC used y this platform with IPC4 module */
#define PLATFORM_IPC4_MAX_CPC	400000	/* based on max 400M clk and 1ms period */

#endif /* __ACE_LIB_CPU_H__ */

#else

#error "This file shouldn't be included from outside of platform/lib/cpu.h"

#endif /* __PLATFORM_LIB_CPU_H__ */
