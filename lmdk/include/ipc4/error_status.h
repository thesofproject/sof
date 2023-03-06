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
 * \file include/ipc4/error_status.h
 * \brief IPC4 global definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_ERROR_STATUS_H__
#define __SOF_IPC4_ERROR_STATUS_H__

#define IPC4_IXC_STATUS_BITS	24

enum ipc4_status {
	/**< The operation was successful */
	IPC4_SUCCESS = 0,

	/**< Invalid parameter specified */
	IPC4_ERROR_INVALID_PARAM = 1,
	/**< Unknown message type specified */
	IPC4_UNKNOWN_MESSAGE_TYPE = 2,
	/**< Not enough space in the IPC reply buffer to complete the request */
	IPC4_OUT_OF_MEMORY = 3,

	/**< The system or resource is busy */
	IPC4_BUSY = 4,
	/**< Replaced ADSP IPC PENDING (unused) - according to cAVS v0.5 */
	IPC4_BAD_STATE = 5,
	/**< Unknown error while processing the request */
	IPC4_FAILURE = 6,
	/**< Unsupported operation requested */
	IPC4_INVALID_REQUEST = 7,

	/**< Specified resource not found */
	IPC4_INVALID_RESOURCE_ID = 9,
	/**< A resource's ID requested to be created is already assigned */
	IPC4_RESOURCE_ID_EXISTS = 10,

	/**< Required resource is in invalid state */
	IPC4_INVALID_RESOURCE_STATE = 12,

	/**< Requested power transition failed to complete */
	IPC4_POWER_TRANSITION_FAILED = 13,

	/**< Manifest of the library being loaded is invalid */
	IPC4_INVALID_MANIFEST = 14,

	/**< Requested service or data is unavailable on the target platform */
	IPC4_UNAVAILABLE = 15,

	/* Load/unload library specific codes */

	/**< Library target address is out of storage memory range */
	IPC4_LOAD_ADDRESS_OUT_OF_RANGE = 42,

	/**< Image verification by CSE failed */
	IPC4_CSE_VALIDATION_FAILED = 44,

	/**< General module management error */
	IPC4_MOD_MGMT_ERROR = 100,
	/**< Module loading failed */
	IPC4_MOD_LOAD_CL_FAILED = 101,
	/**< Integrity check of the loaded module content failed */
	IPC4_MOD_LOAD_INVALID_HASH = 102,

	/**< Attempt to unload code of the module in use */
	IPC4_MOD_INSTANCE_EXISTS = 103,
	/**< Other failure of module instance initialization request */
	IPC4_MOD_NOT_INITIALIZED = 104,

	/**< Invalid (out of range) module ID provided */
	IPC4_MOD_INVALID_ID  = 110,
	/**< Invalid module instance ID provided */
	IPC4_MOD_INST_INVALID_ID  = 111,
	/**< Invalid queue (pin) ID provided */
	IPC4_QUEUE_INVALID_ID = 112,
	/**< Invalid destination queue (pin) ID provided */
	IPC4_QUEUE_DST_INVALID_ID = 113,

	/**< Invalid target code ID provided */
	IPC4_INVALID_CORE_ID = 116,

	/**< Injection DMA buffer is too small for probing the input pin */
	IPC4_PROBE_DMA_INJECTION_BUFFER_TOO_SMALL = 117,
	/**< Extraction DMA buffer is too small for probing the output pin */
	IPC4_PROBE_DMA_EXTRACTION_BUFFER_TOO_SMALL = 118,

	/**< Invalid ID of configuration item provided in TLV list */
	IPC4_INVALID_CONFIG_PARAM_ID = 120,
	/**< Invalid length of configuration item provided in TLV list */
	IPC4_INVALID_CONFIG_DATA_LEN = 121,
	/**< Invalid structure of configuration item provided */
	IPC4_INVALID_CONFIG_DATA_STRUCT = 122,

	/**< Initialization of DMA Gateway failed */
	IPC4_GATEWAY_NOT_INITIALIZED = 140,
	/**< Invalid ID of gateway provided */
	IPC4_GATEWAY_NOT_EXIST = 141,
	/**< Setting state of DMA Gateway failed */
	IPC4_GATEWAY_STATE_NOT_SET = 142,
	/**< DMA_CONTROL message targeting gateway not allocated yet */
	IPC4_GATEWAY_NOT_ALLOCATED = 143,

	/**< Attempt to configure SCLK while I2S port is running */
	IPC4_SCLK_ALREADY_RUNNING = 150,
	/**< Attempt to configure MCLK while I2S port is running */
	IPC4_MCLK_ALREADY_RUNNING = 151,
	/**< Attempt to stop SCLK that is not running */
	IPC4_NO_RUNNING_SCLK = 152,
	/**< Attempt to stop MCLK that is not running */
	IPC4_NO_RUNNING_MCLK = 153,

	/**< Reverted for ULP purposes */
	IPC4_PIPELINE_STATE_NOT_SET = 164,

	/**< HDA chained gateways wrong state transition*/
	IPC4_INVALID_CHAIN_STATE_TRANSITION = 3018,

	IPC4_MAX_STATUS = ((1 << IPC4_IXC_STATUS_BITS) - 1)
};
#endif
