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
 * \brief IPC4 global message headers, envelope formats, and command types.
 * \ingroup ipc4_envelope_api
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_HEADER_H__
#define __SOF_IPC4_HEADER_H__

#include <stdint.h>

/** Cast a generic buffer or message pointer to an IPC4 message request structure. */
#define ipc_from_hdr(x) ((struct ipc4_message_request *)x)

/**
 * \brief Message target routing selector (msg_tgt bitfield in Primary DWORD).
 * \ingroup ipc4_envelope_api
 */
enum ipc4_message_target {
	/** Global firmware message (dispatched to Base FW or global handler). */
	SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG = 0,
	/** Module-specific message (routed directly to module instance by module_id). */
	SOF_IPC4_MESSAGE_TARGET_MODULE_MSG = 1
};

/**
 * \brief Message direction indicator (rsp bitfield in Primary DWORD).
 * \ingroup ipc4_envelope_api
 */
enum ipc4_message_direction {
	/** Host-to-DSP command request, or unsolicited DSP-to-host notification. */
	SOF_IPC4_MESSAGE_DIR_MSG_REQUEST = 0,
	/** Command completion response / reply. */
	SOF_IPC4_MESSAGE_DIR_MSG_REPLY = 1,
};

/**
 * \brief Global IPC4 message types (5-bit opcode in Primary DWORD when msg_tgt == FW_GEN_MSG).
 * \ingroup ipc4_envelope_api
 */
enum ipc4_message_type {
	/** Boot configuration handshake. */
	SOF_IPC4_GLB_BOOT_CONFIG = 0,
	/** ROM control command (directed to ADSP ROM bootloader). */
	SOF_IPC4_GLB_ROM_CONTROL = 1,
	/** Execute IPC gateway command. */
	SOF_IPC4_GLB_IPCGATEWAY_CMD = 2,

	/* GAP HERE- DO NOT USE - size 10 (3 .. 12)  */

	/** Execute performance measurement command. */
	SOF_IPC4_GLB_PERF_MEASUREMENTS_CMD = 13,
	/** DMA chain setup and configuration command. */
	SOF_IPC4_GLB_CHAIN_DMA = 14,
	/** Load multiple module binary images. */
	SOF_IPC4_GLB_LOAD_MULTIPLE_MODULES = 15,
	/** Unload multiple module binary images. */
	SOF_IPC4_GLB_UNLOAD_MULTIPLE_MODULES = 16,
	/** Create pipeline with specified ID, priority, and memory attributes. */
	SOF_IPC4_GLB_CREATE_PIPELINE = 17,
	/** Delete pipeline and associated module graph. */
	SOF_IPC4_GLB_DELETE_PIPELINE = 18,
	/** Transition pipeline state (RESET, PAUSED, RUNNING, EOS). */
	SOF_IPC4_GLB_SET_PIPELINE_STATE = 19,
	/** Retrieve current pipeline state. */
	SOF_IPC4_GLB_GET_PIPELINE_STATE = 20,
	/** Query pipeline context memory size. */
	SOF_IPC4_GLB_GET_PIPELINE_CONTEXT_SIZE = 21,
	/** Save pipeline context to host memory before power gating. */
	SOF_IPC4_GLB_SAVE_PIPELINE = 22,
	/** Restore pipeline context from host memory after wake. */
	SOF_IPC4_GLB_RESTORE_PIPELINE = 23,
	/** Load dynamic library via HD-Audio DMA. */
	SOF_IPC4_GLB_LOAD_LIBRARY = 24,
	/** Prepare for dynamic library loading. */
	SOF_IPC4_GLB_LOAD_LIBRARY_PREPARE = 25,
	/** Internal firmware control message. */
	SOF_IPC4_GLB_INTERNAL_MESSAGE = 26,
	/** Asynchronous notification event from DSP to host driver. */
	SOF_IPC4_GLB_NOTIFICATION = 27,
	/* GAP HERE- DO NOT USE - size 3 (28 .. 30)  */
	/** Enter GDB debugging stub to wait for host debugger in memory window. */
	SOF_IPC4_GLB_ENTER_GDB = 31,

	/** Maximum supported global message opcode count. */
	SOF_IPC4_GLB_MAX_IXC_MESSAGE_TYPE = 32
};

/**
 * \brief Generic raw 64-bit IPC4 message header registers.
 * \ingroup ipc4_envelope_api
 *
 * All IPC4 messages are passed across the mailbox as a pair of 32-bit registers
 * (Primary DWORD + Extension DWORD).
 */
struct ipc_cmd_hdr {
	uint32_t pri; /**< Primary 32-bit register value. */
	uint32_t ext; /**< Extension 32-bit register value. */
};

/**
 * \brief Standard 64-bit IPC4 message request envelope.
 * \ingroup ipc4_envelope_api
 *
 * Used for all host-to-DSP command requests. When msg_tgt is
 * #SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG, the type field is one of #ipc4_message_type.
 * When msg_tgt is #SOF_IPC4_MESSAGE_TARGET_MODULE_MSG, type is one of #sof_ipc4_module_type.
 */
struct ipc4_message_request {
	union  {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			/** Command-specific payload, pipeline ID, or module/instance identifier. */
			uint32_t rsvd0 : 24;

			/** 5-bit message opcode (enum ipc4_message_type or enum sof_ipc4_module_type). */
			uint32_t type : 5;

			/** Direction: 0 for request/notification (SOF_IPC4_MESSAGE_DIR_MSG_REQUEST). */
			uint32_t rsp : 1;

			/** Target selector: 0 for global firmware, 1 for module instance. */
			uint32_t msg_tgt : 1;

			/** Hardware reserved control bit. */
			uint32_t _reserved_0 : 1;
		} r;
	} primary;
	union  {
		uint32_t dat; /**< Raw 32-bit Extension register value. */
		struct {
			/** Command-specific extension payload, buffer offsets, or parameter flags. */
			uint32_t ext_data : 30;
			/** Hardware reserved control bits. */
			uint32_t _reserved_0 : 2;
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/**
 * \brief Standard 64-bit IPC4 message reply envelope.
 * \ingroup ipc4_envelope_api
 *
 * Written to the mailbox registers by the DSP firmware upon command completion.
 * The primary register contains the processing status code in its lower 24 bits.
 */
struct ipc4_message_reply {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			/** Command processing status (one of enum ipc4_status). */
			uint32_t status	: 24;

			/** Message type, symmetric to the incoming request. */
			uint32_t type : 5;

			/** Direction: 1 for reply (SOF_IPC4_MESSAGE_DIR_MSG_REPLY). */
			uint32_t rsp : 1;

			/** Target: symmetric to request (FW_GEN_MSG or MODULE_MSG). */
			uint32_t msg_tgt : 1;

			/** Hardware control reserved bit. */
			uint32_t _reserved_0 : 1;
		} r;
	} primary;

	union {
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			/** Extension reply data, transfer length, or additional status information. */
			uint32_t rsvd1 : 30;

			/** Hardware control reserved bits. */
			uint32_t _reserved_2 : 2;
		} r;
	} extension;
} __attribute((packed, aligned(4)));

/** ALSA/kcontrol switch parameter ID for IPC4 Large Config. */
#define SOF_IPC4_SWITCH_CONTROL_PARAM_ID	200
/** ALSA/kcontrol enum parameter ID for IPC4 Large Config. */
#define SOF_IPC4_ENUM_CONTROL_PARAM_ID		201
/** ALSA/kcontrol binary bytes parameter ID for IPC4 Large Config. */
#define SOF_IPC4_BYTES_CONTROL_PARAM_ID		202
/** Magic event ID prefix for ALSA kcontrol notification events (0xA15A0000). */
#define SOF_IPC4_NOTIFY_MODULE_EVENTID_ALSA_MAGIC_VAL ((uint32_t)0xA15A << 16)
/** Magic event ID prefix for compressed audio stream notifications (0xC0C00000). */
#define SOF_IPC4_NOTIFY_MODULE_EVENTID_COMPR_MAGIC_VAL ((uint32_t)0xC0C0 << 16)

/**
 * \brief Generic per-channel control value mapping.
 * \ingroup ipc4_envelope_api
 */
struct sof_ipc4_ctrl_value_chan {
	uint32_t channel; /**< Logical audio channel ID (0 = Left, 1 = Right, etc.). */
	uint32_t value;   /**< Control integer value (gain level, mute toggle, enum index). */
} __attribute((packed, aligned(4)));

/**
 * \brief IPC4 payload structure for ALSA kcontrol parameters.
 * \ingroup ipc4_envelope_api
 */
struct sof_ipc4_control_msg_payload {
	uint16_t id;          /**< Unique identifier of the control. */
	uint16_t num_elems;   /**< Number of elements in the chanv array or number of bytes in data. */
	uint32_t reserved[4]; /**< Reserved for future expansion; must be set to 0. */
	union {
		struct sof_ipc4_ctrl_value_chan chanv[0]; /**< Channel ID and value array. */
		uint8_t data[0];                          /**< Binary byte payload for raw controls. */
	};
} __attribute((packed, aligned(4)));

/**
 * \brief Asynchronous notification payload originating from a module instance.
 * \ingroup ipc4_envelope_api
 */
struct sof_ipc4_notify_module_data {
	uint16_t instance_id;     /**< Instance ID of the originator module of the notification. */
	uint16_t module_id;       /**< Module ID of the originator of the notification. */
	uint32_t event_id;        /**< Module-specific event identifier. */
	uint32_t event_data_size; /**< Size of the event_data payload (if any) in bytes. */
	uint8_t event_data[];     /**< Optional notification data, module and notification dependent. */
} __attribute((packed, aligned(4)));

#endif
