/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intelligo Technology Inc. All rights reserved.
 *
 * Author: Fu-Yun TSUO <fy.tsuo@intelli-go.com>
 */

/*******************************************************************************
 * [2017] - [2021] Copyright (c) Intelligo Technology Inc.
 *
 * This unpublished material is proprietary to Intelligo Technology Inc.
 * All rights reserved. The methods and techniques described herein are
 * considered trade secrets and/or confidential. Reproduction or
 * distribution, in whole or in part, is forbidden except by express written
 * permission of Intelligo Technology Inc.
 *
 *******************************************************************************/

#ifndef _IGO_LIB_H_
#define _IGO_LIB_H_

#include <stdint.h>

enum IgoRet {
	IGO_RET_OK = 0,
	IGO_RET_ERR,
	IGO_RET_NO_SERVICE,
	IGO_RET_INVL_ARG,
	IGO_RET_NO_MEMORY,
	IGO_RET_NOT_SUPPORT,
	IGO_RET_ALGO_NAME_NOT_FOUND,
	IGO_RET_CH_NUM_ERR,
	IGO_RET_SAMPLING_RATE_NOT_SUPPORT,
	IGO_RET_IN_DATA_ERR,
	IGO_RET_REF_DATA_ERR,
	IGO_RET_OUT_DATA_ERR,
	IGO_RET_PARAM_NOT_FOUND,
	IGO_RET_PARAM_READ_ONLY,
	IGO_RET_PARAM_WRITE_ONLY,
	IGO_RET_PARAM_INVALID_VAL,
	IGO_RET_LAST
};

enum IgoDataWidth {
	IGO_DATA_16BIT = 0,
	IGO_DATA_24BIT,
	IGO_DATA_LAST
};

/**
 * @brief IgoLibInfo is used to keep information for iGo library.
 *
 */
struct IgoLibInfo {
	uint32_t major_version; /* Major version */
	uint32_t minor_version; /* Minor version */
	uint32_t build_version; /* Build version */
	uint32_t ext_version;   /* Extension version */
	uint32_t handle_size;   /* Size of handle structure */
};

/**
 * @brief IgoStreamData is used to keep audio data for iGo library.
 *
 */
struct IgoStreamData {
	void *data;				 /* Data array */
	enum IgoDataWidth data_width; /* Specify audio data bit width */
	uint16_t sample_num;	 /* Sample number in this data bulk */
	uint16_t sampling_rate;  /* Sampling rate for the data stream */
};

/**
 * @brief IgoLibConfig is used to keep lib configuration for lib instance
 * initialization.
 *
 */
struct IgoLibConfig {
	const char *algo_name; /* Algorithm name */
	uint8_t in_ch_num;     /* Input channel number for the algo instance */
	uint8_t ref_ch_num;    /* Reference channel number for the algo instance */
	uint8_t out_ch_num;    /* Output channel number for the algo instance */
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief IgoLibGetInfo() - Retrieve the lib information.
 * @param[out]      info     Lib information.
 *
 * This API is used to get library detail information.
 *
 * @return iGo defined return value.
 */
enum IgoRet IgoLibGetInfo(struct IgoLibInfo *info);

/**
 * @brief IgoLibInit() - Initialize iGo lib instance.
 * @param[out]      handle  iGo lib handle.
 * @param[in]       config  Lib configuration for initialization.
 * @param[in]       param   the point of iGO Parameter for initialization.
 *
 * This API is used to initialize iGo lib instance and get a handle which is
 * for control the iGo lib instance.
 *
 * P.S. The channel number in the config is algorithm dependent.
 *
 * @return iGo defined return value.
 */
enum IgoRet IgoLibInit(void *handle,
		       const struct IgoLibConfig *config,
		       void *param);

/**
 * @brief IgoLibProcess() - Process audio stream.
 * @param[in]       handle  iGo lib handle.
 * @param[in]       in      input audio stream.
 * @param[in]       ref     reference audio stream.
 * @param[out]      out     output audio stream.
 *
 * This API is used to process audio stream. The default audio sample is 16bit.
 * The sampling rate and sample number should be specified in IgoStreamData
 * structure. If the channel number > 1 for IgoStreamData, the data should be
 * interleaved sample by sample.
 *
 * Note:    IgoLibProcess supports 16k/48k 16bit data only by default.
 *          If other data format or sampling rate is required, please
 *          ask intelliGo for support.
 *
 * @return iGo defined return value.
 */
enum IgoRet IgoLibProcess(void *handle,
			  const struct IgoStreamData *in,
			  const struct IgoStreamData *ref,
			  const struct IgoStreamData *out);

#ifdef __cplusplus
}
#endif

#endif
