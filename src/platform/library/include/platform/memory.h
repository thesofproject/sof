/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __PLATFORM_MEMORY_H__
#define __PLATFORM_MEMORY_H__

#include <config.h>

#if CONFIG_HT_BAYTRAIL
#include <baytrail/include/platform/memory.h>
#endif

#define HEAP_BUFFER_SIZE	(1024 * 128)
#define SOF_STACK_SIZE		0x1000

#define MAILBOX_DSPBOX_BASE	0
#define MAILBOX_DSPBOX_SIZE	0x400
#define MAILBOX_HOSTBOX_BASE	0
#define MAILBOX_HOSTBOX_SIZE	0x400
#define MAILBOX_BASE		0
#define MAILBOX_BASE_SIZE	0x400

#endif /* __PLATFORM_MEMORY_H__ */
