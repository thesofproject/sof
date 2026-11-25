/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2023 NXP
 */

#ifdef __SOF_LIB_MEMORY_H__

#ifndef __PLATFORM_LIB_MEMORY_H__
#define __PLATFORM_LIB_MEMORY_H__

#include <rtos/cache.h>

/* seems like in xtensa architecture cached data
 * may be placed at some special addresses in the
 * SRAM?
 *
 * since we're running on A55 cores, there's no such
 * thing so all the below cache/shared data "management"
 * functions aren't necessary.
 */
#define cache_to_uncache_init(address) address

/* host and firmware use shared memory */
#define host_to_local(addr) (addr)
#define local_to_host(addr) (addr)

/* if this gets too big then it will be
 * necessary to increase the SRAM size from
 * the Zephyr DTS. This should be fine as
 * we're going to reserve 800MB for Jailhouse
 * usage.
 */
#define HEAPMEM_SIZE 0x00010000

/* SOF uses A side of the WAKEUPMIX MU */
#define MU_BASE 0x42430000ULL
#endif /* __PLATFORM_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/memory.h"

#endif /* __SOF_LIB_MEMORY_H__*/
