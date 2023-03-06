/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifndef _ADSP_ERROR_CODE_H_
#define _ADSP_ERROR_CODE_H_

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

#endif /* _ADSP_ERROR_CODE_H_ */
