/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Johny Lin <johnylin@google.com>
 */

#ifndef __SOF_AUDIO_DCBLOCK_CODEC__
#define __SOF_AUDIO_DCBLOCK_CODEC__

#include <stdint.h>
#include <sof/audio/component.h>
#include <sof/platform.h>

/*****************************************************************************/
/* DCBlock private data types						     */
/*****************************************************************************/
struct dcblock_state {
	int32_t x_prev; /**< state variable referring to x[n-1] */
	int32_t y_prev; /**< state variable referring to y[n-1] */
};

/**
 * \brief Type definition for the processing function for the
 * DC Blocking Filter.
 */
typedef void (*dcblock_func)(const struct comp_dev *dev,
			     const void *in_buff,
			     const void *out_buff,
			     uint32_t avail_bytes,
			     uint32_t *produced_bytes);

enum dcblock_config_id {
	DCBLOCK_CONFIG_NONE = 0, /**< Not used */
	DCBLOCK_CONFIG_R_COEFFS, /**< R_coeffs */
};

struct dcblock_codec_data {
	/**< filters state */
	struct dcblock_state state[PLATFORM_MAX_CHANNELS];

	/** coefficients for the processing function */
	int32_t R_coeffs[PLATFORM_MAX_CHANNELS];

	dcblock_func dcblock_func; /**< processing function */
};

/*****************************************************************************/
/* DCBlock interfaces							     */
/*****************************************************************************/
int dcblock_codec_init(struct comp_dev *dev);
int dcblock_codec_prepare(struct comp_dev *dev);
int dcblock_codec_process(struct comp_dev *dev);
int dcblock_codec_apply_config(struct comp_dev *dev);
int dcblock_codec_reset(struct comp_dev *dev);
int dcblock_codec_free(struct comp_dev *dev);

#endif /* __SOF_AUDIO_DCBLOCK_CODEC__ */
