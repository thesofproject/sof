/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 */

/**
 * \file include/ipc4/module.h
 * \brief IPC4 module definitions
 */

#ifndef __IPC4_MODULE_H__
#define __IPC4_MODULE_H__

#include <stdint.h>

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

/* common module ipc msg */
#define SOF_IPC4_MOD_INSTANCE_SHIFT	16
#define SOF_IPC4_MOD_INSTANCE_MASK	0xFF0000
#define SOF_IPC4_MOD_INSTANCE(x)	((x) << SOF_IPC4_MOD_INSTANCE_SHIFT)

#define SOF_IPC4_MOD_ID_SHIFT	0
#define SOF_IPC4_MOD_ID_MASK	0xFFFF
#define SOF_IPC4_MOD_ID(x)	((x) << SOF_IPC4_MOD_ID_SHIFT)

/* init module ipc msg */
#define SOF_IPC4_MOD_EXT_PARAM_SIZE_SHIFT	0
#define SOF_IPC4_MOD_EXT_PARAM_SIZE_MASK	0xFFFF
#define SOF_IPC4_MOD_EXT_PARAM_SIZE(x)	((x) << SOF_IPC4_MOD_EXT_PARAM_SIZE_SHIFT)

#define SOF_IPC4_MOD_EXT_PPL_ID_SHIFT	16
#define SOF_IPC4_MOD_EXT_PPL_ID_MASK	0xFF0000
#define SOF_IPC4_MOD_EXT_PPL_ID(x)	((x) << SOF_IPC4_MOD_EXT_PPL_ID_SHIFT)

#define SOF_IPC4_MOD_EXT_CORE_ID_SHIFT	24
#define SOF_IPC4_MOD_EXT_CORE_ID_MASK	0xF000000
#define SOF_IPC4_MOD_EXT_CORE_ID(x)	((x) << SOF_IPC4_MOD_EXT_CORE_ID_SHIFT)

#define SOF_IPC4_MOD_EXT_DOMAIN_SHIFT	28
#define SOF_IPC4_MOD_EXT_DOMAIN_MASK	BIT(28)
#define SOF_IPC4_MOD_EXT_DOMAIN(x)	((x) << SOF_IPC4_MOD_EXT_DOMAIN_SHIFT)

/*  bind/unbind module ipc msg */
#define SOF_IPC4_MOD_EXT_DST_MOD_ID_SHIFT	0
#define SOF_IPC4_MOD_EXT_DST_MOD_ID_MASK	0xFFFF
#define SOF_IPC4_MOD_EXT_DST_MOD_ID(x)	((x) << SOF_IPC4_MOD_EXT_DST_MOD_ID_SHIFT)

#define SOF_IPC4_MOD_EXT_DST_MOD_INSTANCE_SHIFT	16
#define SOF_IPC4_MOD_EXT_DST_MOD_INSTANCE_MASK	0xFF0000
#define SOF_IPC4_MOD_EXT_DST_MOD_INSTANCE(x)	((x) << SOF_IPC4_MOD_EXT_DST_MOD_INSTANCE_SHIFT)

#define SOF_IPC4_MOD_EXT_DST_MOD_QUEUE_ID_SHIFT	24
#define SOF_IPC4_MOD_EXT_DST_MOD_QUEUE_ID_MASK	0x7000000
#define SOF_IPC4_MOD_EXT_DST_MOD_QUEUE_ID(x)	((x) << SOF_IPC4_MOD_EXT_DST_MOD_QUEUE_ID_SHIFT)

#define SOF_IPC4_MOD_EXT_SRC_MOD_QUEUE_ID_SHIFT	27
#define SOF_IPC4_MOD_EXT_SRC_MOD_QUEUE_ID_MASK	0x38000000
#define SOF_IPC4_MOD_EXT_SRC_MOD_QUEUE_ID(x)	((x) << SOF_IPC4_MOD_EXT_SRC_MOD_QUEUE_ID_SHIFT)

#define MOD_ENABLE_LOG	6
#define MOD_SYSTEM_TIME	20

/* set module large config */
#define SOF_IPC4_MOD_EXT_MSG_SIZE_SHIFT	0
#define SOF_IPC4_MOD_EXT_MSG_SIZE_MASK	0xFFFFF
#define SOF_IPC4_MOD_EXT_MSG_SIZE(x)	((x) << SOF_IPC4_MOD_EXT_MSG_SIZE_SHIFT)

#define SOF_IPC4_MOD_EXT_MSG_PARAM_ID_SHIFT	20
#define SOF_IPC4_MOD_EXT_MSG_PARAM_ID_MASK	0xFF00000
#define SOF_IPC4_MOD_EXT_MSG_PARAM_ID(x)	((x) << SOF_IPC4_MOD_EXT_MSG_PARAM_ID_SHIFT)

#define SOF_IPC4_MOD_EXT_MSG_LAST_BLOCK_SHIFT	28
#define SOF_IPC4_MOD_EXT_MSG_LAST_BLOCK_MASK	BIT(28)
#define SOF_IPC4_MOD_EXT_MSG_LAST_BLOCK(x)	((x) << SOF_IPC4_MOD_EXT_MSG_LAST_BLOCK_SHIFT)

#define SOF_IPC4_MOD_EXT_MSG_FIRST_BLOCK_SHIFT	29
#define SOF_IPC4_MOD_EXT_MSG_FIRST_BLOCK_MASK	BIT(29)
#define SOF_IPC4_MOD_EXT_MSG_FIRST_BLOCK(x)	((x) << SOF_IPC4_MOD_EXT_MSG_FIRST_BLOCK_SHIFT)

/*
 * //! Host Driver sends this message to create a new module instance.
 */
struct ipc4_module_init_ext_init {
	//! if it is set to 1, proc_domain should be ignored and processing
	// domain is RTOS scheduling
	uint32_t rtos_domain : 1;
	//! Indicates that GNA is used by a module and additional information
	//(gna_config) is passed after ExtendedData.
	uint32_t gna_used    : 1;
	uint32_t rsvd_0      : 30;
	uint32_t rsvd_1[2];
} __attribute__((packed, aligned(4)));

struct ipc4_module_init_ext_data {
	struct ipc4_module_init_ext_init extended_init;

	//! Data (actual size set to param_block_size)
	uint32_t param_data[0];
} __attribute__((packed, aligned(4)));

struct ipc4_module_init_gna_config {
	//! Number of GNA cycles required to process one input frame.
	//This information is used by DP scheduler to correctly schedule a DP module.
	uint32_t gna_cpc;
	uint32_t rsvd;
} __attribute__((packed, aligned(4)));

struct ipc4_module_init_data {
	//! Data (actual size set to param_block_size)
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

	//! module id
	uint32_t module_id          : 16;
	//! instance id
	uint32_t instance_id        : 8;
	//! ModuleMsg::INIT_INSTANCE
	uint32_t type               : 5;
	//! Msg::MSG_REQUEST
	uint32_t rsp                : 1;
	//! Msg::MODULE_MSG
	uint32_t msg_tgt            : 1;
	uint32_t _reserved_0        : 1;

	//! Size of Data::param_data[] (in dwords)
	uint32_t param_block_size   : 16;
	//! ID of module instance's parent pipeline
	uint32_t ppl_instance_id    : 8;
	//! ID of core that instance will run on
	uint32_t core_id            : 4;
	//! Processing domain, 0-LL, 1-DP
	uint32_t proc_domain        : 1;
	 // reserved in cAVS
	uint32_t extended_init      : 1;
	uint32_t _hw_reserved_2     : 2;

	struct ipc4_module_init_ext_init ext_init;
	struct ipc4_module_init_ext_data ext_data;
	struct ipc4_module_init_gna_config gna_config;
	struct ipc4_module_init_data data;
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

	//! module id
	uint32_t module_id          : 16;
	//! instance id
	uint32_t instance_id        : 8;
	//! ModuleMsg::BIND / UNBIND.
	uint32_t type               : 5;
	//! Msg::MSG_REQUEST
	uint32_t rsp                : 1;
	//! Msg::MODULE_MSG
	uint32_t msg_tgt            : 1;
	uint32_t _reserved_0        : 1;

	//! destination module id
	uint32_t dst_module_id      : 16;
	//! destination instance id
	uint32_t dst_instance_id    : 8;
	//! destination queue (pin) id
	uint32_t dst_queue          : SOF_IPC4_DST_QUEUE_ID_BITFIELD_SIZE;
	//! source queue (pin) id
	uint32_t src_queue          : SOF_IPC4_SRC_QUEUE_ID_BITFIELD_SIZE;
	uint32_t _reserved_2        : 2;
} __attribute__((packed, aligned(4)));

#endif

