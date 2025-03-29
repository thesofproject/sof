// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.

#ifndef __TFMICRO_SPEECH_H__
#define __TFMICRO_SPEECH_H__

#include "tensorflow/lite/core/c/common.h"

/* default model configuration */
#define TFMICRO_SAMPLE_RATE 16000
#define TFMICRO_FEATURE_SIZE 40
#define TFMICRO_FEATURE_COUNT 49
#define TFMICRO_FEATURE_ELEM_COUNT (TFMICRO_FEATURE_SIZE * TFMICRO_FEATURE_COUNT)
#define TFMICRO_FEATURE_STRIDE_MS 20
#define TFMICRO_FEATURE_DURATION_MS 30

#define TFMICRO_CATEGORY_COUNT  4
#define TFMICRO_CATEGORY_DATA   {"silence", "unknown", "yes", "no",}


struct tf_classify {
    int8_t *audio_features;
    size_t audio_data_size;
    int categories;
    const char *error;
    float predictions[TFMICRO_CATEGORY_COUNT];
};

/* Export of C++ APIs into C namespace for linkage */
#ifdef __cplusplus
extern "C"
{
#endif

    /* 1st - pass in tflite flatbuffer formatted model, size is included in
     *  model metadata.
     */
    int TF_SetModel(struct tf_classify *tfc, unsigned char *model);

    /* 2nd - register the kernels and init TF micro for inference */
    int TF_InitOps(struct tf_classify *tfc);

    /* 3rd - perform the inference */
    int TF_ProcessClassify(struct tf_classify *tfc);

#ifdef __cplusplus
}
#endif

#endif