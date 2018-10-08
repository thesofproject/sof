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

/*
 * The Window Region on Suecreek HPSRAM is organised like this :-
 * +--------------------------------------------------------------------------+
 * | Offset              | Region         |  Size                             |
 * +---------------------+----------------+-----------------------------------+
 * | SRAM_TRACE_BASE     | Trace Buffer   |  SRAM_TRACE_SIZE                  |
 * +---------------------+----------------+-----------------------------------+
 * | SRAM_DEBUG_BASE     | Debug data     |  SRAM_DEBUG_SIZE                  |
 * +---------------------+----------------+-----------------------------------+
 * | SRAM_INBOX_BASE     | Inbox          |  SRAM_INBOX_SIZE                  |
 * +---------------------+----------------+-----------------------------------+
 * | SRAM_OUTBOX_BASE    | Outbox         |  SRAM_MAILBOX_SIZE                |
 * +---------------------+----------------+-----------------------------------+
 */

#define MAILBOX_TRACE_SIZE	SRAM_TRACE_SIZE
#define MAILBOX_TRACE_BASE	SRAM_TRACE_BASE

#define MAILBOX_DEBUG_SIZE	SRAM_DEBUG_SIZE
#define MAILBOX_DEBUG_BASE	SRAM_DEBUG_BASE

#define MAILBOX_EXCEPTION_SIZE	SRAM_EXCEPT_SIZE
#define MAILBOX_EXCEPTION_BASE	SRAM_EXCEPT_BASE
#define MAILBOX_EXCEPTION_OFFSET  SRAM_DEBUG_SIZE

#define MAILBOX_STREAM_SIZE    SRAM_STREAM_SIZE
#define MAILBOX_STREAM_BASE    SRAM_STREAM_BASE
#define MAILBOX_STREAM_OFFSET  (SRAM_DEBUG_SIZE + SRAM_EXCEPT_SIZE)

#define MAILBOX_HOSTBOX_SIZE	SRAM_INBOX_SIZE
#define MAILBOX_HOSTBOX_BASE	SRAM_INBOX_BASE

#define MAILBOX_DSPBOX_SIZE	SRAM_OUTBOX_SIZE
#define MAILBOX_DSPBOX_BASE	SRAM_OUTBOX_BASE

#define MAILBOX_SW_REG_SIZE	0
#define MAILBOX_SW_REG_BASE	0

#endif
