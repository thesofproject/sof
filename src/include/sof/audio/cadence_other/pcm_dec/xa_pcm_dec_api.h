/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 */

#ifndef __XA_PCM_DEC_API_H__
#define __XA_PCM_DEC_API_H__

/*****************************************************************************/
/* PCM Decoder specific API definitions                                     */
/*****************************************************************************/

/* pcm_dec-specific configuration parameters */
enum xa_config_param_pcm_dec {
	XA_PCM_DEC_CONFIG_PARAM_CHANNELS = 0,
	XA_PCM_DEC_CONFIG_PARAM_SAMPLE_RATE = 1,
	XA_PCM_DEC_CONFIG_PARAM_PCM_WIDTH = 2,
	XA_PCM_DEC_CONFIG_PARAM_PRODUCED = 3,
	XA_PCM_DEC_CONFIG_PARAM_INTERLEAVE = 5
};

/* commands */
#include "xa_apicmd_standards.h"

/* error codes */
#include "xa_error_standards.h"

#define XA_CODEC_PCM_DEC 15

/* pcm_dec-specific error codes */
/*****************************************************************************/
/* Class 1: Configuration Errors                                             */
/*****************************************************************************/
/* Nonfatal Errors */
enum xa_error_nonfatal_config_pcm_dec {
	XA_PCMDEC_CONFIG_NONFATAL_INVALID_PCM_WIDTH =
	    XA_ERROR_CODE(xa_severity_nonfatal, xa_class_config, XA_CODEC_PCM_DEC, 2)
};

/*****************************************************************************/
/* Class 2: Execution Errors                                                 */
/*****************************************************************************/
/* Nonfatal Errors */
enum xa_error_nonfatal_execute_pcm_dec {
	XA_PCMDEC_EXECUTE_NONFATAL_INSUFFICIENT_DATA =
	    XA_ERROR_CODE(xa_severity_nonfatal, xa_class_execute, XA_CODEC_PCM_DEC, 0)
};

/* Fatal Errors */
#define XA_PCMDEC_EXECUTE_FATAL_UNINITIALIZED \
	XA_ERROR_CODE((uint32_t)xa_severity_fatal, xa_class_execute, XA_CODEC_PCM_DEC, 0)

#endif /* __XA_PCM_DEC_API_H__ */
