/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Google LLC.  All rights reserved.
 * Author: Andy Ross <andyross@google.com>
 */
#ifndef PLATFORM_HOST_PLATFORM_MEMORY_H
#define PLATFORM_HOST_PLATFORM_MEMORY_H

#include <stdint.h>

#define PLATFORM_DCACHE_ALIGN 64
#define uncache_to_cache(addr) (addr)
#define cache_to_uncache(addr) (addr)

extern uint8_t posix_hostbox[];
#define MAILBOX_HOSTBOX_SIZE 1024
#define MAILBOX_HOSTBOX_BASE (&posix_hostbox[0])

extern uint8_t posix_dspbox[];
#define MAILBOX_DSPBOX_SIZE 4096
#define MAILBOX_DSPBOX_BASE (&posix_dspbox[0])

extern uint8_t posix_stream[];
#define MAILBOX_STREAM_SIZE 4096
#define MAILBOX_STREAM_BASE (&posix_stream[0])

extern uint8_t posix_trace[];
#define MAILBOX_TRACE_BASE (&posix_trace[0])
#define MAILBOX_TRACE_SIZE 4096

#define PLATFORM_HEAP_SYSTEM 1
#define PLATFORM_HEAP_SYSTEM_RUNTIME 1
#define PLATFORM_HEAP_RUNTIME 1
#define PLATFORM_HEAP_BUFFER 1

#define host_to_local(addr) (addr)

static inline void *platform_shared_get(void *ptr, int bytes)
{
	return ptr;
}

#define SHARED_DATA /**/

#endif /* PLATFORM_HOST_PLATFORM_MEMORY_H */
