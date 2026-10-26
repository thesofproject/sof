/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2026 NXP
 */

#ifdef __SOF_PLATFORM_H__

#ifndef __PLATFORM_PLATFORM_H__
#define __PLATFORM_PLATFORM_H__

/* refers to M7 core clock - one core, one clock */
#define PLATFORM_DEFAULT_CLOCK 0

#define HOST_PAGE_SIZE 4096

#define PLATFORM_PAGE_TABLE_SIZE 256

/* WAKEUP domain MU7 side B */
#define PLATFORM_IPC_INTERRUPT 97

#endif /* __PLATFORM_PLATFORM_H__ */

#else

#error "This file shouldn't be included from outside of sof/platform.h"

#endif /* __SOF_PLATFORM_H__ */
