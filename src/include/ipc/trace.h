/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/**
 * \file include/ipc/trace.h
 * \brief IPC definitions
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 * \author Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __IPC_TRACE_H__
#define __IPC_TRACE_H__

#include <ipc/header.h>
#include <ipc/stream.h>
#include <stdint.h>

/*
 * DMA for Trace
 */

#define SOF_TRACE_FILENAME_SIZE		32

/* DMA for Trace params info - SOF_IPC_DEBUG_DMA_PARAMS */
/* Deprecated - use sof_ipc_dma_trace_params_ext */
struct sof_ipc_dma_trace_params {
	struct sof_ipc_cmd_hdr hdr;
	struct sof_ipc_host_buffer buffer;
	uint32_t stream_tag;
} __attribute__((packed));

/* DMA for Trace params info - SOF_IPC_DEBUG_DMA_PARAMS_EXT */
struct sof_ipc_dma_trace_params_ext {
	struct sof_ipc_cmd_hdr hdr;
	struct sof_ipc_host_buffer buffer;
	uint32_t stream_tag;
	uint64_t timestamp_ns; /* in nanosecnd */
	uint32_t reserved[8];
} __attribute__((packed));

/* DMA for Trace params info - SOF_IPC_DEBUG_DMA_PARAMS */
struct sof_ipc_dma_trace_posn {
	struct sof_ipc_reply rhdr;
	uint32_t host_offset;	/* Offset of DMA host buffer */
	uint32_t overflow;	/* overflow bytes if any */
	uint32_t messages;	/* total trace messages */
} __attribute__((packed));

/*
 * Commom debug
 */

/*
 * SOF panic codes
 */
#define SOF_IPC_PANIC_MAGIC			0x0dead000
#define SOF_IPC_PANIC_MAGIC_MASK		0x0ffff000
#define SOF_IPC_PANIC_CODE_MASK			0x00000fff
#define SOF_IPC_PANIC_MEM			(SOF_IPC_PANIC_MAGIC | 0x0)
#define SOF_IPC_PANIC_WORK			(SOF_IPC_PANIC_MAGIC | 0x1)
#define SOF_IPC_PANIC_IPC			(SOF_IPC_PANIC_MAGIC | 0x2)
#define SOF_IPC_PANIC_ARCH			(SOF_IPC_PANIC_MAGIC | 0x3)
#define SOF_IPC_PANIC_PLATFORM			(SOF_IPC_PANIC_MAGIC | 0x4)
#define SOF_IPC_PANIC_TASK			(SOF_IPC_PANIC_MAGIC | 0x5)
#define SOF_IPC_PANIC_EXCEPTION			(SOF_IPC_PANIC_MAGIC | 0x6)
#define SOF_IPC_PANIC_DEADLOCK			(SOF_IPC_PANIC_MAGIC | 0x7)
#define SOF_IPC_PANIC_STACK			(SOF_IPC_PANIC_MAGIC | 0x8)
#define SOF_IPC_PANIC_IDLE			(SOF_IPC_PANIC_MAGIC | 0x9)
#define SOF_IPC_PANIC_WFI			(SOF_IPC_PANIC_MAGIC | 0xa)
#define SOF_IPC_PANIC_ASSERT			(SOF_IPC_PANIC_MAGIC | 0xb)

/* panic info include filename and line number
 * filename array will not include null terminator if fully filled
 */
struct sof_ipc_panic_info {
	struct sof_ipc_hdr hdr;
	uint32_t code;			/* SOF_IPC_PANIC_ */
	char filename[SOF_TRACE_FILENAME_SIZE];
	uint32_t linenum;
} __attribute__((packed));

#endif /* __IPC_TRACE_H__ */
