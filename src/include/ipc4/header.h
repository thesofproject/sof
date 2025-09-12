/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 */

/*
 * This file contains structures that are exact copies of an existing ABI used
 * by IOT middleware. They are Intel specific and will be used by one middleware.
 *
 * Some of the structures may contain programming implementations that makes them
 * unsuitable for generic use and general usage.
 *
 * This code is mostly copied "as-is" from existing C++ interface files hence the use of
 * different style in places. The intention is to keep the interface as close as possible to
 * original so it's easier to track changes with IPC host code.
 */

/**
 * \file include/ipc4/header.h
 * \brief IPC4 global definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_HEADER_H__
#define __SOF_IPC4_HEADER_H__

#include <stdint.h>

#define ipc_from_hdr(x) ((struct ipc4_message_request *)x)

/**< Message target, value of msg_tgt field. */
enum ipc4_message_target {
	/**< Global FW message */
	SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG = 0,
	/**< Module message */
	SOF_IPC4_MESSAGE_TARGET_MODULE_MSG = 1
};

/**< Message direction, value of rsp field. */
enum ipc4_message_direction {
	/**< Request, Notification */
	SOF_IPC4_MESSAGE_DIR_MSG_REQUEST = 0,
	/**< Reply */
	SOF_IPC4_MESSAGE_DIR_MSG_REPLY = 1,
};

/*
 * Global IPC4 message types - must fit into 5 bits.
 */
enum ipc4_message_type {
	/**< Boot Config. */
	SOF_IPC4_GLB_BOOT_CONFIG = 0,
	/**< ROM Control (directed to ROM). */
	SOF_IPC4_GLB_ROM_CONTROL = 1,
	/**< Execute IPC gateway command */
	SOF_IPC4_GLB_IPCGATEWAY_CMD = 2,

	/* GAP HERE- DO NOT USE - size 10 (3 .. 12)  */

	/**< Execute performance measurements command. */
	SOF_IPC4_GLB_PERF_MEASUREMENTS_CMD = 13,
	/**< DMA Chain command. */
	SOF_IPC4_GLB_CHAIN_DMA = 14,
	/**< Load multiple modules */
	SOF_IPC4_GLB_LOAD_MULTIPLE_MODULES = 15,
	/**< Unload multiple modules */
	SOF_IPC4_GLB_UNLOAD_MULTIPLE_MODULES = 16,
	/**< Create pipeline */
	SOF_IPC4_GLB_CREATE_PIPELINE = 17,
	/**< Delete pipeline */
	SOF_IPC4_GLB_DELETE_PIPELINE = 18,
	/**< Set pipeline state */
	SOF_IPC4_GLB_SET_PIPELINE_STATE = 19,
	/**< Get pipeline state */
	SOF_IPC4_GLB_GET_PIPELINE_STATE = 20,
	/**< Get pipeline context size */
	SOF_IPC4_GLB_GET_PIPELINE_CONTEXT_SIZE = 21,
	/**< Save pipeline */
	SOF_IPC4_GLB_SAVE_PIPELINE = 22,
	/**< Restore pipeline */
	SOF_IPC4_GLB_RESTORE_PIPELINE = 23,
	/**< Loads library */
	SOF_IPC4_GLB_LOAD_LIBRARY = 24,
	/**< Loads library prepare */
	SOF_IPC4_GLB_LOAD_LIBRARY_PREPARE = 25,
	/**< Internal FW message */
	SOF_IPC4_GLB_INTERNAL_MESSAGE = 26,
	/**< Notification (FW to SW driver) */
	SOF_IPC4_GLB_NOTIFICATION = 27,
	/* GAP HERE- DO NOT USE - size 3 (28 .. 30)  */
	/**< Enter GDB stub to wait for commands in memory window */
	SOF_IPC4_GLB_ENTER_GDB = 31,

	/**< Maximum message number */
	SOF_IPC4_GLB_MAX_IXC_MESSAGE_TYPE = 32
};

/**
 * \brief Generic message header. IPC MAJOR 4 version.
 * All IPC4 messages use this header as abstraction
 * to platform specific calls.
 */
struct ipc_cmd_hdr {
	uint32_t pri;
	uint32_t ext;
};

/**
 * \brief IPC MAJOR 4 message header. All IPC4 messages use this header.
 * When msg_tgt is SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG then type is
 * enum ipc4_message_type.
 */
struct ipc4_message_request {
	union  {
		uint32_t dat;

		struct {
			uint32_t rsvd0 : 24;

			/**< One of Global::Type */
			uint32_t type : 5;

			/**< Msg::MSG_REQUEST */
			uint32_t rsp : 1;

			/**< Msg::FW_GEN_MSG */
			uint32_t msg_tgt : 1;

			uint32_t _reserved_0 : 1;
		} r;
	} primary;
	union  {
		uint32_t dat;
		struct {
			uint32_t ext_data : 30;
			uint32_t _reserved_0 : 2;
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

struct ipc4_message_reply {
	union {
		uint32_t dat;

		struct {
			/**< Processing status, one of IxcStatus values */
			uint32_t status	: 24;

			/**< Type, symmetric to Msg */
			uint32_t type : 5;

			/**< MSG_REPLY */
			uint32_t rsp : 1;

			/**< same as request, one of FW_GEN_MSG, MODULE_MSG */
			uint32_t msg_tgt : 1;

			/**< Reserved field (HW ctrl bits) */
			uint32_t _reserved_0 : 1;
		} r;
	} primary;

	union {
		uint32_t dat;

		struct {
			/**< Reserved field */
			uint32_t rsvd1 : 30;

			/**< Reserved field (HW ctrl bits) */
			uint32_t _reserved_2 : 2;
		} r;
	} extension;
} __attribute((packed, aligned(4)));

#define SOF_IPC4_SWITCH_CONTROL_PARAM_ID	200
#define SOF_IPC4_ENUM_CONTROL_PARAM_ID		201
#define SOF_IPC4_BYTES_CONTROL_PARAM_ID		202
#define SOF_IPC4_NOTIFY_MODULE_EVENTID_ALSA_MAGIC_VAL ((uint32_t)(0xA15A << 16))

/**
 * struct sof_ipc4_ctrl_value_chan: generic channel mapped value data
 * @channel: Channel ID
 * @value: control value
 */
struct sof_ipc4_ctrl_value_chan {
	uint32_t channel;
	uint32_t value;
} __attribute((packed, aligned(4)));

/**
 * struct sof_ipc4_control_msg_payload - IPC payload for kcontrol parameters
 * @id: unique id of the control
 * @num_elems: Number of elements in the chanv array or number of bytes in data
 * @reserved: reserved for future use, must be set to 0
 * @chanv: channel ID and value array
 * @data: binary payload
 */
struct sof_ipc4_control_msg_payload {
	uint16_t id;
	uint16_t num_elems;
	uint32_t reserved[4];
	union {
		struct sof_ipc4_ctrl_value_chan chanv[0];
		uint8_t data[0];
	};
} __attribute((packed, aligned(4)));

/**
 * struct sof_ipc4_notify_module_data - payload for module notification
 * @instance_id: instance ID of the originator module of the notification
 * @module_id: module ID of the originator of the notification
 * @event_id: module specific event id
 * @event_data_size: Size of the @event_data (if any) in bytes
 * @event_data: Optional notification data, module and notification dependent
 */
struct sof_ipc4_notify_module_data {
	uint16_t instance_id;
	uint16_t module_id;
	uint32_t event_id;
	uint32_t event_data_size;
	uint8_t event_data[];
} __attribute((packed, aligned(4)));

#endif
