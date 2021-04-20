/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 */

/**
 * \file include/ipc4/header.h
 * \brief IPC4 gloabl definitions
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
	//! Create EDF task and run FreeRTOS instance in it
	SOF_IPC4_GLB_START_RTOS_EDF_TASK = 3,
	//! Stop FreeRTOS and delete its EDF task context
	SOF_IPC4_GLB_STOP_RTOS_EDF_TASK = 4,

	/* GAP size 8 here - can 5 .. 12  be used ?? */

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
	//! Loads library (using Code Load or HD/A Host Output DMA)
	SOF_IPC4_GLB_LOAD_LIBRARY = 24,
	//! Internal FW message
	SOF_IPC4_GLB_INTERNAL_MESSAGE = 26,
	//! Notification (FW to SW driver)
	SOF_IPC4_GLB_NOTIFICATION = 27,

	/* Gap size 3 here - can be 28 .. 30 used ?? */

	//! Maximum message number
	SOF_IPC4_GLB_MAX_IXC_MESSAGE_TYPE = 31
};

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
