/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/**
 * \file include/ipc/trace.h
 * \brief Host-DSP IPC trace DMA configuration, filtering, and panic descriptors.
 * \ingroup trace_logging_api
 */

#ifndef __IPC_TRACE_H__
#define __IPC_TRACE_H__

#include <ipc/header.h>
#include <ipc/stream.h>
#include <stdint.h>

/** Maximum filename string length embedded in panic reports */
#define SOF_TRACE_FILENAME_SIZE		32

/**
 * \brief Legacy host DMA trace parameters payload (SOF_IPC_TRACE_DMA_PARAMS).
 * \ingroup trace_logging_api
 * @deprecated Use sof_ipc_dma_trace_params_ext instead.
 */
struct sof_ipc_dma_trace_params {
	struct sof_ipc_cmd_hdr hdr;		/**< IPC command header */
	struct sof_ipc_host_buffer buffer;	/**< Host-allocated circular buffer descriptor */
	uint32_t stream_tag;			/**< Assigned host DMA stream tag */
} __attribute__((packed, aligned(4)));

/**
 * \brief Extended host DMA trace parameters payload (SOF_IPC_TRACE_DMA_PARAMS_EXT).
 * \ingroup trace_logging_api
 */
struct sof_ipc_dma_trace_params_ext {
	struct sof_ipc_cmd_hdr hdr;		/**< IPC command header */
	struct sof_ipc_host_buffer buffer;	/**< Host-allocated circular buffer descriptor */
	uint32_t stream_tag;			/**< Assigned host DMA stream tag */
	uint64_t timestamp_ns;			/**< Host wallclock timestamp at synchronization point in ns */
	uint32_t reserved[8];			/**< Reserved for future expansion */
} __attribute__((packed, aligned(4)));

/**
 * \brief DMA trace circular buffer position report sent to host (SOF_IPC_TRACE_DMA_POSITION).
 * \ingroup trace_logging_api
 */
struct sof_ipc_dma_trace_posn {
	struct sof_ipc_reply rhdr;		/**< IPC reply header */
	uint32_t host_offset;			/**< Current write offset in host DMA buffer in bytes */
	uint32_t overflow;			/**< Cumulative count of dropped bytes due to buffer overflow */
	uint32_t messages;			/**< Total trace messages processed since boot */
} __attribute__((packed, aligned(4)));

/* Values used in sof_ipc_trace_filter_elem */

/* bits 6..0 */
#define SOF_IPC_TRACE_FILTER_ELEM_SET_LEVEL	0x01	/**< trace level for selected components */
#define SOF_IPC_TRACE_FILTER_ELEM_BY_UUID	0x02	/**< filter by uuid key */
#define SOF_IPC_TRACE_FILTER_ELEM_BY_COMP	0x03	/**< filter by component id */
#define SOF_IPC_TRACE_FILTER_ELEM_BY_PIPE	0x04	/**< filter by pipeline */
#define SOF_IPC_TRACE_FILTER_ELEM_TYPE_MASK	0x7F	/**< filter element type mask */

/* bit 7 */
#define SOF_IPC_TRACE_FILTER_ELEM_FIN	0x80	/**< mark last filter in set */

/* bits 31..8: Unused */

/** part of sof_ipc_trace_filter, ABI3.17 */
struct sof_ipc_trace_filter_elem {
	int32_t key;		/**< SOF_IPC_TRACE_FILTER_ELEM_ {LEVEL, UUID, COMP, PIPE} */
	int32_t value;		/**< element value */
} __attribute__((packed, aligned(4)));

/** Runtime tracing filtration data - SOF_IPC_TRACE_FILTER_UPDATE, ABI3.17 */
struct sof_ipc_trace_filter {
	struct sof_ipc_cmd_hdr hdr;	/**< IPC command header */
	uint32_t elem_cnt;		/**< number of entries in elems[] array */
	uint32_t reserved[8];		/**< reserved for future usage */
	/** variable size array with new filtering settings */
	struct sof_ipc_trace_filter_elem elems[];
} __attribute__((packed, aligned(4)));

/*
 * Commom debug
 */

/** \name SOF Panic Codes
 *  Firmware panic identifiers written to mailboxes or reported upon crash.
 *  @{
 */
#define SOF_IPC_PANIC_MAGIC			0x0dead000	/**< Magic pattern identifying valid panic packet */
#define SOF_IPC_PANIC_MAGIC_MASK		0x0ffff000	/**< Mask isolating panic magic pattern */
#define SOF_IPC_PANIC_CODE_MASK			0x00000fff	/**< Mask isolating specific panic category */

#define SOF_IPC_PANIC_MEM			(SOF_IPC_PANIC_MAGIC | 0x0)	/**< Memory allocation exhaustion panic */
#define SOF_IPC_PANIC_WORK			(SOF_IPC_PANIC_MAGIC | 0x1)	/**< Scheduling task failure panic */
#define SOF_IPC_PANIC_IPC			(SOF_IPC_PANIC_MAGIC | 0x2)	/**< Host IPC transport failure panic */
#define SOF_IPC_PANIC_ARCH			(SOF_IPC_PANIC_MAGIC | 0x3)	/**< Architecture exception panic */
#define SOF_IPC_PANIC_PLATFORM			(SOF_IPC_PANIC_MAGIC | 0x4)	/**< Platform device driver panic */
#define SOF_IPC_PANIC_TASK			(SOF_IPC_PANIC_MAGIC | 0x5)	/**< Real-time task deadline panic */
#define SOF_IPC_PANIC_EXCEPTION			(SOF_IPC_PANIC_MAGIC | 0x6)	/**< Unhandled CPU exception panic */
#define SOF_IPC_PANIC_DEADLOCK			(SOF_IPC_PANIC_MAGIC | 0x7)	/**< Multicore spinlock deadlock panic */
#define SOF_IPC_PANIC_STACK			(SOF_IPC_PANIC_MAGIC | 0x8)	/**< Stack guard overflow panic */
#define SOF_IPC_PANIC_IDLE			(SOF_IPC_PANIC_MAGIC | 0x9)	/**< Idle power state transition panic */
#define SOF_IPC_PANIC_WFI			(SOF_IPC_PANIC_MAGIC | 0xa)	/**< Wait-For-Interrupt lockup panic */
#define SOF_IPC_PANIC_ASSERT			(SOF_IPC_PANIC_MAGIC | 0xb)	/**< Assertion failure panic */
/** @} */

/**
 * \brief DSP panic crash dump descriptor reported to host.
 * \ingroup trace_logging_api
 */
struct sof_ipc_panic_info {
	struct sof_ipc_hdr hdr;			/**< IPC base header */
	uint32_t code;				/**< Panic classification code (SOF_IPC_PANIC_*) */
	char filename[SOF_TRACE_FILENAME_SIZE];	/**< Source filename where panic occurred */
	uint32_t linenum;			/**< Source line number where panic occurred */
} __attribute__((packed, aligned(4)));

#endif /* __IPC_TRACE_H__ */
