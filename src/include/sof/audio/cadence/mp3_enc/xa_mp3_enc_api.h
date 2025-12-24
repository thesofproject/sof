/*
 * Copyright (c) 2022-2025 Cadence Design Systems, Inc.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */



#ifndef __XA_MP3ENC_CONFIG_PARAMS_H__
#define __XA_MP3ENC_CONFIG_PARAMS_H__

/* mp3_enc-specific configuration parameters */
enum xa_config_param_mp3_enc {
  XA_MP3ENC_CONFIG_PARAM_PCM_WDSZ     = 0,
  XA_MP3ENC_CONFIG_PARAM_SAMP_FREQ    = 1,
  XA_MP3ENC_CONFIG_PARAM_NUM_CHANNELS = 2,
  XA_MP3ENC_CONFIG_PARAM_BITRATE      = 3
#ifdef ENABLE_CUT_OFF_FREQ_CONFIG
  , XA_MP3ENC_CONFIG_FATAL_FRAC_BANDWIDTH = 4
#endif // ENABLE_CUT_OFF_FREQ_CONFIG
};

/* commands */
#include "xa_apicmd_standards.h"

/* mp3_enc-specific commands */
/* (none) */

/* mp3_enc-specific command types */
/* (none) */

/* error codes */
#include "xa_error_standards.h"

#define	XA_CODEC_MP3_ENC	2

/* mp3_enc-specific error_codes */
/*****************************************************************************/
/* Class 0: API Errors                                                       */
/*****************************************************************************/
/* Nonfatal Errors */
/* (none) */
/* Fatal Errors */
/* (none) */

/*****************************************************************************/
/* Class 1: Configuration Errors                                             */
/*****************************************************************************/
/* Nonfatal Errors */
enum xa_error_nonfatal_config_mp3_enc {
  XA_MP3ENC_CONFIG_NONFATAL_INVALID_BITRATE = XA_ERROR_CODE(xa_severity_nonfatal, xa_class_config, XA_CODEC_MP3_ENC, 0)
};

/* Fatal Errors */
enum xa_error_fatal_config_mp3_enc {
  XA_MP3ENC_CONFIG_FATAL_SAMP_FREQ    = XA_ERROR_CODE(xa_severity_fatal, xa_class_config, XA_CODEC_MP3_ENC, 0),
  XA_MP3ENC_CONFIG_FATAL_NUM_CHANNELS = XA_ERROR_CODE(xa_severity_fatal, xa_class_config, XA_CODEC_MP3_ENC, 1),
  XA_MP3ENC_CONFIG_FATAL_PCM_WDSZ     = XA_ERROR_CODE(xa_severity_fatal, xa_class_config, XA_CODEC_MP3_ENC, 2)
#ifdef ENABLE_CUT_OFF_FREQ_CONFIG
  , XA_MP3ENC_CONFIG_PARAM_FRAC_BANDWIDTH   = XA_ERROR_CODE(xa_severity_fatal, xa_class_config, XA_CODEC_MP3_ENC, 3)
#endif // ENABLE_CUT_OFF_FREQ_CONFIG
};
/* (none) */

#include "xa_type_def.h"

#if defined(__cplusplus)
extern "C" {
#endif	/* __cplusplus */
xa_codec_func_t xa_mp3_enc;
#if defined(__cplusplus)
}
#endif	/* __cplusplus */
#endif /* __XA_MP3ENC_CONFIG_PARAMS_H__ */
