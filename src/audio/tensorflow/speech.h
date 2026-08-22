// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.

#ifndef __TFLM_SPEECH_H__
#define __TFLM_SPEECH_H__

#include "tensorflow/lite/core/c/common.h"

/* Class-count and label names emitted alongside the retrained model by
 * sof_tflm_train.py. Regenerated on every training run so speech.h never
 * needs a manual edit when the keyword list changes.
 */
#include "sof_tflm_labels.h"

/* default model configuration */
#define TFLM_SAMPLE_RATE 16000
#define TFLM_FEATURE_SIZE 40
#define TFLM_FEATURE_COUNT 49
#define TFLM_FEATURE_ELEM_COUNT (TFLM_FEATURE_SIZE * TFLM_FEATURE_COUNT)
#define TFLM_FEATURE_STRIDE_MS 20
#define TFLM_FEATURE_DURATION_MS 30

struct tf_classify {
	int8_t *audio_features;
	size_t audio_data_size;
	int categories;
	const char *error;
	float predictions[TFLM_CATEGORY_COUNT];
	int8_t raw_output[TFLM_CATEGORY_COUNT];
	int op_count;
	uint32_t node_cycles[10];
	int node_codes[10];
};

/* Export of C++ APIs into C namespace for linkage */
#ifdef __cplusplus
extern "C"
{
#endif

	/* 1st - pass in tflite flatbuffer formatted model, size is included in
	 * model metadata.
	 */
	int TF_SetModel(struct tf_classify *tfc, unsigned char *model);

	/* 2nd - register the kernels and init TF micro for inference */
	int TF_InitOps(struct tf_classify *tfc);

	/* 3rd - perform the inference */
	int TF_ProcessClassify(struct tf_classify *tfc);

	/* Interpreter tensor-arena usage after AllocateTensors(); 0 if the
	 * interpreter is not initialized. Diagnostic for tuning kArenaSize.
	 */
	size_t TF_ArenaUsedBytes(void);

	/* Total tensor-arena capacity provisioned in the firmware image. */
	size_t TF_ArenaCapacity(void);

#ifdef __cplusplus
}
#endif

#endif
