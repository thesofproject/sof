/*
 * Copyright (c) 2017, Intel Corporation
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
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
 */

#ifndef __INCLUDE_PLATFORM_MAILBOX__
#define __INCLUDE_PLATFORM_MAILBOX__

#include <platform/memory.h>

/* TODO: no SHM mailbox on SUE, must be sent via SPI */
#if 0
#define MAILBOX_HOST_OFFSET	0x144000

#define MAILBOX_OUTBOX_OFFSET	0x0
#define MAILBOX_OUTBOX_SIZE	0x400
#define MAILBOX_OUTBOX_BASE \
	(MAILBOX_BASE + MAILBOX_OUTBOX_OFFSET)

#define MAILBOX_INBOX_OFFSET	MAILBOX_OUTBOX_SIZE
#define MAILBOX_INBOX_SIZE	0x400
#define MAILBOX_INBOX_BASE \
	(MAILBOX_BASE + MAILBOX_INBOX_OFFSET)

#define MAILBOX_EXCEPTION_OFFSET \
	(MAILBOX_INBOX_SIZE + MAILBOX_OUTBOX_SIZE)
#define MAILBOX_EXCEPTION_SIZE	0x100
#define MAILBOX_EXCEPTION_BASE \
	(MAILBOX_BASE + MAILBOX_EXCEPTION_OFFSET)

#define MAILBOX_DEBUG_OFFSET \
	(MAILBOX_EXCEPTION_SIZE + MAILBOX_EXCEPTION_OFFSET)
#define MAILBOX_DEBUG_SIZE	0x100
#define MAILBOX_DEBUG_BASE \
	(MAILBOX_BASE + MAILBOX_DEBUG_OFFSET)

#define MAILBOX_STREAM_OFFSET \
	(MAILBOX_DEBUG_SIZE + MAILBOX_DEBUG_OFFSET)
#define MAILBOX_STREAM_SIZE	0x200
#define MAILBOX_STREAM_BASE \
	(MAILBOX_BASE + MAILBOX_STREAM_OFFSET)

#define MAILBOX_TRACE_OFFSET \
	(MAILBOX_STREAM_SIZE + MAILBOX_STREAM_OFFSET)
#define MAILBOX_TRACE_SIZE	0x380
#define MAILBOX_TRACE_BASE \
	(MAILBOX_BASE + MAILBOX_TRACE_OFFSET)

#endif

// TODO need added to linker map

#define MAILBOX_DEBUG_SIZE	0
#define MAILBOX_DEBUG_BASE 	0

#define MAILBOX_HOSTBOX_SIZE	0
#define MAILBOX_HOSTBOX_BASE 	0

#define MAILBOX_DSPBOX_SIZE	0
#define MAILBOX_DSPBOX_BASE 	0

#define MAILBOX_TRACE_SIZE	0
#define MAILBOX_TRACE_BASE 	0

#define MAILBOX_EXCEPTION_SIZE	0
#define MAILBOX_EXCEPTION_BASE 0

#endif
