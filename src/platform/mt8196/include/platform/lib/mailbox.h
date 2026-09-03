/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Hailong Fan <hailong.fan@mediatek.com>
 */

#ifdef __SOF_LIB_MAILBOX_H__

#ifndef __PLATFORM_LIB_MAILBOX_H__
#define __PLATFORM_LIB_MAILBOX_H__

#include <sof/lib/io.h>
#include <sof/lib/memory.h>
#include <stddef.h>
#include <stdint.h>

/*
 * The Window Region on MT8186 SRAM is organised like this :-
 * +--------------------------------------------------------------------------+
 * | Offset              | Region         |  Size                             |
 * +---------------------+----------------+-----------------------------------+
 * | SRAM_OUTBOX_BASE    | Outbox         |  SRAM_MAILBOX_SIZE                |
 * +---------------------+----------------+-----------------------------------+
 * | SRAM_INBOX_BASE     | Inbox          |  SRAM_INBOX_SIZE                  |
 * +---------------------+----------------+-----------------------------------+
 * | SRAM_DEBUG_BASE     | Debug data     |  SRAM_DEBUG_SIZE                  |
 * +---------------------+----------------+-----------------------------------+
 * | SRAM_EXCEPT_BASE    | Except         |  SRAM_EXCEPT_SIZE                 |
 * +---------------------+----------------+-----------------------------------+
 * | SRAM_STREAM_BASE    | Stream         |  SRAM_STREAM_SIZE                 |
 * +---------------------+----------------+-----------------------------------+
 * | SRAM_TRACE_BASE     | Trace Buffer   |  SRAM_TRACE_SIZE                  |
 * +---------------------+----------------+-----------------------------------+
 */

#define MAILBOX_DSPBOX_SIZE		SRAM_OUTBOX_SIZE
#define MAILBOX_DSPBOX_BASE		SRAM_OUTBOX_BASE
#define MAILBOX_DSPBOX_OFFSET		SRAM_OUTBOX_OFFSET

#define MAILBOX_HOSTBOX_SIZE		SRAM_INBOX_SIZE
#define MAILBOX_HOSTBOX_BASE		SRAM_INBOX_BASE
#define MAILBOX_HOSTBOX_OFFSET		SRAM_INBOX_OFFSET

#define MAILBOX_DEBUG_SIZE		SRAM_DEBUG_SIZE
#define MAILBOX_DEBUG_BASE		SRAM_DEBUG_BASE
#define MAILBOX_DEBUG_OFFSET		SRAM_DEBUG_OFFSET

#define MAILBOX_EXCEPTION_SIZE		SRAM_EXCEPT_SIZE
#define MAILBOX_EXCEPTION_BASE		SRAM_EXCEPT_BASE
#define MAILBOX_EXCEPTION_OFFSET	SRAM_EXCEPT_OFFSET

#define MAILBOX_STREAM_SIZE		SRAM_STREAM_SIZE
#define MAILBOX_STREAM_BASE		SRAM_STREAM_BASE
#define MAILBOX_STREAM_OFFSET		SRAM_STREAM_OFFSET

#define MAILBOX_TRACE_SIZE		SRAM_TRACE_SIZE
#define MAILBOX_TRACE_BASE		SRAM_TRACE_BASE
#define MAILBOX_TRACE_OFFSET		SRAM_TRACE_OFFSET

#define ADSP_IPI_OP_REQ	0x1
#define ADSP_IPI_OP_RSP	0x2
void trigger_irq_to_host_req(void);
void trigger_irq_to_host_rsp(void);

static inline void mailbox_sw_reg_write(size_t offset, uint32_t src)
{
	io_reg_write(MAILBOX_DEBUG_BASE + offset, src);
}

#endif /* __PLATFORM_LIB_MAILBOX_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/mailbox.h"

#endif /* __SOF_LIB_MAILBOX_H__ */
