/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2024 NXP
 */

#ifdef __SOF_PLATFORM_H__

#ifndef __PLATFORM_PLATFORM_H__
#define __PLATFORM_PLATFORM_H__

/* refers to M7 core clock - one core, one clock */
#define PLATFORM_DEFAULT_CLOCK 0

#define HOST_PAGE_SIZE 4096

#define PLATFORM_PAGE_TABLE_SIZE 256

/* TODO: generous (SOF is usually used with 2 channels at most on i.MX
 * platforms) and (potentially) not true. Can be adjusted later on if
 * need be.
 */
#define PLATFORM_MAX_CHANNELS 4
/* TODO: same as PLATFORM_MAX_CHANNELS */
#define PLATFORM_MAX_STREAMS 5

/* WAKEUP domain MU7 side B */
#define PLATFORM_IPC_INTERRUPT 207

#endif /* __PLATFORM_PLATFORM_H__ */

#else

#error "This file shouldn't be included from outside of sof/platform.h"

#endif /* __SOF_PLATFORM_H__ */
