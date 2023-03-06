/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/**
 * \file include/ipc/header.h
 * \brief IPC command header
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 * \author Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __IPC_HEADER_H__
#define __IPC_HEADER_H__

#include <stdint.h>

/**
 * Common IPC logic uses standard types for abstract IPC features. This means all ABI MAJOR
 * abstraction is done in the IPC layer only and not in the surrounding infrastructure.
 */
#include <../ipc4/header.h>

/**
 * Generic IPC Header structure - Header for all IPC structures which provides
 * abstraction of IPC header implementation for different IPC ABI MAJOR types.
 */
struct ipc_cmd_hdr;

#define ipc_to_hdr(x) ((struct ipc_cmd_hdr *)x)

/** \addtogroup sof_uapi uAPI
 * SOF uAPI specification
 *
 * **Overview**
 *
 * The SOF Audio DSP firmware defines an Inter-Process Communication
 * (IPC) interface to facilitate communication with the host.
 *
 * The SOF IPC is bi-directional. Messages can be initiated by the
 * host and acknowledged by the DSP. Similarly, they can be initiated
 * by the DSP and acknowledged by the host.
 *
 * IPC messages are divided into several groups: global, topology,
 * power management, component, stream, DAI, trace, and a separate
 * "firmware ready" message. Multiple messages can also be grouped
 * into a message that belong to a compound group. Most messages are
 * sent by the host to the DSP; only the following messages are sent
 * by the DSP to the host:
 *  - firmware ready: sent only once during initialization
 *  - trace: optional, contains firmware trace data
 *  - position update: only used if position data cannot be
 *    transferred in a memory window or if forced by the kernel
 *    configuration
 *
 * **Message encoding**
 *
 * All multi-byte protocol fields are encoded with little-endian
 * byte-order.
 *
 * **Message structure**
 *
 * IPC messages have a fixed header and variable length payload.
 *
 *      0                   1                   2                   3
 *      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     | Size                                                          |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     | Command                                                       |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * The header contains size of the IPC message and a 32bit Command
 * identifier with following structure:
 *
 * 0xGCCCNNNN is little-endian value where
 * - G is the Global Type (4 bits)
 * - C is the Command Type (12 bits)
 * - N is the ID Number (16 bits) - monotonic and overflows
 *
 * The Global and Command Types together define the structure of the
 * payload. E.g. for topology IPC message COMP_NEW structure is (ABI
 * 3.17.0 example):
 *
 *      0                   1                   2                   3
 *      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     | Size (Total size including Extended Data)                     |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     | Command (G=TPLG_MSG, C=COMP_NEW)                              |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     | Component ID                                                  |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     | Type                                                          |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     | Pipeline ID                                                   |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     | Core                                                          |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     | Extended Data size                                            |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     | Type specific payload, size varies based on Component ID.     |
 *     | Ends at: Total size - Extended Data size                      |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     | extended data, including e.g. UUID                            |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * \see sof_ipc_comp
 *
 * **Reply Messages**
 *
 * Reply messages are defined per Command. The response IPC messages
 * have a common layout, but some Commands have extended fields
 * defined. The common format is:
 *
 *      0                   1                   2                   3
 *      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     | Size                                                          |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     | Command (G=REPLY, C=0)                                        |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     | Error                                                         |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * \see sof_ipc_reply
 *
 * **IPC ABI version compatibility rules**
 *
 * 1. FW binaries will only support one MAJOR ABI version which is advertised
 *    to host at FW boot.
 *
 * 2. Host drivers will support the current and older MAJOR ABI versions of
 *    the IPC ABI (up to a certain age to be determined by market information).
 *
 * 3. MINOR and PATCH ABI versions can differ between host and FW but must be
 *    backwards compatible on both host and FW.
 *
 *    IPC messages sizes can be different for sender and receiver if MINOR or
 *    PATCH ABI versions differ as new fields can be added to the end of
 *    messages.
 *
 *    i) SenderVersion > ReceiverVersion:
 *           Receiver only copies its own ABI structure size.
 *
 *    ii) ReceiverVersion > SenderVersion:
 *           Receiver copies its own ABI size and zero pads
 *           new fields. i.e. new structure fields must be non
 *           zero to be activated.
 *
 * Guidelines for extending ABI compatible messages :-
 *
 * - i) Use reserved fields.
 *
 * - ii) Grow structure at the end.
 *
 * - iii) Iff (i) and (ii) are not possible then MAJOR ABI is bumped.
 *
 *  @{
 */

/** \name Global Message - Generic
 *  @{
 */

/** Shift-left bits to extract the global cmd type */
#define SOF_GLB_TYPE_SHIFT			28
#define SOF_GLB_TYPE_MASK			(0xfUL << SOF_GLB_TYPE_SHIFT)
#define SOF_GLB_TYPE(x)				((x) << SOF_GLB_TYPE_SHIFT)

/** @} */

/** \name Command Message - Generic
 *  @{
 */

/** Shift-left bits to extract the command type */
#define SOF_CMD_TYPE_SHIFT			16
#define SOF_CMD_TYPE_MASK			(0xfffL << SOF_CMD_TYPE_SHIFT)
#define SOF_CMD_TYPE(x)				((x) << SOF_CMD_TYPE_SHIFT)

/** @} */

/** \name Global Message Types
 *  @{
 */

#define SOF_IPC_GLB_REPLY			SOF_GLB_TYPE(0x1U)
#define SOF_IPC_GLB_COMPOUND			SOF_GLB_TYPE(0x2U)
#define SOF_IPC_GLB_TPLG_MSG			SOF_GLB_TYPE(0x3U)
#define SOF_IPC_GLB_PM_MSG			SOF_GLB_TYPE(0x4U)
#define SOF_IPC_GLB_COMP_MSG			SOF_GLB_TYPE(0x5U)
#define SOF_IPC_GLB_STREAM_MSG			SOF_GLB_TYPE(0x6U)
#define SOF_IPC_FW_READY			SOF_GLB_TYPE(0x7U)
#define SOF_IPC_GLB_DAI_MSG			SOF_GLB_TYPE(0x8U)
#define SOF_IPC_GLB_TRACE_MSG			SOF_GLB_TYPE(0x9U)
#define SOF_IPC_GLB_GDB_DEBUG                   SOF_GLB_TYPE(0xAU)
#define SOF_IPC_GLB_TEST			SOF_GLB_TYPE(0xBU)
#define SOF_IPC_GLB_PROBE			SOF_GLB_TYPE(0xCU)
#define SOF_IPC_GLB_DEBUG			SOF_GLB_TYPE(0xDU)

/** @} */

/** \name DSP Command: Topology
 *  \anchor tplg_cmd_type
 *  @{
 */

#define SOF_IPC_TPLG_COMP_NEW			SOF_CMD_TYPE(0x001)
#define SOF_IPC_TPLG_COMP_FREE			SOF_CMD_TYPE(0x002)
#define SOF_IPC_TPLG_COMP_CONNECT		SOF_CMD_TYPE(0x003)
#define SOF_IPC_TPLG_PIPE_NEW			SOF_CMD_TYPE(0x010)
#define SOF_IPC_TPLG_PIPE_FREE			SOF_CMD_TYPE(0x011)
#define SOF_IPC_TPLG_PIPE_CONNECT		SOF_CMD_TYPE(0x012)
#define SOF_IPC_TPLG_PIPE_COMPLETE		SOF_CMD_TYPE(0x013)
#define SOF_IPC_TPLG_BUFFER_NEW			SOF_CMD_TYPE(0x020)
#define SOF_IPC_TPLG_BUFFER_FREE		SOF_CMD_TYPE(0x021)

/** @} */

/** \name DSP Command: PM
 *  @{
 */

#define SOF_IPC_PM_CTX_SAVE			SOF_CMD_TYPE(0x001)
#define SOF_IPC_PM_CTX_RESTORE			SOF_CMD_TYPE(0x002)
#define SOF_IPC_PM_CTX_SIZE			SOF_CMD_TYPE(0x003)
#define SOF_IPC_PM_CLK_SET			SOF_CMD_TYPE(0x004)
#define SOF_IPC_PM_CLK_GET			SOF_CMD_TYPE(0x005)
#define SOF_IPC_PM_CLK_REQ			SOF_CMD_TYPE(0x006)
#define SOF_IPC_PM_CORE_ENABLE			SOF_CMD_TYPE(0x007)
#define SOF_IPC_PM_GATE				SOF_CMD_TYPE(0x008)

/** @} */

/** \name DSP Command: Component runtime config - multiple different types
 *  @{
 */

#define SOF_IPC_COMP_SET_VALUE			SOF_CMD_TYPE(0x001)
#define SOF_IPC_COMP_GET_VALUE			SOF_CMD_TYPE(0x002)
#define SOF_IPC_COMP_SET_DATA			SOF_CMD_TYPE(0x003)
#define SOF_IPC_COMP_GET_DATA			SOF_CMD_TYPE(0x004)
#define SOF_IPC_COMP_NOTIFICATION		SOF_CMD_TYPE(0x005)

/** @} */

/** \name DSP Command: DAI messages
 *  @{
 */
#define SOF_IPC_DAI_CONFIG			SOF_CMD_TYPE(0x001)
#define SOF_IPC_DAI_LOOPBACK			SOF_CMD_TYPE(0x002)

/** @} */

/** \name DSP Command: Stream
 *  @{
 */
#define SOF_IPC_STREAM_PCM_PARAMS		SOF_CMD_TYPE(0x001)
#define SOF_IPC_STREAM_PCM_PARAMS_REPLY		SOF_CMD_TYPE(0x002)
#define SOF_IPC_STREAM_PCM_FREE			SOF_CMD_TYPE(0x003)
#define SOF_IPC_STREAM_TRIG_START		SOF_CMD_TYPE(0x004)
#define SOF_IPC_STREAM_TRIG_STOP		SOF_CMD_TYPE(0x005)
#define SOF_IPC_STREAM_TRIG_PAUSE		SOF_CMD_TYPE(0x006)
#define SOF_IPC_STREAM_TRIG_RELEASE		SOF_CMD_TYPE(0x007)
#define SOF_IPC_STREAM_TRIG_DRAIN		SOF_CMD_TYPE(0x008)
#define SOF_IPC_STREAM_TRIG_XRUN		SOF_CMD_TYPE(0x009)
#define SOF_IPC_STREAM_POSITION			SOF_CMD_TYPE(0x00a)
#define SOF_IPC_STREAM_VORBIS_PARAMS		SOF_CMD_TYPE(0x010)
#define SOF_IPC_STREAM_VORBIS_FREE		SOF_CMD_TYPE(0x011)

/** @} */

/** \name DSP Command: Trace and debug
 *  @{
 */

#define SOF_IPC_TRACE_DMA_PARAMS		SOF_CMD_TYPE(0x001)
#define SOF_IPC_TRACE_DMA_POSITION		SOF_CMD_TYPE(0x002)
#define SOF_IPC_TRACE_DMA_PARAMS_EXT		SOF_CMD_TYPE(0x003)
#define SOF_IPC_TRACE_FILTER_UPDATE		SOF_CMD_TYPE(0x004) /**< ABI3.17 */
#define SOF_IPC_TRACE_DMA_FREE		SOF_CMD_TYPE(0x005) /**< ABI3.20 */

/** @} */

/** \name DSP Command: Probes
 *  @{
 */

#define SOF_IPC_PROBE_INIT			SOF_CMD_TYPE(0x001)
#define SOF_IPC_PROBE_DEINIT			SOF_CMD_TYPE(0x002)
#define SOF_IPC_PROBE_DMA_ADD			SOF_CMD_TYPE(0x003)
#define SOF_IPC_PROBE_DMA_INFO			SOF_CMD_TYPE(0x004)
#define SOF_IPC_PROBE_DMA_REMOVE		SOF_CMD_TYPE(0x005)
#define SOF_IPC_PROBE_POINT_ADD			SOF_CMD_TYPE(0x006)
#define SOF_IPC_PROBE_POINT_INFO		SOF_CMD_TYPE(0x007)
#define SOF_IPC_PROBE_POINT_REMOVE		SOF_CMD_TYPE(0x008)

 /** @} */

/** \name DSP Command: Debug - additional services
 *  @{
 */

#define SOF_IPC_DEBUG_MEM_USAGE			SOF_CMD_TYPE(0x001)

/** @} */

/** \name DSP Command: Test - Debug build only
 *  @{
 */

#define SOF_IPC_TEST_IPC_FLOOD			SOF_CMD_TYPE(0x001)

/** @} */

/** \name IPC Message Definitions
 * @{
 */

/** Get message component id */
#define SOF_IPC_MESSAGE_ID(x)			((x) & 0xffff)

/** Maximum message size for mailbox Tx/Rx */
#define SOF_IPC_MSG_MAX_SIZE			384

/** @} */

/**
 * Structure Header - Header for all IPC structures except command structs.
 * The size can be greater than the structure size and that means there is
 * extended bespoke data beyond the end of the structure including variable
 * arrays.
 */
struct sof_ipc_hdr {
	uint32_t size;			/**< size of structure */
} __attribute__((packed, aligned(4)));

/**
 * Command Header - Header for all IPC commands. Identifies IPC message.
 * The size can be greater than the structure size and that means there is
 * extended bespoke data beyond the end of the structure including variable
 * arrays.
 */
struct sof_ipc_cmd_hdr {
	uint32_t size;			/**< size of structure */
	uint32_t cmd;			/**< SOF_IPC_GLB_ + cmd */
} __attribute__((packed, aligned(4)));

/**
 * Generic reply message. Some commands override this with their own reply
 * types that must include this at start.
 */
struct sof_ipc_reply {
	struct sof_ipc_cmd_hdr hdr;
	int32_t error;			/**< negative error numbers */
} __attribute__((packed, aligned(4)));

/**
 * Compound commands - SOF_IPC_GLB_COMPOUND.
 *
 * Compound commands are sent to the DSP as a single IPC operation. The
 * commands are split into blocks and each block has a header. This header
 * identifies the command type and the number of commands before the next
 * header.
 */
struct sof_ipc_compound_hdr {
	struct sof_ipc_cmd_hdr hdr;
	uint32_t count;		/**< count of 0 means end of compound sequence */
} __attribute__((packed, aligned(4)));

/**
 * OOPS header architecture specific data.
 */
struct sof_ipc_dsp_oops_arch_hdr {
	uint32_t arch;		/* Identifier of architecture */
	uint32_t totalsize;	/* Total size of oops message */
} __attribute__((packed, aligned(4)));

/**
 * OOPS header platform specific data.
 */
struct sof_ipc_dsp_oops_plat_hdr {
	uint32_t configidhi;	/* ConfigID hi 32bits */
	uint32_t configidlo;	/* ConfigID lo 32bits */
	uint32_t numaregs;	/* Special regs num */
	uint32_t stackoffset;	/* Offset to stack pointer from beginning of
				 * oops message
				 */
	uint32_t stackptr;	/* Stack ptr */
} __attribute__((packed, aligned(4)));

/** @}*/

#endif /* __IPC_HEADER_H__ */
