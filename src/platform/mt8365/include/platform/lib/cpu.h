/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Andrew Perepech <andrew.perepech@mediatek.com>
 */

#ifdef __SOF_LIB_CPU_H__

#ifndef __PLATFORM_LIB_CPU_H__
#define __PLATFORM_LIB_CPU_H__

/** \brief Id of primary DSP core */
#define PLATFORM_PRIMARY_CORE_ID 0

#endif /* __PLATFORM_LIB_CPU_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/cpu.h"

#endif /* __SOF_LIB_CPU_H__ */
