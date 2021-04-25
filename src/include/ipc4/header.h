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
 */

/**
 * \file include/ipc4/header.h
 * \brief IPC4 global definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_HEADER_H__
#define __SOF_IPC4_HEADER_H__

#include <stdint.h>

//! Message target, value of msg_tgt field.
enum ipc4_message_target {
	//! Global FW message
	SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG = 0,
	//! Module message
	SOF_IPC4_MESSAGE_TARGET_MODULE_MSG = 1
};

//! Message direction, value of rsp field.
enum ipc4_message_direction {
	//! Request, Notification
	SOF_IPC4_MESSAGE_DIR_MSG_REQUEST = 0,
	//! Reply
	SOF_IPC4_MESSAGE_DIR_MSG_REPLY = 1,
};

/*
 * Global IPC4 message types - must fit into 5 bits.
 */
enum ipc4_message_type {
	//! Boot Config.
	SOF_IPC4_GLB_BOOT_CONFIG = 0,
	//! ROM Control (directed to ROM).
	SOF_IPC4_GLB_ROM_CONTROL = 1,
	//! Execute IPC gateway command
	SOF_IPC4_GLB_IPCGATEWAY_CMD = 2,

	/* GAP HERE- DO NOT USE - size 10 (3 .. 12)  */

	//! Execute performance measurements command.
	SOF_IPC4_GLB_PERF_MEASUREMENTS_CMD = 13,
	//! DMA Chain command.
	SOF_IPC4_GLB_CHAIN_DMA = 14,
	//! Load multiple modules
	SOF_IPC4_GLB_LOAD_MULTIPLE_MODULES = 15,
	//! Unload multiple modules
	SOF_IPC4_GLB_UNLOAD_MULTIPLE_MODULES = 16,
	//! Create pipeline
	SOF_IPC4_GLB_CREATE_PIPELINE = 17,
	//! Delete pipeline
	SOF_IPC4_GLB_DELETE_PIPELINE = 18,
	//! Set pipeline state
	SOF_IPC4_GLB_SET_PIPELINE_STATE = 19,
	//! Get pipeline state
	SOF_IPC4_GLB_GET_PIPELINE_STATE = 20,
	//! Get pipeline context size
	SOF_IPC4_GLB_GET_PIPELINE_CONTEXT_SIZE = 21,
	//! Save pipeline
	SOF_IPC4_GLB_SAVE_PIPELINE = 22,
	//! Restore pipeline
	SOF_IPC4_GLB_RESTORE_PIPELINE = 23,
	//! Loads library
	SOF_IPC4_GLB_LOAD_LIBRARY = 24,
	//! Internal FW message
	SOF_IPC4_GLB_INTERNAL_MESSAGE = 26,
	//! Notification (FW to SW driver)
	SOF_IPC4_GLB_NOTIFICATION = 27,
	/* GAP HERE- DO NOT USE - size 3 (28 .. 30)  */

	//! Maximum message number
	SOF_IPC4_GLB_MAX_IXC_MESSAGE_TYPE = 31
};

/**
 * \brief IPC MAJOR 4 message header. All IPC4 messages use this header.
 * When msg_tgt is SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG then type is
 * enum ipc4_message_type.
 */
union ipc4_message_header {
	uint32_t dat;

	struct {
		uint32_t rsvd0		: 24;

		//! One of Global::Type
		uint32_t type		: 5;

		//! Msg::MSG_REQUEST
		uint32_t rsp		: 1;

		//! Msg::FW_GEN_MSG
		uint32_t msg_tgt	: 1;

		uint32_t _reserved_0: 1;
	} r;
} __attribute__((packed, aligned(4)));

#endif
