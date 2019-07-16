/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_LIB_MEMORY_H__

#ifndef __PLATFORM_LIB_MEMORY_H__
#define __PLATFORM_LIB_MEMORY_H__

#define HEAP_BUFFER_SIZE	(1024 * 128)
#define SOF_STACK_SIZE		0x1000

#define MAILBOX_DSPBOX_BASE	0
#define MAILBOX_DSPBOX_SIZE	0x400
#define MAILBOX_HOSTBOX_BASE	0
#define MAILBOX_HOSTBOX_SIZE	0x400
#define MAILBOX_BASE		0
#define MAILBOX_BASE_SIZE	0x400

#define PLATFORM_HEAP_SYSTEM		1
#define PLATFORM_HEAP_SYSTEM_RUNTIME	1
#define PLATFORM_HEAP_RUNTIME		1
#define PLATFORM_HEAP_BUFFER		3

#endif /* __PLATFORM_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/memory.h"

#endif /* __SOF_LIB_MEMORY_H__ */
