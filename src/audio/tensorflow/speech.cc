// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>

#include "tensorflow/lite/core/c/common.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/testing/micro_test.h"
#include "speech.h"

#include "sof_tflm_quantized_model_data.h"

// The following values are derived from values used during model training.
// If you change the way you preprocess the input, update all these constants.
static constexpr int kFeatureSize = TFLM_FEATURE_SIZE;
static constexpr int kFeatureCount = TFLM_FEATURE_COUNT;
static constexpr int kFeatureElementCount = TFLM_FEATURE_ELEM_COUNT;

// Sized for the retrained tiny_conv DS-CNN wake-word model.  Tune down to
// interpreter->arena_used_bytes() (printed by tflm-classify.c on success)
// once the model is finalized.
static constexpr size_t kArenaSize = 131072;
alignas(16) static uint8_t g_arena[kArenaSize];

// type for features
using Features = int8_t[kFeatureCount][kFeatureSize];

// inference
static const tflite::Model *model;
static TfLiteTensor *input;
static TfLiteTensor *output;
static tflite::MicroInterpreter *interpreter;

using MicroSpeechOpResolver = tflite::MicroMutableOpResolver<7>;
static MicroSpeechOpResolver *op_resolver;

// Adding more kernels is quite efficient. TODO add more
int RegisterOps(MicroSpeechOpResolver *op_resolver) {
	TF_LITE_ENSURE_STATUS(op_resolver->AddReshape());
	TF_LITE_ENSURE_STATUS(op_resolver->AddFullyConnected());
	TF_LITE_ENSURE_STATUS(op_resolver->AddDepthwiseConv2D());
	TF_LITE_ENSURE_STATUS(op_resolver->AddSoftmax());
	/* Dynamic-Reshape triple emitted by newer TF/Keras -> tflite converters. */
	TF_LITE_ENSURE_STATUS(op_resolver->AddShape());
	TF_LITE_ENSURE_STATUS(op_resolver->AddStridedSlice());
	TF_LITE_ENSURE_STATUS(op_resolver->AddPack());
	return 0;
}

static int Init_Interpreter(struct tf_classify *tfc);

int TF_InitOps(struct tf_classify *tfc)
{
	op_resolver = new MicroSpeechOpResolver();
	if (!op_resolver) {
		tfc->error = "op_resolver alloc failed (OOM)";
		return -ENOMEM;
	}

	if (RegisterOps(op_resolver) != 0) {
		tfc->error = "register ops failed";
		return -EINVAL;
	}

	// create the interpreter
	interpreter = new tflite::MicroInterpreter(model, *op_resolver,
						   g_arena, kArenaSize);
	if (!interpreter) {
		tfc->error = "interpreter alloc failed (OOM)";
		delete op_resolver;
		op_resolver = nullptr;
		return -ENOMEM;
	}

	// and allocate the tensors
	if (interpreter->AllocateTensors() != kTfLiteOk) {
		tfc->error = "interpreter tensor allocate failed";
		delete interpreter;
		delete op_resolver;
		interpreter = nullptr;
		op_resolver = nullptr;
		return -EINVAL;
	}

	// fetch input/output tensors + quantization params once; the
	// interpreter/tensors are stable for the lifetime of this instance
	return Init_Interpreter(tfc);
}

static int Init_Interpreter(struct tf_classify *tfc)
{
	input = interpreter->input(0);
	if (!input){
		tfc->error = "input interpreter NULL";
		return -EINVAL;
	}

	// Accept any input rank as long as the total element count matches
	// (retrained tiny_conv is [1,49,40,1] rather than a flat [1,1960]).
	int input_elems = 1;
	for (int i = 0; i < input->dims->size; i++)
		input_elems *= input->dims->data[i];
	if (kFeatureElementCount != input_elems) {
		tfc->error = "input interpreter shape incompatible";
		MicroPrintf("TFLM: input rank=%d elems=%d expected=%d",
			    input->dims->size, input_elems, kFeatureElementCount);
		for (int i = 0; i < input->dims->size; i++)
			MicroPrintf("TFLM: input dim[%d]=%d", i,
				    input->dims->data[i]);
		return -EINVAL;
	}

	output = interpreter->output(0);
	if (!output){
		tfc->error = "output interpreter NULL";
		return -EINVAL;
	}

	// Accept any output rank as long as total element count == categories.
	int output_elems = 1;
	for (int i = 0; i < output->dims->size; i++)
		output_elems *= output->dims->data[i];
	if (tfc->categories != output_elems) {
		tfc->error = "output shape != categories";
		MicroPrintf("TFLM: output rank=%d elems=%d categories=%d",
			    output->dims->size, output_elems, tfc->categories);
		return -EINVAL;
	}

	return 0;
}

int TF_SetModel(struct tf_classify *tfc, unsigned char *model_tflite)
{
	// ignore passed in model today until we can load via binary kcontrol

	// Map the model into a usable data structure. This doesn't involve any
	// copying or parsing, it's a very lightweight operation.
	model = tflite::GetModel(g_sof_tflm_quantized_model_data);
	if (model->version() != TFLITE_SCHEMA_VERSION) {
		tfc->error = "failed to load model";
		return -EINVAL;
	}

	return 0;
}

int TF_ProcessClassify(struct tf_classify *tfc)
{
	Features *features = reinterpret_cast<Features *>(tfc->audio_features);
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
		int8_t raw = tflite::GetTensorData<int8_t>(output)[i];
		tfc->raw_output[i] = raw;
		tfc->predictions[i] = (raw - output_zero_point) * output_scale;
	}

return 0;
}

size_t TF_ArenaUsedBytes(void)
{
	return interpreter ? interpreter->arena_used_bytes() : 0;
}

size_t TF_ArenaCapacity(void)
{
	return kArenaSize;
}
