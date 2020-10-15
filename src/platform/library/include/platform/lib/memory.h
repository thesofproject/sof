/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_LIB_MEMORY_H__

#ifndef __PLATFORM_LIB_MEMORY_H__
#define __PLATFORM_LIB_MEMORY_H__

#include <inttypes.h>

#define PLATFORM_DCACHE_ALIGN	sizeof(void *)

#define HEAP_BUFFER_SIZE	(1024 * 128)
#define SOF_STACK_SIZE		0x1000

uint8_t *get_library_mailbox(void);

#define MAILBOX_BASE	get_library_mailbox()

#define PLATFORM_HEAP_SYSTEM		1
#define PLATFORM_HEAP_SYSTEM_RUNTIME	1
#define PLATFORM_HEAP_RUNTIME		1
#define PLATFORM_HEAP_BUFFER		3

#define SHARED_DATA

static inline void *platform_shared_get(void *ptr, int bytes)
{
	return ptr;
}

static inline void platform_shared_commit(void *ptr, int bytes) { }

#endif /* __PLATFORM_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/memory.h"

#endif /* __SOF_LIB_MEMORY_H__ */
