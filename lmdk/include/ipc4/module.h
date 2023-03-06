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
 * \file include/ipc4/module.h
 * \brief IPC4 module definitions
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __IPC4_MODULE_H__
#define __IPC4_MODULE_H__

#include <ipc4/error_status.h>

#include <stdint.h>

/* TODO: revisit it. Now it aligns with audio sdk
 * and we will update this value and sdk when more
 * libraries are supported
 */
#define IPC4_MAX_SUPPORTED_LIBRARIES 16

#define IPC4_MAX_MODULE_COUNT 128

#define SOF_IPC4_DST_QUEUE_ID_BITFIELD_SIZE	3
#define SOF_IPC4_SRC_QUEUE_ID_BITFIELD_SIZE	3

enum sof_ipc4_module_type {
	SOF_IPC4_MOD_INIT_INSTANCE		= 0,
	SOF_IPC4_MOD_CONFIG_GET			= 1,
	SOF_IPC4_MOD_CONFIG_SET			= 2,
	SOF_IPC4_MOD_LARGE_CONFIG_GET		= 3,
	SOF_IPC4_MOD_LARGE_CONFIG_SET		= 4,
	SOF_IPC4_MOD_BIND			= 5,
	SOF_IPC4_MOD_UNBIND			= 6,
	SOF_IPC4_MOD_SET_DX			= 7,
	SOF_IPC4_MOD_SET_D0IX			= 8,
	SOF_IPC4_MOD_ENTER_MODULE_RESTORE	= 9,
	SOF_IPC4_MOD_EXIT_MODULE_RESTORE	= 10,
	SOF_IPC4_MOD_DELETE_INSTANCE		= 11,
};

/*
 * Host Driver sends this message to create a new module instance.
 */
struct ipc4_module_init_ext_init {
	/**< if it is set to 1, proc_domain should be ignored and processing */
	/* domain is RTOS scheduling */
	uint32_t rtos_domain : 1;
	/**< Indicates that GNA is used by a module and additional information */
	/* (gna_config) is passed after ExtendedData. */
	uint32_t gna_used    : 1;
	uint32_t rsvd_0      : 30;
	uint32_t rsvd_1[2];
} __attribute__((packed, aligned(4)));

struct ipc4_module_init_ext_data {
	struct ipc4_module_init_ext_init extended_init;

	/**< Data (actual size set to param_block_size) */
	uint32_t param_data[0];
} __attribute__((packed, aligned(4)));

struct ipc4_module_init_gna_config {
	/**< Number of GNA cycles required to process one input frame. */
	/* This information is used by DP scheduler to correctly schedule
	 * a DP module.
	 */
	uint32_t gna_cpc;
	uint32_t rsvd;
} __attribute__((packed, aligned(4)));

struct ipc4_module_init_data {
	/**< Data (actual size set to param_block_size) */
	uint32_t param_data[0];
} __attribute__((packed, aligned(4)));

/*!
  Created instance is a child element of pipeline identified by the ppl_id
  specified by the driver.

  The module_id should be set to an index of the module entry in the FW Image
  Manifest.

  The instance_id assigned by the driver should be in the
  0..ModuleEntry.max_instance_count range defined in the FW Image Manifest.

  Initial configuration of the module instance is provided by the driver in
  the param_data array. Size of the array is specified in param_block_size
  field of the message header.

  Refer to Module Configuration section of FW I/F Specification for details on
  module specific initial configuration parameters.

  \remark hide_methods
*/
struct ipc4_module_init_instance {

	union {
		uint32_t dat;

		struct {
			/**< module id */
			uint32_t module_id          : 16;
			/**< instance id */
			uint32_t instance_id        : 8;
			/**< ModuleMsg::INIT_INSTANCE */
			uint32_t type               : 5;
			/**< Msg::MSG_REQUEST */
			uint32_t rsp                : 1;
			/**< Msg::MODULE_MSG */
			uint32_t msg_tgt            : 1;
			uint32_t _reserved_0        : 1;
		} r;
	} primary;

	union {
		uint32_t dat;

		struct {
			/**< Size of Data::param_data[] (in dwords) */
			uint32_t param_block_size   : 16;
			/**< ID of module instance's parent pipeline */
			uint32_t ppl_instance_id    : 8;
			/**< ID of core that instance will run on */
			uint32_t core_id            : 4;
			/**< Processing domain, 0-LL, 1-DP */
			uint32_t proc_domain        : 1;
			/* reserved in cAVS  */
			uint32_t extended_init      : 1;
			uint32_t _hw_reserved_2     : 2;
		} r;
	} extension;

	struct ipc4_module_init_ext_init ext_init;
	struct ipc4_module_init_ext_data ext_data;
	struct ipc4_module_init_gna_config gna_config;
	struct ipc4_module_init_data init_data;
} __attribute__((packed, aligned(4)));

/*!
  SW Driver sends Bind IPC message to connect two module instances together
  creating data processing path between them.

  Unbind IPC message is sent to destroy a connection between two module instances
  (belonging to different pipelines) previously created with Bind call.

  NOTE: when both module instances are parts of the same pipeline Unbind IPC would
  be ignored by FW since FW does not support changing internal topology of pipeline
  during run-time. The only way to change pipeline topology is to delete the whole
  pipeline and create it in modified form.

  \remark hide_methods
 */
struct ipc4_module_bind_unbind {
	union {
		uint32_t dat;

		struct {
			/**< module id */
			uint32_t module_id : 16;
			/**< instance id */
			uint32_t instance_id : 8;
			/**< ModuleMsg::BIND / UNBIND. */
			uint32_t type : 5;
			/**< Msg::MSG_REQUEST */
			uint32_t rsp : 1;
			/**< Msg::MODULE_MSG */
			uint32_t msg_tgt : 1;
			uint32_t _reserved_0 : 1;
		} r;
	} primary;

	union {
		uint32_t dat;

		struct {
			/**< destination module id */
			uint32_t dst_module_id : 16;
			/**< destination instance id */
			uint32_t dst_instance_id : 8;
			/**< destination queue (pin) id */
			uint32_t dst_queue : SOF_IPC4_DST_QUEUE_ID_BITFIELD_SIZE;
			/**< source queue (pin) id */
			uint32_t src_queue : SOF_IPC4_SRC_QUEUE_ID_BITFIELD_SIZE;
			uint32_t _reserved_2 : 2;
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

struct ipc4_module_large_config {
	union {
		uint32_t dat;

		struct {
			/**< module id */
			uint32_t module_id : 16;
			/**< instance id */
			uint32_t instance_id : 8;
			/**< ModuleMsg::LARGE_CONFIG_GET / LARGE_CONFIG_SET */
			uint32_t type : 5;
			/**< Msg::MSG_REQUEST or Msg::MSG_REPLY */
			uint32_t rsp : 1;
			/**< Msg::MODULE_MSG or Msg::FW_GEN_MSG */
			uint32_t msg_tgt : 1;
			uint32_t _reserved_0 : 1;
			} r;
		} primary;

	union {
		uint32_t dat;

		struct {
			/**< data size for single block, offset for multiple block case */
			uint32_t data_off_size : 20;
			/**< param type : VENDOR_CONFIG_PARAM / GENERIC_CONFIG_PARAM */
			uint32_t large_param_id : 8;
			/**< 1 if final block */
			uint32_t final_block : 1;
			/**< 1 if init block */
			uint32_t init_block : 1;
			uint32_t _reserved_2 : 2;
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

struct ipc4_module_large_config_reply {
	union {
		uint32_t dat;

		struct {
			uint32_t status :IPC4_IXC_STATUS_BITS;
			/**< ModuleMsg::LARGE_CONFIG_GET / LARGE_CONFIG_SET */
			uint32_t type : 5;
			/**< Msg::MSG_REQUEST */
			uint32_t rsp : 1;
			/**< Msg::MODULE_MSG */
			uint32_t msg_tgt : 1;
			uint32_t _reserved_0 : 1;
			} r;
		} primary;

	union {
		uint32_t dat;

		struct {
			/**< data size/offset */
			uint32_t data_off_size : 20;
			/**< param type : VENDOR_CONFIG_PARAM / GENERIC_CONFIG_PARAM */
			uint32_t large_param_id : 8;
			/**< 1 if final block */
			uint32_t final_block : 1;
			/**< 1 if first block */
			uint32_t init_block : 1;
			uint32_t _reserved_2 : 2;
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

struct ipc4_module_delete_instance {
	union {
		uint32_t dat;

		struct {
			uint32_t module_id : 16;
			uint32_t instance_id : 8;
			/**< ModuleMsg::DELETE_INSTANCE */
			uint32_t type : 5;
			/**< Msg::MSG_REQUEST */
			uint32_t rsp : 1;
			/**< Msg::MODULE_MSG */
			uint32_t msg_tgt : 1;
			uint32_t _reserved_0 : 1;
		} r;
	} primary;

	union {
		uint32_t dat;

		struct {
			uint32_t rsvd : 30;
			uint32_t _reserved_1 : 2;
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

struct ipc4_module_set_d0ix {
	union {
		uint32_t dat;

		struct {
			/* module id (must be 0 - Base FW) */
			uint32_t module_id		: 16;
			/* instance id (must be 0 - core 0) */
			uint32_t instance_id	: 8;
			/* ModuleMsg::SET_D0IX */
			uint32_t type			: 5;
			/* Msg::MSG_REQUEST */
			uint32_t rsp			: 1;
			/* Msg::MODULE_MSG */
			uint32_t msg_tgt		: 1;
			uint32_t _reserved_0	: 1;
		} r;
	} primary;

	union {
		uint32_t dat;

		struct {
			/* legacy wake type (see WakeType) */
			uint32_t wake			: 1;
			/* streaming active now */
			uint32_t streaming		: 1;
			/* D0/D0ix transitions allowed (PG disabled) */
			uint32_t prevent_power_gating : 1;
			/* Clock gating enabled */
			uint32_t prevent_local_clock_gating : 1;

			uint32_t rsvd1			: 26;
			uint32_t _reserved_2	: 2;
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

struct ipc4_dx_state_info {
	/* Indicates which cores are subject to change the power state */
	uint32_t core_mask;
	/* Indicates core state.
	 * bit[core_id] = 0 -> put core_id to D3
	 * bit[core_id] = 1 -> put core_id to D0
	 */
	uint32_t dx_mask;
} __packed __aligned(4);

struct ipc4_module_set_dx {
	union {
		uint32_t dat;

		struct {
			/* module id (must be 0 - Base FW) */
			uint32_t module_id			: 16;
			/* instance id (must be 0 - core 0) */
			uint32_t instance_id		: 8;
			/* ModuleMsg::SET_DX */
			uint32_t type				: 5;
			/* Msg::MSG_REQUEST */
			uint32_t rsp				: 1;
			/* Msg::MODULE_MSG */
			uint32_t msg_tgt			: 1;
			uint32_t _reserved_0		: 1;
		} r;
	} primary;

	union {
		uint32_t dat;

		struct {
			uint32_t rsvd				: 30;
			uint32_t _reserved_2		: 2;
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

struct ipc4_module_load_library {
	union {
		uint32_t dat;

		struct {
			/* ID of HD/A HO DMA to load the code */
			uint32_t dma_id				: 5;
			uint32_t rsvd0				: 11;
			/* ID of library */
			uint32_t lib_id				: 4;
			uint32_t rsvd1				: 4;
			/* Global::LOAD_LIBRARY */
			uint32_t type				: 5;
			/* Msg::MSG_REQUEST */
			uint32_t rsp				: 1;
			/* Msg::FW_GEN_MSG */
			uint32_t msg_tgt			: 1;
			uint32_t _reserved_0		: 1;
		} r;
	} header;

	union {
		uint32_t dat;

		struct {
			uint32_t load_offset		: 30;
			uint32_t _reserved_2		: 2;
		} r;
	} data;
} __packed __aligned(4);

#define IPC4_COMP_ID(x, y)	((x) << 16 | (y))
#define IPC4_MOD_ID(x) ((x) >> 16)
#define IPC4_INST_ID(x)	((x) & 0xffff)
#define IPC4_SRC_QUEUE_ID(x)	(((x) >> 16) & 0xffff)
#define IPC4_SINK_QUEUE_ID(x)	((x) & 0xffff)

#endif
