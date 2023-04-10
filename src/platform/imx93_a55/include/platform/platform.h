/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2023 NXP
 */

#ifdef __SOF_PLATFORM_H__

#ifndef __PLATFORM_PLATFORM_H__
#define __PLATFORM_PLATFORM_H__

#define PLATFORM_DEFAULT_CLOCK CLK_CPU(0)

/* host uses 4KB page granularity */
#define HOST_PAGE_SIZE 4096

#define PLATFORM_PAGE_TABLE_SIZE 256

/* TODO: these values are taken from i.MX8 platform */
#define PLATFORM_MAX_CHANNELS 4
#define PLATFORM_MAX_STREAMS 5

/* SOF uses A side of the WAKEUPMIX MU.
 * We need to add 32 (SPI_BASE) to the
 * INTID found in the TRM since all the
 * interrupt IDs here are SPIs.
 */
#define PLATFORM_IPC_INTERRUPT (23 + 32)

#endif /* __PLATFORM_PLATFORM_H__ */

#else

#error "This file shouldn't be included from outside of sof/platform.h"

#endif /* __SOF_PLATFORM_H__ */
