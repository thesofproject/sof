/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Espressif Systems / SOF Project
 */

#ifdef __SOF_PLATFORM_H__

#ifndef __PLATFORM_PLATFORM_H__
#define __PLATFORM_PLATFORM_H__

#define PLATFORM_DEFAULT_CLOCK CLK_CPU(0)

#define HOST_PAGE_SIZE 4096
#define PLATFORM_PAGE_TABLE_SIZE 256

#define PLATFORM_MAX_CHANNELS 8
#define PLATFORM_MAX_STREAMS 8

#endif /* __PLATFORM_PLATFORM_H__ */

#else

#error "This file shouldn't be included from outside of sof/platform.h"

#endif /* __SOF_PLATFORM_H__ */
