// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.

#include <algorithm>
#include <cstdint>
#include <iterator>

#include "tensorflow/lite/core/c/common.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/testing/micro_test.h"
#include "speech.h"

// hard code the model today
#include "micro_speech_quantized_model_data.h"

// The following values are derived from values used during model training.
// If you change the way you preprocess the input, update all these constants.
//constexpr int kAudioSampleFrequency = TFLM_SAMPLE_RATE;
static constexpr int kFeatureSize = TFLM_FEATURE_SIZE;
static constexpr int kFeatureCount = TFLM_FEATURE_COUNT;
static constexpr int kFeatureElementCount = TFLM_FEATURE_ELEM_COUNT;

// Arena size is a guesstimate, followed by use of
// MicroInterpreter::arena_used_bytes() on both the AudioPreprocessor and
// MicroSpeech models and using the larger of the two results.
static constexpr size_t kArenaSize = 28584;  // xtensa p6
alignas(16) static uint8_t g_arena[kArenaSize];

// type for features
using Features = int8_t[kFeatureCount][kFeatureSize];

// inference
static const tflite::Model *model;
static TfLiteTensor *input;
static TfLiteTensor *output;
static tflite::MicroInterpreter *interpreter;

using MicroSpeechOpResolver = tflite::MicroMutableOpResolver<4>;
static MicroSpeechOpResolver *op_resolver;

// Adding more kernels is quite efficient. TODO add more
int RegisterOps(MicroSpeechOpResolver *op_resolver) {
	TF_LITE_ENSURE_STATUS(op_resolver->AddReshape());
	TF_LITE_ENSURE_STATUS(op_resolver->AddFullyConnected());
	TF_LITE_ENSURE_STATUS(op_resolver->AddDepthwiseConv2D());
	TF_LITE_ENSURE_STATUS(op_resolver->AddSoftmax());
	return 0;
}

int TF_InitOps(struct tf_classify *tfc)
{
	op_resolver = new MicroSpeechOpResolver();
	if (RegisterOps(op_resolver) != 0) {
		tfc->error = "register ops failed";
		return -EINVAL;
	}

	// create the interpreter
	interpreter = new tflite::MicroInterpreter(model, *op_resolver,
						   g_arena, kArenaSize);

	// and allocate the tensors
	if (interpreter->AllocateTensors() != kTfLiteOk) {
		tfc->error = "interpreter tensor allocate failed";
		delete interpreter;
		delete op_resolver;
		interpreter = nullptr;
		op_resolver = nullptr;
		return -EINVAL;
	}

	return 0;
}

static int Init_Interpreter(struct tf_classify *tfc)
{
	input = interpreter->input(0);
	if (!input){
		tfc->error = "input interpreter NULL";
		return -EINVAL;
	}

	// check input shape is compatible with our feature data size
	if (kFeatureElementCount != input->dims->data[input->dims->size - 1]){
		tfc->error = "input interpreter shape incompatible";
		return -EINVAL;
	}

	output = interpreter->output(0);
	if (!output){
		tfc->error = "output interpreter NULL";
		return -EINVAL;
	}

	// check output shape is compatible with our number of prediction categories
	if (tfc->categories != output->dims->data[output->dims->size - 1]) {
		tfc->error = "output shape != categories";
		return -EINVAL;
	}

	return 0;
}

int TF_SetModel(struct tf_classify *tfc, unsigned char *model_tflite)
{
	// ignore passed in model today until we can load via binary kcontrol

	// Map the model into a usable data structure. This doesn't involve any
	// copying or parsing, it's a very lightweight operation.
	model = tflite::GetModel(g_micro_speech_quantized_model_data);
	if (model->version() != TFLITE_SCHEMA_VERSION) {
		tfc->error = "failed to load model";
		return -EINVAL;
	}

	return 0;
}

int TF_ProcessClassify(struct tf_classify *tfc)
{
	Features *features = reinterpret_cast<Features *>(tfc->audio_features);
	int ret;

	// initialise the interpreter for current feature block
	ret = Init_Interpreter(tfc);
	if (!ret)
		return ret;

	float output_scale = output->params.scale;
	int output_zero_point = output->params.zero_point;

	// copy features to input then invoke()
	std::copy_n(features[0][0], kFeatureElementCount,
		    tflite::GetTensorData<int8_t>(input));

	// run the interpreter
	if (interpreter->Invoke() != kTfLiteOk) {
		tfc->error = "invoke failed";
		return -EINVAL;
	}

	// Dequantize output values
	for (int i = 0; i < tfc->categories; i++) {
		tfc->predictions[i] =
		(tflite::GetTensorData<int8_t>(output)[i] - output_zero_point) *
			output_scale;
	}

return 0;
}
