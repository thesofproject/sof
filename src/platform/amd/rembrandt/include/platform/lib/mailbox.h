/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 AMD.All rights reserved.
 *
 * Author:      Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
 *              Bala Kishore <balakishore.pati@amd.com>
 */
#ifdef __SOF_LIB_MAILBOX_H__

#ifndef __PLATFORM_LIB_MAILBOX_H__
#define __PLATFORM_LIB_MAILBOX_H__

#include <sof/lib/memory.h>

#define MAILBOX_DSPBOX_SIZE		SRAM_OUTBOX_SIZE
#define MAILBOX_DSPBOX_BASE		SRAM_OUTBOX_BASE
#define MAILBOX_DSPBOX_OFFSET		SRAM_OUTBOX_OFFSET

#define MAILBOX_HOSTBOX_SIZE		SRAM_INBOX_SIZE
#define MAILBOX_HOSTBOX_BASE		SRAM_INBOX_BASE
#define MAILBOX_HOSTBOX_OFFSET		SRAM_INBOX_OFFSET

#define MAILBOX_DEBUG_SIZE		SRAM_DEBUG_SIZE
#define MAILBOX_DEBUG_BASE		SRAM_DEBUG_BASE
#define MAILBOX_DEBUG_OFFSET		SRAM_DEBUG_OFFSET

#define MAILBOX_TRACE_SIZE		SRAM_TRACE_SIZE
#define MAILBOX_TRACE_BASE		SRAM_TRACE_BASE
#define MAILBOX_TRACE_OFFSET		SRAM_TRACE_OFFSET

#define MAILBOX_EXCEPTION_SIZE		SRAM_EXCEPT_SIZE
#define MAILBOX_EXCEPTION_BASE		SRAM_EXCEPT_BASE
#define MAILBOX_EXCEPTION_OFFSET	SRAM_EXCEPT_OFFSET

#define MAILBOX_STREAM_SIZE		SRAM_STREAM_SIZE

#define MAILBOX_STREAM_BASE		SRAM_STREAM_BASE
#define MAILBOX_STREAM_OFFSET		SRAM_STREAM_OFFSET

static inline void mailbox_sw_reg_write(size_t offset, uint32_t src)
{
	volatile uint32_t *ptr;

	ptr = (volatile uint32_t *)(MAILBOX_DEBUG_BASE + offset);
	*ptr = src;
}

#endif /* __PLATFORM_LIB_MAILBOX_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/mailbox.h"

#endif /* __SOF_LIB_MAILBOX_H__ */
