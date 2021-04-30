/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 */

#ifndef __SOF_AUDIO_CADENCE_CODEC__
#define __SOF_AUDIO_CADENCE_CODEC__

#include <sof/audio/cadence/xa_type_def.h>
#include <sof/audio/cadence/xa_apicmd_standards.h>
#include <sof/audio/cadence/xa_error_standards.h>
#include <sof/audio/cadence/xa_memory_standards.h>

#define LIB_NAME_MAX_LEN 30
#define LIB_NO_ERROR XA_NO_ERROR
#define LIB_IS_FATAL_ERROR(e) ((e) & XA_FATAL_ERROR)

/*****************************************************************************/
/* Cadence API functions							     */
/*****************************************************************************/
extern xa_codec_func_t cadence_api_function;
extern xa_codec_func_t xa_aac_dec;
extern xa_codec_func_t xa_bsac_dec;
extern xa_codec_func_t xa_dabplus_dec;
extern xa_codec_func_t xa_drm_dec;
extern xa_codec_func_t xa_mp3_dec;
extern xa_codec_func_t xa_sbc_dec;

/*****************************************************************************/
/* Cadence private data types						     */
/*****************************************************************************/
struct cadence_api {
	uint32_t id;
	xa_codec_func_t *api;
};

enum default_stream_params_ids {
	CADENCE_SAMPLE_RATE_ID = 0x01,
	CADENCE_SAMPLE_WIDTH_ID = 0x02,
	CADENCE_CHANNELS_COUNT_ID = 0x04
};

struct cadence_codec_data {
	char name[LIB_NAME_MAX_LEN];
	void *self;
	xa_codec_func_t *api;
	void *mem_tabs;
	uint32_t sample_width_id;
	uint32_t sample_rate_id;
	uint32_t channels_id;
};

/*****************************************************************************/
/* Cadence interfaces							     */
/*****************************************************************************/
int cadence_codec_init(struct comp_dev *dev);
int cadence_codec_prepare(struct comp_dev *dev);
int cadence_codec_get_samples(struct comp_dev *dev);
int cadence_codec_init_process(struct comp_dev *dev);
int cadence_codec_process(struct comp_dev *dev);
int cadence_codec_apply_config(struct comp_dev *dev);
int cadence_codec_reset(struct comp_dev *dev);
int cadence_codec_free(struct comp_dev *dev);

#endif /* __SOF_AUDIO_CADENCE_CODEC__ */
