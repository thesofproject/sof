/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Espressif Systems / SOF Project
 */

#ifndef __PLATFORM_LIB_MEMORY_H__
#define __PLATFORM_LIB_MEMORY_H__

#include <stdint.h>
#include <stddef.h>
#include <rtos/cache.h>

#define SRAM_BANK_SIZE 0x10000
#define EPROMS_START 0
#define HEAP_SYSTEM_SIZE 0x8000
#define HEAP_RUNTIME_SIZE 0x8000
#define HEAP_BUFFER_SIZE 0x10000

#define host_to_local(addr) (addr)
#define local_to_host(addr) (addr)

#endif /* __PLATFORM_LIB_MEMORY_H__ */
