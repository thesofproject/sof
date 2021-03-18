/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2021 NXP
 *
 * Author: Mihai Despotovici <mihai.despotovici@nxp.com>
 */

#ifndef __KWD_NN_CONFIG_H__
#define __KWD_NN_CONFIG_H__

/* RAW AUDIO DATA CONFIGURATION */
#define KWD_NN_MS_TO_SAMPLES(samplerate, time) ((samplerate) * (time) / 1000)
#define KWD_NN_SIZE_FROM_STRIDE_SIZE(stride, size, no_windows) \
		(((no_windows) - 1) * (stride) + (size))

#define KWD_NN_CONFIG_SAMPLERATE 16000
#define KWD_NN_CONFIG_NO_CHANNELS 1

#define KWD_NN_CONFIG_WINDOW_STRIDE KWD_NN_MS_TO_SAMPLES(KWD_NN_CONFIG_SAMPLERATE, 20)
#define KWD_NN_CONFIG_WINDOW_SIZE KWD_NN_MS_TO_SAMPLES(KWD_NN_CONFIG_SAMPLERATE, 30)
#define KWD_NN_CONFIG_NO_WINDOWS 49

#define KWD_NN_CONFIG_RAW_SIZE (KWD_NN_CONFIG_NO_CHANNELS * \
		KWD_NN_SIZE_FROM_STRIDE_SIZE( \
			KWD_NN_CONFIG_WINDOW_STRIDE, \
			KWD_NN_CONFIG_WINDOW_SIZE, \
			KWD_NN_CONFIG_NO_WINDOWS))

/* PREPROCESSED DATA CONFIGURATION */
#define KWD_NN_CONFIG_PREPROCESSED_HEIGHT KWD_NN_CONFIG_NO_WINDOWS
#define KWD_NN_CONFIG_SPECTROGRAM_SIZE 256
#define KWD_NN_CONFIG_PREPROCESSED_AVGPOOL_WIDTH 6
#define KWD_NN_CONFIG_PREPROCESSED_WIDTH 43
#define KWD_NN_CONFIG_PREPROCESSED_SIZE (KWD_NN_CONFIG_PREPROCESSED_HEIGHT * \
		KWD_NN_CONFIG_PREPROCESSED_WIDTH)

/* NN can report one of the following 4 answers
 * after applying inference: YES, NO, UNKNOWN or
 * SILENCE. The function computes confidence
 * value for each possible answer
 */
#define KWD_NN_CONFIDENCES_SIZE        4

/* if confidence of the NN result is lower than
 * this threshold, the probability to be a
 * false-positive is high
 */
#define KWD_NN_MIN_ACCEPTABLE_CONFIDENCE	128

#endif /* __KWD_NN_CONFIG_H__ */
