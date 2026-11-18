/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#ifndef __PLATFORM_LIB_MEMORY_H__
#define __PLATFORM_LIB_MEMORY_H__

#include <stdint.h>
#include <stddef.h>

#define SRAM_BANK_SIZE 0x10000
#define EPROMS_START 0

#if defined(CONFIG_PLATFORM_NRF54L15)
#define HEAP_SYSTEM_SIZE 0x3000
#define HEAP_RUNTIME_SIZE 0x3000
#define HEAP_BUFFER_SIZE 0x10000
#else
#define HEAP_SYSTEM_SIZE 0x4000
#define HEAP_RUNTIME_SIZE 0x4000
#define HEAP_BUFFER_SIZE 0x20000
#endif

#ifndef PLATFORM_DCACHE_ALIGN
#define PLATFORM_DCACHE_ALIGN 32
#endif
#ifndef PLATFORM_DCACHE_ALIGN_SHIFT
#define PLATFORM_DCACHE_ALIGN_SHIFT 5
#endif

#ifndef uncache_to_cache
#define uncache_to_cache(address) (address)
#endif
#ifndef cache_to_uncache
#define cache_to_uncache(address) (address)
#endif
#ifndef is_uncached
#define is_uncached(address) 0
#endif
#define host_to_local(addr) (addr)
#define local_to_host(addr) (addr)

#endif /* __PLATFORM_LIB_MEMORY_H__ */
