/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

/**
 * \file platform/lib/mailbox_layout.h
 * \brief Layout of the host memory windows used by the mailbox
 *
 * User-space code can include this, through sof/lib/mailbox.h. The rest
 * of the platform memory map is in platform/lib/memory.h.
 */

#ifndef __PLATFORM_LIB_MAILBOX_LAYOUT_H__
#define __PLATFORM_LIB_MAILBOX_LAYOUT_H__

#include <zephyr/devicetree.h>
#include <mem_window.h>
#include <stdint.h>

/* HP SRAM windows */
#define WIN_BASE(n)			DT_REG_ADDR(DT_PHANDLE(MEM_WINDOW_NODE(n), memory))

/* window 0 */
#define SRAM_SW_REG_BASE		((uint32_t)(WIN_BASE(0) + WIN0_OFFSET))
#define SRAM_SW_REG_SIZE		0x1000

#define SRAM_OUTBOX_BASE		(SRAM_SW_REG_BASE + SRAM_SW_REG_SIZE)
#define SRAM_OUTBOX_SIZE		0x1000

/* window 1 */
#define SRAM_INBOX_BASE			((uint32_t)(WIN_BASE(1) + WIN1_OFFSET))
#define SRAM_INBOX_SIZE			((uint32_t)WIN_SIZE(1))

/* window 2 */
#define SRAM_DEBUG_BASE			((uint32_t)(WIN_BASE(2) + WIN2_OFFSET))
#define SRAM_DEBUG_SIZE			0x800

#define SRAM_EXCEPT_BASE		(SRAM_DEBUG_BASE + SRAM_DEBUG_SIZE)
#define SRAM_EXCEPT_SIZE		0x800

#define SRAM_STREAM_BASE		(SRAM_EXCEPT_BASE + SRAM_EXCEPT_SIZE)
#define SRAM_STREAM_SIZE		0x1000

#endif /* __PLATFORM_LIB_MAILBOX_LAYOUT_H__ */
