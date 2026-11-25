/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 */

#ifdef __SOF_LIB_MEMORY_H__

#ifndef __PLATFORM_LIB_MEMORY_H__
#define __PLATFORM_LIB_MEMORY_H__

/* prioritize definitions in Zephyr SoC layer */
#ifdef __ZEPHYR__
#include <adsp_memory.h>
#endif

#include <ace/lib/memory.h>
#include <mem_window.h>
#include <sof/lib/cpu.h>

#include <platform/lib/mailbox_layout.h>

/* Stack configuration */
#define SOF_STACK_SIZE			0x1000

/**
 * size of HPSRAM system heap
 */
#define HEAPMEM_SIZE CONFIG_SOF_ZEPHYR_HEAP_SIZE

#endif /* __PLATFORM_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/memory.h"

#endif /* __SOF_LIB_MEMORY_H__ */
