/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022-2024 Intel Corporation.
 *
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 */

#ifdef __SOF_LIB_MEMORY_H__

#ifndef __PLATFORM_LIB_MEMORY_H__
#define __PLATFORM_LIB_MEMORY_H__

/* prioritize definitions in Zephyr SoC layer */
#include <adsp_memory.h>

#include <ace/lib/memory.h>
#include <mem_window.h>
#include <sof/lib/cpu.h>

#include <platform/lib/mailbox_layout.h>

/* Stack configuration */
#define SOF_STACK_SIZE			0x1000

#define PLATFORM_HEAP_SYSTEM		CONFIG_CORE_COUNT /* one per core */
#define PLATFORM_HEAP_SYSTEM_RUNTIME	CONFIG_CORE_COUNT /* one per core */
#define PLATFORM_HEAP_RUNTIME		1
#define PLATFORM_HEAP_RUNTIME_SHARED	1
#define PLATFORM_HEAP_SYSTEM_SHARED	1
#define PLATFORM_HEAP_BUFFER		2

/**
 * size of HPSRAM system heap
 */
#define HEAPMEM_SIZE CONFIG_SOF_ZEPHYR_HEAP_SIZE

#endif /* __PLATFORM_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/memory.h"

#endif /* __SOF_LIB_MEMORY_H__ */
