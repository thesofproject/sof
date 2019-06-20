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

#ifndef __INCLUDE_UAPI_IPC_HEADER_H__
#define __INCLUDE_UAPI_IPC_HEADER_H__

#include <kernel/abi.h>

/** \addtogroup sof_uapi uAPI
 *  SOF uAPI specification.
 *
 * IPC messages have a prefixed 32 bit identifier made up as follows :-
 *
 * 0xGCCCNNNN where
 * - G is global cmd type (4 bits)
 * - C is command type (12 bits)
 * - N is the ID number (16 bits) - monotonic and overflows
 *
 * This is sent at the start of the IPM message in the mailbox. Messages should
 * not be sent in the doorbell (special exceptions for firmware).

 *  @{
 */

/** \name Global Message - Generic
 *  @{
 */

/** Shift-left bits to extract the global cmd type */
#define SOF_GLB_TYPE_SHIFT			28
#define SOF_GLB_TYPE_MASK			(0xf << SOF_GLB_TYPE_SHIFT)
#define SOF_GLB_TYPE(x)				((x) << SOF_GLB_TYPE_SHIFT)

/** @} */

/** \name Command Message - Generic
 *  @{
 */

/** Shift-left bits to extract the command type */
#define SOF_CMD_TYPE_SHIFT			16
#define SOF_CMD_TYPE_MASK			(0xfff << SOF_CMD_TYPE_SHIFT)
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
} __attribute__((packed));

/**
 * Command Header - Header for all IPC commands. Identifies IPC message.
 * The size can be greater than the structure size and that means there is
 * extended bespoke data beyond the end of the structure including variable
 * arrays.
 */
struct sof_ipc_cmd_hdr {
	uint32_t size;			/**< size of structure */
	uint32_t cmd;			/**< SOF_IPC_GLB_ + cmd */
} __attribute__((packed));

/**
 * Generic reply message. Some commands override this with their own reply
 * types that must include this at start.
 */
struct sof_ipc_reply {
	struct sof_ipc_cmd_hdr hdr;
	int32_t error;			/**< negative error numbers */
} __attribute__((packed));

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
} __attribute__((packed));

/**
 * OOPS header architecture specific data.
 */
struct sof_ipc_dsp_oops_arch_hdr {
	uint32_t arch;		/* Identifier of architecture */
	uint32_t totalsize;	/* Total size of oops message */
} __attribute__((packed));

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
} __attribute__((packed));

/** @}*/

#endif
