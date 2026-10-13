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
 * \file include/ipc4/pipeline.h
 * \brief IPC4 pipeline creation, states, priorities, and management commands.
 * \ingroup ipc4_pipeline_api
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_PIPELINE_H__
#define __SOF_IPC4_PIPELINE_H__

#include <stdint.h>
#include <ipc4/error_status.h>

/**
 * \brief Pipeline execution scheduling priority levels.
 * \ingroup ipc4_pipeline_api
 */
enum ipc4_pipeline_priority {
	SOF_IPC4_PIPELINE_PRIORITY_0 = 0, /**< Priority level 0 (highest priority, scheduled first). */
	SOF_IPC4_PIPELINE_PRIORITY_1,     /**< Priority level 1. */
	SOF_IPC4_PIPELINE_PRIORITY_2,     /**< Priority level 2. */
	SOF_IPC4_PIPELINE_PRIORITY_3,     /**< Priority level 3. */
	SOF_IPC4_PIPELINE_PRIORITY_4,     /**< Priority level 4. */
	SOF_IPC4_PIPELINE_PRIORITY_5,     /**< Priority level 5. */
	SOF_IPC4_PIPELINE_PRIORITY_6,     /**< Priority level 6. */
	SOF_IPC4_PIPELINE_PRIORITY_7,     /**< Priority level 7 (lowest priority). */
	SOF_IPC4_MAX_PIPELINE_PRIORITY = SOF_IPC4_PIPELINE_PRIORITY_7 /**< Maximum valid priority level. */
};

/**
 * \brief Pipeline operational state machine values.
 * \ingroup ipc4_pipeline_api
 */
enum ipc4_pipeline_state {
	SOF_IPC4_PIPELINE_STATE_INVALID = 0,       /**< Invalid or uninitialized pipeline state. */
	SOF_IPC4_PIPELINE_STATE_UNINITIALIZED = 1, /**< Created but module initialization is incomplete. */
	SOF_IPC4_PIPELINE_STATE_RESET = 2,         /**< Reset state: buffers cleared, hardware inactive. */
	SOF_IPC4_PIPELINE_STATE_PAUSED = 3,        /**< Paused state: buffers pre-allocated, clocks armed. */
	SOF_IPC4_PIPELINE_STATE_RUNNING = 4,       /**< Running state: audio streaming actively scheduled. */
	SOF_IPC4_PIPELINE_STATE_EOS = 5,           /**< End Of Stream (EOS): draining final buffered audio. */
	SOF_IPC4_PIPELINE_STATE_ERROR_STOP,        /**< Pipeline halted due to unrecoverable DSP error. */
	SOF_IPC4_PIPELINE_STATE_SAVED              /**< Pipeline context serialized to host memory for D3. */
};

/**
 * \brief Extended pipeline object type identifiers.
 * \ingroup ipc4_pipeline_api
 */
enum ipc4_pipeline_ext_obj_id {
	IPC4_GLB_PIPE_EXT_OBJ_ID_INVALID = 0,     /**< Invalid extension object identifier. */
	IPC4_GLB_PIPE_EXT_OBJ_ID_MEM_DATA = 1,    /**< Pipeline memory stack/heap allocation parameters. */
	IPC4_GLB_PIPE_EXT_OBJ_ID_MAX = IPC4_GLB_PIPE_EXT_OBJ_ID_MEM_DATA, /**< Maximum extension object ID. */
};

/**
 * \brief Header for extended pipeline initialization objects.
 * \ingroup ipc4_pipeline_api
 */
struct ipc4_pipeline_ext_object {
	uint32_t last_object : 1;	/**< 1 if terminating object in array, 0 if another object follows. */
	uint32_t object_id : 15;	/**< Unique object identifier (enum ipc4_pipeline_ext_obj_id). */
	uint32_t object_words : 16;	/**< Payload size in 32-bit DWORDs following this header. */
} __packed __aligned(4);

/**
 * \brief Memory allocation parameters for pipeline execution context.
 * \ingroup ipc4_pipeline_api
 */
struct ipc4_pipeline_ext_obj_mem_data {
	uint32_t domain_id;		/**< Userspace domain identifier. */
	uint32_t stack_bytes;		/**< Stack size in bytes required for pipeline thread. */
	uint32_t heap_bytes;		/**< Heap memory size in bytes required for pipeline graph. */
} __packed __aligned(4);

/**
 * \brief Container for extended pipeline creation payload.
 * \ingroup ipc4_pipeline_api
 */
struct ipc4_pipeline_ext_payload {
	uint32_t payload_words : 24;	/**< Total payload size in 32-bit DWORDs. */
	uint32_t data_obj_array : 1;	/**< 1 if struct ipc4_pipeline_ext_object array follows. */
	uint32_t rsvd0		: 7;	/**< Reserved bits. */
	uint32_t rsvd1;			/**< Reserved word. */
	uint32_t rsvd2;			/**< Reserved word. */
} __packed __aligned(4);

/**
 * \brief Pipeline creation command structure (SOF_IPC4_GLB_CREATE_PIPELINE).
 * \ingroup ipc4_pipeline_api
 */
struct ipc4_pipeline_create {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			uint32_t ppl_mem_size   : 11; /**< Number of 4KB memory pages allocated for pipeline. */
			uint32_t ppl_priority   : 5;  /**< Scheduling priority (enum ipc4_pipeline_priority). */
			uint32_t instance_id    : 8;  /**< Pipeline instance identifier (0..max_ppl-1). */
			uint32_t type           : 5;  /**< Opcode: SOF_IPC4_GLB_CREATE_PIPELINE (17). */
			uint32_t rsp            : 1;  /**< 0 for request. */
			uint32_t msg_tgt        : 1;  /**< 0 for global firmware message. */
			uint32_t _reserved_0    : 1;  /**< Reserved bit. */
		} r;
	} primary;

	union{
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			uint32_t lp             : 1;  /**< 1 = low power pipeline (kept running during D0i3). */
			uint32_t rsvd1          : 3;  /**< Reserved bits. */
			uint32_t attributes     : 16; /**< Pipeline hardware attributes bitmask. */
			uint32_t core_id        : 4;  /**< Target DSP core ID hosting this pipeline. */
			uint32_t rsvd2          : 5;  /**< Reserved bits. */
			uint32_t payload	: 1;  /**< 1 if struct ipc4_pipeline_ext_payload follows. */
			uint32_t _reserved_2    : 2;  /**< Reserved bits. */
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/**
 * \brief Pipeline deletion command structure (SOF_IPC4_GLB_DELETE_PIPELINE).
 * \ingroup ipc4_pipeline_api
 *
 * Deletes a pipeline and all its associated child module instances from DSP memory.
 * All external cross-pipeline bindings must be severed prior to issuing delete.
 */
struct ipc4_pipeline_delete {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			uint32_t rsvd0          : 16; /**< Reserved padding. */
			uint32_t instance_id    : 8;  /**< Pipeline instance identifier to delete. */
			uint32_t type           : 5;  /**< Opcode: SOF_IPC4_GLB_DELETE_PIPELINE (18). */
			uint32_t rsp            : 1;  /**< 0 for request. */
			uint32_t msg_tgt        : 1;  /**< 0 for global firmware message. */
			uint32_t _reserved_0    : 1;  /**< Reserved bit. */
		} r;
	} primary;

	union{
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			uint32_t rsvd1          : 30; /**< Reserved padding. */
			uint32_t _reserved_2    : 2;  /**< Reserved bits. */
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/*!
 * Host SW sends this message to set a pipeline to the specified state.
 *
 * If there are multiple pipelines, from FW input to FW output, connected
 * in a data processing stream, the driver should start them in reverse order,
 * beginning with pipeline connected to the output gateway, in order to avoid
 * overruns (FW protects the output gateway against underruns in this scenario).
 *
 * If driver starts multiple pipelines using a single Set Pipeline State
 * command, it should take care of the pipeline ID order in the command payload
 * to follow the above rule.
 *
 * sync_stop_start indicates whether all specified pipelines' gateways should
 * be started with a minimal delay possible. If this flag is set to 0 while
 * multiple pipelines are specified, the target pipelines' state is adjusted
 * pipeline by pipeline meaning that internal propagation to all child modules
 * may take more time between reaching state of attached gateways. Output and
 * input gateways are grouped separately, and started/stopped separately.
 *
 * NOTE: Task Creation/Registration is part of the first state transition.
 * There is no other dedicated call for this.
 *
 * \remark hide_methods
 */
/**
 * \brief Payload array containing pipeline IDs for multi-pipeline state transitions.
 * \ingroup ipc4_pipeline_api
 */
struct ipc4_pipeline_set_state_data {
	uint32_t pipelines_count; /**< Number of pipeline instance IDs in ppl_id[]. */
	uint32_t ppl_id[];        /**< Array of pipeline instance identifiers. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Pipeline state transition command structure (SOF_IPC4_GLB_SET_PIPELINE_STATE).
 * \ingroup ipc4_pipeline_api
 *
 * Directs one or more pipelines to transition their operational state (e.g. RESET -> PAUSED -> RUNNING).
 * For multi-pipeline audio chains, drivers must start pipelines in reverse topological order
 * (from output gateway backwards to input) to prevent intermediate buffer overruns.
 */
struct ipc4_pipeline_set_state {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			/** Target pipeline state (enum ipc4_pipeline_state). */
			uint32_t ppl_state  : 16;
			/** Target pipeline ID (ignored if multi_ppl == 1). */
			uint32_t ppl_id     : 8;
			/** Opcode: SOF_IPC4_GLB_SET_PIPELINE_STATE (19). */
			uint32_t type       : 5;
			/** Direction: 0 for request. */
			uint32_t rsp        : 1;
			/** Target: 0 for global firmware message. */
			uint32_t msg_tgt    : 1;
			uint32_t _reserved_0: 1; /**< Reserved bit. */
		} r;
	} primary;

	union{
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			/** 1 if additional pipeline IDs are provided in s_data payload. */
			uint32_t multi_ppl  : 1;
			/** 1 if DSP should synchronize state transition across all pipelines. */
			uint32_t sync_stop_start : 1;
			uint32_t rsvd1      : 28; /**< Reserved padding. */
			uint32_t _reserved_2: 2;  /**< Reserved bits. */
		} r;
	} extension;

	struct ipc4_pipeline_set_state_data s_data; /**< Optional multi-pipeline ID list. */
} __attribute__((packed, aligned(4)));

/**
 * \brief Reply structure for pipeline state transition.
 * \ingroup ipc4_pipeline_api
 */
struct ipc4_pipeline_set_state_reply {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			uint32_t status     : IPC4_IXC_STATUS_BITS; /**< Status code (enum ipc4_status). */
			uint32_t type       : 5;  /**< Opcode: SOF_IPC4_GLB_SET_PIPELINE_STATE. */
			uint32_t rsp        : 1;  /**< 1 for reply. */
			uint32_t msg_tgt    : 1;  /**< 0 for global message. */
			uint32_t _reserved_0: 1;  /**< Reserved bit. */
		} r;
	} primary;

	union{
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			/** ID of failed pipeline on error. */
			uint32_t ppl_id     : 30;
			uint32_t _reserved_2: 2;  /**< Reserved bits. */
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/**
 * \brief Query pipeline current state command (SOF_IPC4_GLB_GET_PIPELINE_STATE).
 * \ingroup ipc4_pipeline_api
 */
struct ipc4_pipeline_get_state {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			uint32_t ppl_id     : 8;  /**< Target pipeline instance identifier. */
			uint32_t rsvd       : 16; /**< Reserved padding. */
			uint32_t type       : 5;  /**< Opcode: SOF_IPC4_GLB_GET_PIPELINE_STATE (20). */
			uint32_t rsp        : 1;  /**< 0 for request. */
			uint32_t msg_tgt    : 1;  /**< 0 for global message. */
			uint32_t _reserved_0: 1;  /**< Reserved bit. */
		} r;
	} primary;

	union{
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			uint32_t rsvd1      : 30; /**< Reserved padding. */
			uint32_t _reserved_2: 2;  /**< Reserved bits. */
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/**
 * \brief Reply structure containing pipeline current operational state.
 * \ingroup ipc4_pipeline_api
 */
struct ipc4_pipeline_get_state_reply {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			uint32_t status      : IPC4_IXC_STATUS_BITS; /**< Status code (enum ipc4_status). */
			uint32_t type       : 5;  /**< Opcode: SOF_IPC4_GLB_GET_PIPELINE_STATE. */
			uint32_t rsp        : 1;  /**< 1 for reply. */
			uint32_t msg_tgt    : 1;  /**< 0 for global message. */
			uint32_t _reserved_0: 1;  /**< Reserved bit. */
		} r;
	} primary;

	union{
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			/** Current state (one of enum ipc4_pipeline_state). */
			uint32_t state      : 5;
			uint32_t rsvd1      : 25; /**< Reserved padding. */
			uint32_t _reserved_2: 2;  /**< Reserved bits. */
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/**
 * \brief Query memory context size required to serialize pipeline (SOF_IPC4_GLB_GET_PIPELINE_CONTEXT_SIZE).
 * \ingroup ipc4_pipeline_api
 */
struct ipc4_pipeline_get_context_size {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			uint32_t rsvd0          : 16; /**< Reserved padding. */
			uint32_t instance_id    : 8;  /**< Target pipeline instance ID. */
			uint32_t type           : 5;  /**< Opcode: SOF_IPC4_GLB_GET_PIPELINE_CONTEXT_SIZE (21). */
			uint32_t rsp            : 1;  /**< 0 for request. */
			uint32_t msg_tgt        : 1;  /**< 0 for global message. */
			uint32_t _reserved_0    : 1;  /**< Reserved bit. */
		} r;
	} primary;

	union{
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			uint32_t rsvd1          : 30; /**< Reserved padding. */
			uint32_t _reserved_2    : 2;  /**< Reserved bits. */
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/**
 * \brief Reply structure for pipeline context size query.
 * \ingroup ipc4_pipeline_api
 */
struct ipc4_pipeline_get_context_size_reply {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			uint32_t status : IPC4_IXC_STATUS_BITS; /**< Status code (enum ipc4_status). */
			uint32_t type       : 5;  /**< Opcode: SOF_IPC4_GLB_GET_PIPELINE_CONTEXT_SIZE. */
			uint32_t rsp        : 1;  /**< 1 for reply. */
			uint32_t msg_tgt    : 1;  /**< 0 for global message. */
			uint32_t _reserved_0: 1;  /**< Reserved bit. */
		} r;
	} primary;

	union{
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			/** Size of pipeline serialization context in 4KB memory pages. */
			uint32_t ctx_size   : 16;
			uint32_t rsvd1      : 14; /**< Reserved padding. */
			uint32_t _reserved_2: 2;  /**< Reserved bits. */
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/**
 * \brief Direct DMA Chaining configuration command (SOF_IPC4_GLB_CHAIN_DMA).
 * \ingroup ipc4_pipeline_api
 */
struct ipc4_chain_dma {
	union {
		uint32_t dat; /**< Raw 32-bit Primary register value. */

		struct {
			uint32_t host_dma_id		: 5; /**< Host DMA channel identifier. */
			uint32_t rsvd4			: 3; /**< Reserved bits. */
			uint32_t link_dma_id		: 5; /**< Link DMA channel identifier. */
			uint32_t rsvd3			: 3; /**< Reserved bits. */
			uint32_t allocate		: 1; /**< Allocate FIFO buffer. */
			uint32_t enable			: 1; /**< Enable DMA chaining. */
			uint32_t scs			: 1; /**< Select stream context (SCS) bit in gateways. */
			uint32_t rsvd2			: 5; /**< Reserved bits. */
			uint32_t type			: 5; /**< Opcode: SOF_IPC4_GLB_CHAIN_DMA (14). */
			uint32_t rsp			: 1; /**< 0 for request. */
			uint32_t msg_tgt		: 1; /**< 0 for global message. */
			uint32_t _reserved_0		: 1; /**< Reserved bit. */
		} r;
	} primary;

	union {
		uint32_t dat; /**< Raw 32-bit Extension register value. */

		struct {
			uint32_t fifo_size		: 24; /**< Size of chaining FIFO buffer in bytes. */
			uint32_t rsvd1			: 6;  /**< Reserved bits. */
			uint32_t _reserved_2		: 2;  /**< Reserved bits. */
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

#endif
