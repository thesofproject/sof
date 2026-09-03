/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2021 NXP
 *
 * Author: Cristina Feies <cristina.ilie@nxp.com>
 * Author: Viorel Suman <viorel.suman@nxp.com>
 */

#ifndef __USER_KWD_NN_DETECT_TEST_H__
#define __USER_KWD_NN_DETECT_TEST_H__

#include "kwd_nn/kwd_nn_config.h"
#include <sof/audio/component.h>

#define KWD_NN_SILENCE         0
#define KWD_NN_UNKNOWN         1
#define KWD_NN_YES_KEYWORD     2
#define KWD_NN_NO_KEYWORD      3

/* 990 ms of data considering frame rate 16KHz */
#define KWD_NN_KEY_LEN         (990 * 16 * 1)
#define KWD_NN_NUM_OF_CHANNELS 1
/* ~2 seconds of samples */
#define KWD_NN_IN_BUFF_SIZE    (2 * KWD_NN_KEY_LEN * KWD_NN_NUM_OF_CHANNELS)

int kwd_nn_postprocess(uint8_t confidences[KWD_NN_CONFIDENCES_SIZE]);
void kwd_nn_detect_test(struct comp_dev *dev,
			const struct audio_stream *source,
			uint32_t frames);

#endif /* __USER_KWD_NN_DETECT_TEST_H__ */
