/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_MAILBOX__
#define __INCLUDE_MAILBOX__

#include <platform/mailbox.h>
#include <arch/cache.h>
#include <stdint.h>

/* 4k should be enough for everyone ..... */
#define IPC_MAX_MAILBOX_BYTES 0x1000

#define mailbox_get_exception_base() \
	MAILBOX_EXCEPTION_BASE

#define mailbox_get_exception_size() \
	MAILBOX_EXCEPTION_SIZE

#define mailbox_get_outbox_base() \
	MAILBOX_OUTBOX_BASE

#define mailbox_get_outbox_size() \
	MAILBOX_OUTBOX_SIZE

#define mailbox_get_inbox_base() \
	MAILBOX_INBOX_BASE

#define mailbox_get_inbox_size() \
	MAILBOX_INBOX_SIZE

#define mailbox_get_debug_base() \
	MAILBOX_DEBUG_BASE

#define mailbox_get_debug_size() \
	MAILBOX_DEBUG_SIZE

#define mailbox_outbox_write(dest, src, bytes) \
	rmemcpy((void*)(MAILBOX_OUTBOX_BASE + dest), src, bytes); \
	dcache_writeback_region((void*)(MAILBOX_OUTBOX_BASE + dest), bytes);

#define mailbox_outbox_read(dest, src, bytes) \
	dcache_invalidate_region((void*)(MAILBOX_OUTBOX_BASE + src), bytes); \
	rmemcpy(dest, (void*)(MAILBOX_OUTBOX_BASE + src), bytes);

#define mailbox_inbox_write(dest, src, bytes) \
	rmemcpy((void*)(MAILBOX_INBOX_BASE + dest), src, bytes); \
	dcache_writeback_region((void*)(MAILBOX_INBOX_BASE + dest), bytes);

#define mailbox_inbox_read(dest, src, bytes) \
	dcache_invalidate_region((void*)(MAILBOX_INBOX_BASE + src), bytes); \
	rmemcpy(dest, (void*)(MAILBOX_INBOX_BASE + src), bytes);

#endif
