/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 - 2023 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Warecki <adrian.warecki@intel.com>
 */

#ifndef __MODULE_IADK_ADSP_ERROR_CODE_H__
#define __MODULE_IADK_ADSP_ERROR_CODE_H__

#include <stdint.h>

/**
 * Defines error codes that are returned in the ADSP project.
 * NOTE: intel_adsp::ErrorCode should be merged into this namespaceless
 * type to be used in 3rd party's C code.
 */
typedef uint32_t AdspErrorCode;

 /* Reports no error */
#define ADSP_NO_ERROR 0
/* Reports that some parameters passed to the method are invalid */
#define ADSP_INVALID_PARAMETERS 1
/* Reports that the system or resource is busy. */
#define ADSP_BUSY_RESOURCE 4
/**
 * Module has detected some unexpected critical situation (e.g. memory corruption).
 * Upon this error code the ADSP System is asked to stop any interactions with the module
 * instance.
 */
#define ADSP_FATAL_FAILURE 6
/* Report out of memory. */
#define ADSP_OUT_OF_MEMORY 15
/* Report invalid target. */
#define ADSP_INVALID_TARGET 142
/* Service is not supported on target platform. */
#define ADSP_SERVICE_UNAVAILABLE 143

/* SystemAgentInterface */
#define ADSP_MODULE_CREATION_FAILURE	16

/* ProcessingModuleFactoryInterface */

/* Reports that the given value of Input Buffer Size is invalid */
#define ADSP_INVALID_IBS		17
/* Reports that the given value of Output Buffer Size is invalid */
#define ADSP_INVALID_OBS		18
/* Reports that the given value of Cycles Per Chunk processing is invalid */
#define ADSP_INVALID_CPC		19
/* Reports that the settings provided for module creation are invalid */
#define ADSP_INVALID_SETTINGS		20

/* ProcessingModuleInterface */
/* Reports that the message content given for configuration is invalid */
#define ADSP_INVALID_CONFIGURATION	21

/* Reports that the module does not support retrieval of its current configuration information */
#define ADSP_NO_CONFIGURATION		22

#endif /* __MODULE_IADK_ADSP_ERROR_CODE_H__ */
