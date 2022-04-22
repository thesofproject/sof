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
 * \brief IPC4 pipeline definitions
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_PIPELINE_H__
#define __SOF_IPC4_PIPELINE_H__

#include <stdint.h>
#include <ipc4/error_status.h>

/**< Pipeline priority */
enum ipc4_pipeline_priority {
	/**< Priority 0 (run first) */
	SOF_IPC4_PIPELINE_PRIORITY_0 = 0,
	/**< Priority 1 */
	SOF_IPC4_PIPELINE_PRIORITY_1,
	/**< Priority 2 */
	SOF_IPC4_PIPELINE_PRIORITY_2,
	/**< Priority 3 */
	SOF_IPC4_PIPELINE_PRIORITY_3,
	/**< Priority 4 */
	SOF_IPC4_PIPELINE_PRIORITY_4,
	/**< Priority 5 */
	SOF_IPC4_PIPELINE_PRIORITY_5,
	/**< Priority 6 */
	SOF_IPC4_PIPELINE_PRIORITY_6,
	/**< Priority 7 */
	SOF_IPC4_PIPELINE_PRIORITY_7,
	/**< Max (and lowest) priority */
	SOF_IPC4_MAX_PIPELINE_PRIORITY = SOF_IPC4_PIPELINE_PRIORITY_7
};

/**< Pipeline State */
enum ipc4_pipeline_state {
	/**< Invalid value */
	SOF_IPC4_PIPELINE_STATE_INVALID = 0,
	/**< Created but initialization incomplete */
	SOF_IPC4_PIPELINE_STATE_UNINITIALIZED = 1,
	/**< Resets pipeline */
	SOF_IPC4_PIPELINE_STATE_RESET = 2,
	/**< Pauses pipeline */
	SOF_IPC4_PIPELINE_STATE_PAUSED = 3,
	/**< Starts pipeline */
	SOF_IPC4_PIPELINE_STATE_RUNNING = 4,
	/**< Marks pipeline as expecting End Of Stream */
	SOF_IPC4_PIPELINE_STATE_EOS = 5,
	/**< Stopped on error */
	SOF_IPC4_PIPELINE_STATE_ERROR_STOP,
	/**< Saved to the host memory */
	SOF_IPC4_PIPELINE_STATE_SAVED
};

/*!
 * lp - indicates whether the pipeline should be kept on running in low power
 * mode. On BXT the driver should set this flag to 1 for WoV pipeline.
 *
 * \remark hide_methods
 */
struct ipc4_pipeline_create {

	union {
		uint32_t dat;

		struct {
			/**< # pages for pipeline */
			uint32_t ppl_mem_size   : 11;
			/**< priority - uses enum ipc4_pipeline_priority */
			uint32_t ppl_priority   : 5;
			/**< pipeline id */
			uint32_t instance_id    : 8;
			/**< Global::CREATE_PIPELINE */
			uint32_t type           : 5;
			/**< Msg::MSG_REQUEST */
			uint32_t rsp            : 1;
			/**< Msg::FW_GEN_MSG */
			uint32_t msg_tgt        : 1;
			uint32_t _reserved_0    : 1;
		} r;
	} primary;

	union{
		uint32_t dat;

		struct {
			/**< 1 - is low power */
			uint32_t lp             : 1;
			uint32_t rsvd1          : 3;
			uint32_t attributes     : 16;
			uint32_t rsvd2          : 10;
			uint32_t _reserved_2    : 2;
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/*!
 * SW Driver sends this IPC message to delete a pipeline from ADSP memory.
 *
 * All module instances and tasks that are associated with the pipeline are
 * deleted too.
 *
 * There must be no existing binding from any pipeline's module instance
 * to another pipeline to complete the command successfully.
 *
 * \remark hide_methods
 */
struct ipc4_pipeline_delete {
	union {
		uint32_t dat;

		struct {
			uint32_t rsvd0          : 16;
			/**< Ppl instance id */
			uint32_t instance_id    : 8;
			/**< Global::DELETE_PIPELINE */
			uint32_t type           : 5;
			/**< Msg::MSG_REQUEST */
			uint32_t rsp            : 1;
			/**< Msg::FW_GEN_MSG */
			uint32_t msg_tgt        : 1;
			uint32_t _reserved_0    : 1;
		} r;
	} primary;

	union{
		uint32_t dat;

		struct {
			uint32_t rsvd1          : 30;
			uint32_t _reserved_2    : 2;
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
struct ipc4_pipeline_set_state_data {
	/**< Number of items in ppl_id[] */
	uint32_t pipelines_count;
	/**< Pipeline ids */
	uint32_t ppl_id[0];
} __attribute__((packed, aligned(4)));

struct ipc4_pipeline_set_state {
	union {
		uint32_t dat;

		struct {
			/**< new state, one of enum ipc4_pipeline_state */
			uint32_t ppl_state  : 16;
			/**< pipeline instance id (ignored if multi_ppl =1) */
			uint32_t ppl_id     : 8;
			/**< Global::SET_PIPELINE_STATE */
			uint32_t type       : 5;
			/**< Msg::MSG_REQUEST */
			uint32_t rsp        : 1;

			/**< Msg::FW_GEN_MSG */
			uint32_t msg_tgt    : 1;
			uint32_t _reserved_0: 1;
		} r;
	} primary;

	union{
		uint32_t dat;

		struct {
			/**< = 1 if there are more pipeline ids in payload */
			uint32_t multi_ppl  : 1;
			/**< = 1 if FW should sync state change across multiple ppls */
			uint32_t sync_stop_start : 1;
			uint32_t rsvd1      : 28;
			uint32_t _reserved_2: 2;
		} r;
	} extension;

	/* multiple pipeline states */
	struct ipc4_pipeline_set_state_data s_data;
} __attribute__((packed, aligned(4)));

/**< Reply to Set Pipeline State */
/*!
 * In case of error, there is failed pipeline id reported back.
 *
  \remark hide_methods
*/
struct ipc4_pipeline_set_state_reply {
	union {
		uint32_t dat;

		struct {
			/**< status */
			uint32_t status     : IPC4_IXC_STATUS_BITS;
			/**< Global::SET_PIPELINE_STATE */
			uint32_t type       : 5;
			/**< Msg::MSG_REPLY */
			uint32_t rsp        : 1;
			/**< Msg::FW_GEN_MSG */
			uint32_t msg_tgt    : 1;
			uint32_t _reserved_0: 1;
		} r;
	} primary;

	union{
		uint32_t dat;

		struct {
			/**< id of failed pipeline on error */
			uint32_t ppl_id     : 30;
			uint32_t _reserved_2: 2;
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/**< This IPC message is sent to the FW in order to retrieve a pipeline state. */
/*! \remark hide_methods */
struct ipc4_pipeline_get_state {
	union {
		uint32_t dat;

		struct {
			/**< pipeline id */
			uint32_t ppl_id     : 8;
			uint32_t rsvd       : 16;
			/**< Global::GET_PIPELINE_STATE */
			uint32_t type       : 5;
			/**< Msg::MSG_REQUEST */
			uint32_t rsp        : 1;
			/**< Msg::FW_GEN_MSG */
			uint32_t msg_tgt    : 1;
			uint32_t _reserved_0: 1;
			} r;
	} primary;

	union{
		uint32_t dat;

		struct {
			uint32_t rsvd1      : 30;
			uint32_t _reserved_2: 2;
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/**< Sent by the FW in response to GetPipelineState. */
/*! \remark hide_methods */
struct ipc4_pipeline_get_state_reply {
	union {
		uint32_t dat;

		struct {
			/**< status */
			uint32_t status      :IPC4_IXC_STATUS_BITS;
			/**< Global::GET_PIPELINE_STATE */
			uint32_t type       : 5;
			/**< Msg::MSG_REPLY */
			uint32_t rsp        : 1;
			/**< Msg::FW_GEN_MSG */
			uint32_t msg_tgt    : 1;
			uint32_t _reserved_0: 1;
		} r;
	} primary;

	union{
		uint32_t dat;

		struct {
			/**< one of PipelineState */
			uint32_t state      : 5;
			uint32_t rsvd1      : 25;
			uint32_t _reserved_2: 2;
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/*!
 * The size is expressed in number of pages. It is a total number of memory pages
 * allocated for pipeline memory buffer and all separately allocated child
 * module instances.
 *
 *  \remark hide_methods
 */
struct ipc4_pipeline_get_context_size {
	union {
		uint32_t dat;

		struct {
			uint32_t rsvd0          : 16;
			/**< pipeline id */
			uint32_t instance_id    : 8;
			/**< Global::GET_PIPELINE_CONTEXT_SIZE */
			uint32_t type           : 5;
			/**< Msg::MSG_REQUEST */
			uint32_t rsp            : 1;
			/**< Msg::FW_GEN_MSG */
			uint32_t msg_tgt        : 1;
			uint32_t _reserved_0    : 1;
		} r;
	} primary;

	union{
		uint32_t dat;

		struct {
			uint32_t rsvd1          : 30;
			uint32_t _reserved_2    : 2;
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

/**< Reply to Get Pipeline Context Size. */
/*! \remark hide_methods */
struct ipc4_pipeline_get_context_size_reply {
	union {
		uint32_t dat;

		struct {
			/**< status */
			uint32_t status :IPC4_IXC_STATUS_BITS;
			/**< Global::GET_PIPELINE_CONTEXT_SIZE */
			uint32_t type       : 5;
			/**< Msg::MSG_REPLY */
			uint32_t rsp        : 1;
			/**< Msg::FW_GEN_MSG */
			uint32_t msg_tgt    : 1;
			uint32_t _reserved_0: 1;
		} r;
	} primary;

	union{
		uint32_t dat;

		struct {
			/**< size of pipeline context (in number of pages) */
			uint32_t ctx_size   : 16;
			uint32_t rsvd1      : 14;
			uint32_t _reserved_2: 2;
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

struct ipc4_chain_dma {
	union {
		uint32_t dat;

		struct {
		uint32_t host_dma_id		: 5;
		uint32_t rsvd4		: 3;
		uint32_t link_dma_id		: 5;
		uint32_t rsvd3		: 3;
		/* allocate buffer specified by FIFO size */
		uint32_t allocate		: 1;
		uint32_t enable			: 1;
		/* controls SCS bit in both Host and Link gateway */
		uint32_t scs		: 1;
		uint32_t rsvd2		: 5;
		/* Global::CHAIN_DMA */
		uint32_t type		: 5;
		/* Msg::MSG_REQUEST */
		uint32_t rsp		: 1;
		/* Msg::FW_GEN_MSG */
		uint32_t msg_tgt		: 1;
		uint32_t _reserved_0		: 1;
		} r;
	} primary;

	union {
		uint32_t dat;

		struct {
			/* size of FIFO (bytes) */
			uint32_t fifo_size		: 24;
			uint32_t rsvd1		: 6;
			uint32_t _reserved_2		: 2;
		} r;
	} extension;
} __attribute__((packed, aligned(4)));

#endif
