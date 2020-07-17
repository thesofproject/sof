/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 */

/**
 * \file include/ipc2/header.h
 * \brief IPC header
 */

#ifndef __IPC_2_HEADER_H__
#define __IPC_2_HEADER_H__

#include <stdint.h>

/** \addtogroup sof_uapi uAPI
 *  SOF uAPI specification.
 *
 * All standard mailbox IPC2 messages between host driver and DSP start
 * with a common IPC header structure. This structure is
 *
 * IPC2.0 message header is 1 - 5 words made up as follows :
 *
 * The IPC 2 header works at a high level on the basic principle
 * of a mandatory header metadata followed by optional message metadata.
 *
 * +------------------------+-----------+---------+---------------+
 * | struct sof_ipc2_hdr    | Mandatory | 1 word  |               |
 * +------------------------+-----------+---------+---------------+
 * | struct sof_ipc2_route  | Optional  | 2 words | hdr.route = 1 |
 * +------------------------+-----------+---------+---------------+
 * | struct sof_ipc2_size   | Optional  | 1 word  | hdr.size = 1  |
 * +------------------------+-----------+---------+---------------+
 * | struct sof_ipc2_elem   | Optional  | 1 word  | hdr.elems = 1 |
 * +------------------------+-----------+---------+---------------+
 * |                                                              |
 * | Message body follows here                                    |
 * |  1) Tuple elements                                           |
 * |  2) Private data                                             |
 * +--------------------------------------------------------------+
 *
 * The optional message metadata is ordered, i.e. it always appears in the
 * same order if used (and usage is determined by status bits in
 * struct sof_ipc2_hdr).
 *
 * The header is designed to support the following use cases.
 *
 * 1) Nano messaging via 32bit message and reply. i.e. sending header and
 *    replying with header only is enough for some use cases like starting and
 *    stopping global events.
 *
 * 2) Micro messaging via 64bit message and reply - send and reply header with
 *    micro tuple. Expands uses cases from 1) to support stopping and starting
 *    targeted events.
 *
 * 3) Variable size message and reply - like 1) and 2) but messages and replies
 *    can be variable in size from 32bits upwards. Any use case can be supported
 *    here since there are no message restrictions.
 *
 * 4) Support of legacy ABIs. The header can be prefixed to legacy ABIs by
 *    using hdr.block = 1 and appending any legacy ABI structure. This allows
 *    a stable migration path with a small additional word prefixed to
 *    legacy ABI IPCs.
 *
 * 5) High priority messaging. The header now supports a hint for incoming
 *    message Q handlers so that they can prioritise real time high priority
 *    messages over standard batch messages. e.g stream start for low latency
 *    stream could be processed in the Q before sensor config message.
 *
 * 6) Datagram mode (no reply needed). The header can tell the message receiver
 *    that the message does not need to be acknowledged with a reply. Useful
 *    where the sender may be sending high volume, short lifetime information
 *    or where the sender does not care about reply (to save cycles on both
 *    sender and receiver). Door bell protocol would still be followed.
 *
 * 7) Message addressing. 32bit sender and receiver addresses can be added in
 *    header so that messages can be more easily routed to the correct
 *    destinations. Broadcast messages also supported.
 *  @{
 */

/**
 * Structure Header - Mandatory.
 * Header metadata for all IPC commands. Identifies IPC message.
 * Class spelled with K for C++ users.
 *
 * @klass: 	Message feature class. e.g. audio, sensor, debug.
 * @subklass:	Message sub feature. e.g. PCM, kcontrol, Compressed PCM
 * @action:	Message action. e.g. Start (PCM), Get (kcontrol value)
 * @ack:	Reply - IPC success, other reply data may or may not follow.
 */
struct sof_ipc2_hdr {
	uint32_t klass : 8;		/**< class ID */
	uint32_t subklass : 8;		/**< subclass ID */
	uint32_t action : 8;		/**< action ID */
	uint32_t ack : 1;		/**< message is a reply - success */
	uint32_t nack : 1;		/**< message is a reply - fail */
	uint32_t priority : 1;		/**< 0 normal, 1 high */
	uint32_t datagram : 1;		/**< is datagram - no reply needed */
	uint32_t route : 1;		/**< routing data follows */
	uint32_t size : 1;		/**< size follows */
	uint32_t elems : 1;		/**< data elems follows */
	uint32_t block : 1;		/**< data block follows */
} __attribute__((packed));

/*
 * Structure Route - Optional
 * Header routing data for this message. Allows 1:1 and 1:N messaging.
 */

#define SOF_IPC2_ROUTE_BROADCAST	0xFFFFFFFF

struct sof_ipc2_route {
	uint32_t receiver;		/**< receiver ID */
	uint32_t sender;		/**< sender ID */
} __attribute__((packed));

/*
 * Structure Size - Optional.
 * Header containing message size
 */
struct sof_ipc2_size {
	uint32_t size;			/**< size in words */
} __attribute__((packed));

/*
 * Structure Elems - Optional.
 * Header containing number of tuple elements
 */
struct sof_ipc2_elems {
	uint32_t num_elems;		/**< number of data elems */
} __attribute__((packed));


/*
 * Convenience macros to get struct offsets from header.
 */

/* Internal macros to get header ponters. */
#define _SOF_IPC2_HDR_ROUTE_ADD(hdr)	\
	(hdr->route ? sizeof(struct sof_ipc2_route) : NULL)
#define _SOF_IPC2_HDR_SIZE_ADD(hdr) 	\
	(hdr->size ? sizeof(struct sof_ipc2_size) : NULL)
#define _SOF_IPC2_HDR_ELEM_ADD(hdr) 	\
	(hdr->elem ? sizeof(struct sof_ipc2_elem) : NULL)
#define _SOF_IPC2_HDR_PDATA_ADD(hdr) 	\
	(hdr->block ? sizeof(struct sof_ipc2_elem) : NULL)

/* get header route pointer or NULL */
#define SOF_IPC2_HDR_GET_ROUTE_PTR(hdr) 	\
	(hdr->route ? (void *)hdr +		\
		_SOF_IPC2_HDR_ROUTE_ADD(hdr) : NULL)

/* get header route pointer or NULL */
#define SOF_IPC2_HDR_GET_SIZE_PTR(hdr) 		\
	(hdr->size ? (void *)hdr +  		\
		_SOF_IPC2_HDR_SIZE_ADD(hdr) +  	\
		_SOF_IPC2_HDR_ROUTE_ADD(hdr) : NULL)

/* get header elem pointer or NULL */
#define SOF_IPC2_HDR_GET_ELEM_PTR(hdr)		\
	(hdr->elem ? (void *)hdr +		\
		_SOF_IPC2_HDR_ELEM_ADD(hdr) +	\
		_SOF_IPC2_HDR_SIZE_ADD(hdr) +  	\
		_SOF_IPC2_HDR_ROUTE_ADD(hdr) : NULL)

/* get header elem pointer or NULL */
#define SOF_IPC2_HDR_GET_PDATA_PTR(hdr) 	\
	(hdr->block ? (void *)hdr +		\
		_SOF_IPC2_HDR_PDATA_OFFSET(hdr)	\
		_SOF_IPC2_HDR_ELEM_ADD(hdr) +	\
		_SOF_IPC2_HDR_SIZE_ADD(hdr) +  	\
		_SOF_IPC2_HDR_ROUTE_ADD(hdr) : NULL)


/* get headers size */
#define SOF_IPC2_HDR_GET_HDR_SIZE(hdr) 		\
		_SOF_IPC2_HDR_PDATA_OFFSET(hdr)	\
		_SOF_IPC2_HDR_ELEM_ADD(hdr) +	\
		_SOF_IPC2_HDR_SIZE_ADD(hdr) +  	\
		_SOF_IPC2_HDR_ROUTE_ADD(hdr)

/** \name IPC class
 *  @{
 *  Top Level message class - Used to route message to correct subsystem
 */

#define SOF_IPC_CLASS_SYSTEM			0x1
#define SOF_IPC_CLASS_PM			0x2
#define SOF_IPC_CLASS_DEBUG			0x3
#define SOF_IPC_CLASS_TPLG			0x4
#define SOF_IPC_CLASS_AUDIO			0x5
#define SOF_IPC_CLASS_SENSOR			0x6
#define SOF_IPC_CLASS_SHELL			0x7

/*
 * Legacy IPC classes - reserve space at the block end.
 */
#define SOF_IPC_CLASS_PDATA_CAVS		0xf0
#define SOF_IPC_CLASS_PDATA_SOF1		0xf1


/** @} */

/** \name IPC Generic class - sub-class
 *  @{
 */

#define SOF_IPC_SYS_BOOT			0x1
#define SOF_IPC_SYS_PANIC			0x2

/** @} */

/*
 * Subclasses - Used to route message with the subsystem.
 */

/** \name IPC PM sub-class
 *  @{
 */

#define SOF_IPC_PM_CTX			0x1
#define SOF_IPC_PM_CLK			0x2
#define SOF_IPC_PM_CORE			0x3
#define SOF_IPC_PM_GATE			0x4

/** @} */

/** \name IPC DEBUG class - sub-class
 *  @{
 */
#define SOF_IPC_DEBUG_TRACE			0x1
#define SOF_IPC_DEBUG_GDB			0x2
#define SOF_IPC_DEBUG_TEST			0x3
#define SOF_IPC_DEBUG_PROBE			0x4

/** @} */

/** \name IPC TPLG class - sub-class
 *  @{
 */
#define SOF_IPC_TPLG_COMP			0x1
#define SOF_IPC_TPLG_PIPE			0x2
#define SOF_IPC_TPLG_BUFFER			0x3

/** @} */

/** \name IPC Audio class - sub-class
 *  @{
 */

#define SOF_IPC_AUDIO_COMP			0x1
#define SOF_IPC_AUDIO_STREAM			0x2
#define SOF_IPC_AUDIO_DAI			0x3

/** @} */

/* IPC Actions - each subclass has a set of IPC actions. */

/** \name System Actions
 *  @{
 */
/** SOF_IPC_REPLY actions */
#define SOF_IPC_SYS_BOOT_FAIL			0x001
#define SOF_IPC_SYS_BOOT_DONE			0x002
#define SOF_IPC_SYS_ALERT_NONFATAL		0x003
#define SOF_IPC_SYS_ALERT_FATAL			0x004

/** @} */

/** \name PM Actions
 *  @{
 */
/** SOF_IPC_PM_CTX actions */
#define SOF_IPC_PM_CTX_SAVE			0x001
#define SOF_IPC_PM_CTX_RESTORE			0x002
#define SOF_IPC_PM_CTX_SIZE			0x003
/** SOF_IPC_PM_CLK actions */
#define SOF_IPC_PM_CLK_SET			0x004
#define SOF_IPC_PM_CLK_GET			0x005
#define SOF_IPC_PM_CLK_REQ			0x006
/** SOF_IPC_PM_CORE actions */
#define SOF_IPC_PM_CORE_ENABLE			0x007
/** SOF_IPC_PM_GATE actions */
#define SOF_IPC_PM_GATE_CLK			0x008

/** @} */

/** \name DEBUG Actions
 *  @{
 */
/** SOF_IPC_DEBUG_TRACE actions */
#define SOF_IPC_DEBUG_TRACE_DMA_PARAMS			0x001
#define SOF_IPC_DEBUG_TRACE_DMA_POSITION		0x002
#define SOF_IPC_DEBUG_TRACE_DMA_PARAMS_EXT		0x003
/** SOF_IPC_DEBUG_GDB actions */
/** TODO: add SOF_IPC_DEBUG_GDB actions */
/** SOF_IPC_DEBUG_TEST actions */
#define SOF_IPC_DEBUG_TEST_IPC_FLOOD			0x001
/** SOF_IPC_DEBUG_PROBE actions */
#define SOF_IPC_DEBUG_PROBE_INIT			0x001
#define SOF_IPC_DEBUG_PROBE_DEINIT			0x002
#define SOF_IPC_DEBUG_PROBE_DMA_ADD			0x003
#define SOF_IPC_DEBUG_PROBE_DMA_INFO			0x004
#define SOF_IPC_DEBUG_PROBE_DMA_REMOVE			0x005
#define SOF_IPC_DEBUG_PROBE_POINT_ADD			0x006
#define SOF_IPC_DEBUG_PROBE_POINT_INFO			0x007
#define SOF_IPC_DEBUG_PROBE_POINT_REMOVE		0x008
/** @} */

/** \name TPLG class actions
 *  @{
 */
/** SOF_IPC_TPLG_COMP actions */
#define SOF_IPC_TPLG_COMP_NEW				0x001
#define SOF_IPC_TPLG_COMP_FREE				0x002
#define SOF_IPC_TPLG_COMP_CONNECT			0x003
/** SOF_IPC_TPLG_PIPE actions */
#define SOF_IPC_TPLG_PIPE_NEW				0x010
#define SOF_IPC_TPLG_PIPE_FREE				0x011
#define SOF_IPC_TPLG_PIPE_CONNECT			0x012
#define SOF_IPC_TPLG_PIPE_COMPLETE			0x013
/** SOF_IPC_TPLG_BUFFER actions */
#define SOF_IPC_TPLG_BUFFER_NEW				0x020
#define SOF_IPC_TPLG_BUFFER_FREE			0x021

/** @} */

/** \name Audio class actions
 *  @{
 */
/** SOF_IPC_AUDIO_COMP actions */
#define SOF_IPC_AUDIO_COMP_SET_VALUE			0x001
#define SOF_IPC_AUDIO_COMP_GET_VALUE			0x002
#define SOF_IPC_AUDIO_COMP_SET_DATA			0x003
#define SOF_IPC_AUDIO_COMP_GET_DATA			0x004
#define SOF_IPC_AUDIO_COMP_NOTIFICATION			0x005
/** SOF_IPC_AUDIO_STREAM actions */
#define SOF_IPC_AUDIO_STREAM_PCM_PARAMS			0x001
#define SOF_IPC_AUDIO_STREAM_PCM_PARAMS_REPLY		0x002
#define SOF_IPC_AUDIO_STREAM_PCM_FREE			0x003
#define SOF_IPC_AUDIO_STREAM_TRIG_START			0x004
#define SOF_IPC_AUDIO_STREAM_TRIG_STOP			0x005
#define SOF_IPC_AUDIO_STREAM_TRIG_PAUSE			0x006
#define SOF_IPC_AUDIO_STREAM_TRIG_RELEASE		0x007
#define SOF_IPC_AUDIO_STREAM_TRIG_DRAIN			0x008
#define SOF_IPC_AUDIO_STREAM_TRIG_XRUN			0x009
#define SOF_IPC_AUDIO_STREAM_POSITION			0x00a
#define SOF_IPC_AUDIO_STREAM_VORBIS_PARAMS		0x010
#define SOF_IPC_AUDIO_STREAM_VORBIS_FREE		0x011
/** SOF_IPC_AUDIO_DAI actions */
#define SOF_IPC_AUDIO_DAI_CONFIG			0x001
#define SOF_IPC_AUDIO_DAI_LOOPBACK			0x002

/** @} */

/** \name IPC Message Definitions
 * @{
 */


/** @} */

/** @}*/

#endif /* __IPC_2_HEADER_H__ */
