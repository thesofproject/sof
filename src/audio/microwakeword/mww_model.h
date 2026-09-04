// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.

#ifndef __MWW_MODEL_H__
#define __MWW_MODEL_H__

#include "tensorflow/lite/core/c/common.h"

/* microWakeWord streaming model configuration (hey_jarvis.tflite v2):
 * input tensor shape (1, MWW_FEATURE_SLICE_COUNT, MWW_FEATURE_SIZE), int8,
 * one new 10ms/40-bin MFCC hop per slice. The model is a stateful streaming
 * graph (TFLM resource variables + CALL_ONCE init subgraph) that keeps its
 * own longer-history ring buffers internally, so callers only ever need to
 * supply MWW_FEATURE_SLICE_COUNT fresh hops per Invoke() rather than a
 * caller-side sliding window.
 */
#define MWW_SAMPLE_RATE 16000
#define MWW_FEATURE_SIZE 40
#define MWW_FEATURE_SLICE_COUNT 3
#define MWW_FEATURE_ELEM_COUNT (MWW_FEATURE_SIZE * MWW_FEATURE_SLICE_COUNT)
#define MWW_FEATURE_STRIDE_MS 10
#define MWW_FEATURE_DURATION_MS 30

struct mww_classify {
	int8_t *audio_features;
	size_t audio_data_size;
	const char *error;
	float probability;	/**< dequantized wake-word probability, 0..1 */
	int32_t raw_output;	/**< raw int8/uint8 output tensor value */
	float input_scale;
	int input_zero_point;
};

/* Export of C++ APIs into C namespace for linkage */
#ifdef __cplusplus
extern "C"
{
#endif

	/* 1st - pass in tflite flatbuffer formatted model, size is included in
	 * model metadata.
	 */
	int MWW_SetModel(struct mww_classify *mwc, unsigned char *model);

	/* 2nd - register the kernels and init TF micro for inference */
	int MWW_InitOps(struct mww_classify *mwc);

	/* 3rd - perform the inference */
	int MWW_ProcessClassify(struct mww_classify *mwc);

	/**
	 * \brief Reset streaming resource variables to zero.
	 * \return 0 on success.
	 */
	int MWW_Reset(void);

	/**
	 * \brief Free TFLite Micro interpreter and op resolver heap allocations.
	 */
	void MWW_Free(void);

	size_t MWW_ArenaUsedBytes(void);
	size_t MWW_ArenaCapacity(void);

#ifdef __cplusplus
}
#endif

#endif
