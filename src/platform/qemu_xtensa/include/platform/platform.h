/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 */

#ifndef __PLATFORM_PLATFORM_H__
#define __PLATFORM_PLATFORM_H__

/* Dummy platform header for qemu_xtensa */
#define PLATFORM_CORE_COUNT 1
#define PLATFORM_MAX_CHANNELS 8
#define PLATFORM_MAX_STREAMS 8

#define HW_CFG_VERSION 0x010000
#define DMA_TRACE_LOCAL_SIZE HOST_PAGE_SIZE

struct ipc_msg;
static inline void ipc_platform_send_msg_direct(const struct ipc_msg *msg) {}

#endif /* __PLATFORM_PLATFORM_H__ */
