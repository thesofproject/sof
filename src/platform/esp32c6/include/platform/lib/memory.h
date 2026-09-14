/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Espressif Systems / SOF Project
 */

#ifndef __PLATFORM_LIB_MEMORY_H__
#define __PLATFORM_LIB_MEMORY_H__

#include <stdint.h>
#include <stddef.h>

#define SRAM_BANK_SIZE 0x10000
#define EPROMS_START 0
#define HEAP_SYSTEM_SIZE 0x8000
#define HEAP_RUNTIME_SIZE 0x8000
#define HEAP_BUFFER_SIZE 0x10000

#define PLATFORM_DCACHE_ALIGN 64
#define PLATFORM_DCACHE_ALIGN_SHIFT 6

#define uncache_to_cache(address) (address)
#define cache_to_uncache(address) (address)
#define is_uncached(address) 0
#define host_to_local(addr) (addr)
#define local_to_host(addr) (addr)

#endif /* __PLATFORM_LIB_MEMORY_H__ */
