/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 */

/*
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

/* Special large_param_id values */
#define VENDOR_CONFIG_PARAM 0xFF

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
 * Structs for Vendor Config
 */

union ipc4_extended_param_id {
	uint32_t full;
	struct{
		uint32_t parameter_type     : 8;
		uint32_t parameter_instance : 24;
	} part;
} __attribute__((packed, aligned(4)));

struct ipc4_vendor_error {
	/* Index of the failed parameter */
	uint32_t param_idx;
	/* Error code */
	uint32_t err_code;
};

/* IDs for all global object types in struct ipc4_module_init_ext_object */
enum ipc4_mod_init_data_glb_id {
	IPC4_MOD_INIT_DATA_ID_INVALID = 0,
	IPC4_MOD_INIT_DATA_ID_DP_DATA = 1,
	IPC4_MOD_INIT_DATA_ID_MAX = IPC4_MOD_INIT_DATA_ID_DP_DATA,
};

/* data object for vendor bespoke data with ABI growth and backwards compat */
struct ipc4_module_init_ext_object {
	uint32_t last_object : 1;	/* object is last in array if 1 else object follows. */
	uint32_t object_id : 15;	/* unique ID for this object or globally */
	uint32_t object_words : 16;	/* size in dwords (excluding this structure) */
} __attribute__((packed, aligned(4)));
/* the object data will be placed in memory here and will have size "object_words" */

/* Ext init array data object for Data Processing module memory requirements */
struct ipc4_module_init_ext_obj_dp_data {
	uint32_t domain_id;	/* userspace domain ID */
	uint32_t stack_bytes;	/* required stack size in bytes, 0 means default size */
	uint32_t heap_bytes;	/* required heap size in bytes, 0 means default size */
} __attribute__((packed, aligned(4)));

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
	uint32_t data_obj_array : 1;	/* struct ipc4_module_init_ext_object data */
	uint32_t rsvd_0      : 29;
	uint32_t rsvd_1[2];
} __attribute__((packed, aligned(4)));

struct ipc4_module_init_ext_data {
	struct ipc4_module_init_ext_init extended_init;

	/**< Data (actual size set to param_block_size) */
	uint32_t param_data[];
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
/*
 * The following objects are optional and follow this structure in this
 * order provided the respective object bit is set in preceding object.
 *
 *	struct ipc4_module_init_ext_init ext_init;
 *	struct ipc4_module_init_ext_data ext_data;
 *	struct ipc4_module_init_gna_config gna_config;
 *	struct ipc4_module_init_data init_data;
 */
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

/*
 * Using Module Config Get / Set command, host driver may send a parameter
 * that fits into the header (a very short one), packed along with parameter id.
 * Larger parameters require fragmentation and a series of Large Config Set
 * commands.
 *
 * param_id_data specifies both ID of the parameter, defined by the module
 * and value of the parameter.
 * It is up to the module how to distribute bits to ID and value of the parameter.
 * If there are more bits required than available to value, then Input Data may
 * be used to pass the value
 *
 * NOTE: Module Config Get/Set commands are used internally by the driver
 * for small parameters defined by Intel components. While all externally
 * developed components communicates with host using Large Config commands
 * no matter what the size of parameter is.
 */
struct ipc4_module_config {
	union {
		uint32_t dat;

		struct {
			uint32_t module_id      : 16;	/* module id */
			uint32_t instance_id    : 8;	/* instance id */
			/* SOF_IPC4_MOD_CONFIG_GET / SOF_IPC4_MOD_CONFIG_SET */
			uint32_t type           : 5;
			uint32_t rsp            : 1;	/* SOF_IPC4_MESSAGE_DIR_MSG_REQUEST */
			uint32_t msg_tgt        : 1;	/* SOF_IPC4_MESSAGE_TARGET_MODULE_MSG */
			uint32_t _reserved_0    : 1;
		} r;
	} primary;

	union {
		uint32_t dat;

		struct {
			/* Param id and data */
			uint32_t param_id_data  : 30;
			uint32_t _reserved_2    : 2;
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/*
 * Sent by FW in response to Module Config Get.
 */
struct ipc4_module_config_reply {
	union {
		uint32_t dat;

		struct {
			uint32_t status		: IPC4_IXC_STATUS_BITS;
			uint32_t type		: 5;	/* SOF_IPC4_MOD_CONFIG_GET */
			uint32_t rsp		: 1;	/* SOF_IPC4_MESSAGE_DIR_MSG_REPLY */
			uint32_t msg_tgt	: 1;	/* SOF_IPC4_MESSAGE_TARGET_MODULE_MSG */
			uint32_t _reserved_0	: 1;
		} r;
	} primary;

	union {
		uint32_t dat;

		struct {
			/*
			 * Value of this field may be changed by the module
			 * if parameter value fits into the available bits,
			 * or stay intact if the value is copied to the Output Data.
			 */
			uint32_t param_id_data	: 30;
			uint32_t _reserved_2	: 2;
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
} __attribute__((packed, aligned(4)));

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
} __attribute__((packed, aligned(4)));

#define IPC4_COMP_ID(x, y)	((y) << 16 | (x))
#define IPC4_MOD_ID(x)	(IS_ENABLED(CONFIG_IPC_MAJOR_4) ? ((x) & 0xffff) : 0)
#define IPC4_INST_ID(x)	((x) >> 16)
#define IPC4_SRC_QUEUE_ID(x)	((x) & 0xffff)
#define IPC4_SINK_QUEUE_ID(x)	(((x) >> 16) & 0xffff)

#endif
