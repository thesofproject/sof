// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.

#include <algorithm>
#include <cstdint>
#include <iterator>

#include "tensorflow/lite/core/c/common.h"
#include "tensorflow/lite/micro/micro_allocator.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_resource_variable.h"
#include "mww_model.h"

#if !CONFIG_COMP_MWW_MODEL_FROM_CONTROL
#include "mww_model_data.h"
#endif

static constexpr int kFeatureSize = MWW_FEATURE_SIZE;
static constexpr int kFeatureElementCount = MWW_FEATURE_ELEM_COUNT;

// Arena size is a generous guesstimate for the streaming MixConv graph (12
// ops incl. Conv2D/DepthwiseConv2D/FullyConnected plus 6 persistent resource
// -variable ring buffers). Refine down using
// MicroInterpreter::arena_used_bytes() once measured on hardware (logged in
// MWW_InitOps() below).
static constexpr size_t kArenaSize = 131072;
alignas(16) static uint8_t g_arena[kArenaSize];

// inference
static const tflite::Model *model;
static TfLiteTensor *input;
static TfLiteTensor *output;
static tflite::MicroInterpreter *interpreter;
static tflite::MicroAllocator *allocator;
static tflite::MicroResourceVariables *resource_variables;

// stream, stream_1..stream_5: the MixConv graph's 6 persistent ring-buffer
// state variables (VAR_HANDLE/ASSIGN_VARIABLE/READ_VARIABLE), confirmed by
// direct flatbuffer inspection (see plan Stage 1) -- CALL_ONCE invokes a
// second subgraph that ASSIGN_VARIABLEs their zero initial state.
static constexpr int kNumResourceVariables = 6;

// Ops used by the hey_jarvis.tflite streaming graph: CALL_ONCE, VAR_HANDLE,
// READ_VARIABLE, ASSIGN_VARIABLE, RESHAPE, CONCATENATION, STRIDED_SLICE,
// CONV_2D, DEPTHWISE_CONV_2D, FULLY_CONNECTED, LOGISTIC, QUANTIZE,
// DEQUANTIZE.
using MwwOpResolver = tflite::MicroMutableOpResolver<14>;
static MwwOpResolver *op_resolver;

int RegisterOps(MwwOpResolver *op_resolver) {
	TF_LITE_ENSURE_STATUS(op_resolver->AddCallOnce());
	TF_LITE_ENSURE_STATUS(op_resolver->AddVarHandle());
	TF_LITE_ENSURE_STATUS(op_resolver->AddReadVariable());
	TF_LITE_ENSURE_STATUS(op_resolver->AddAssignVariable());
	TF_LITE_ENSURE_STATUS(op_resolver->AddReshape());
	TF_LITE_ENSURE_STATUS(op_resolver->AddConcatenation());
	TF_LITE_ENSURE_STATUS(op_resolver->AddStridedSlice());
	TF_LITE_ENSURE_STATUS(op_resolver->AddConv2D());
	TF_LITE_ENSURE_STATUS(op_resolver->AddDepthwiseConv2D());
	TF_LITE_ENSURE_STATUS(op_resolver->AddFullyConnected());
	TF_LITE_ENSURE_STATUS(op_resolver->AddLogistic());
	TF_LITE_ENSURE_STATUS(op_resolver->AddQuantize());
	TF_LITE_ENSURE_STATUS(op_resolver->AddDequantize());
	return 0;
}

static int Init_Interpreter(struct mww_classify *mwc);

int MWW_InitOps(struct mww_classify *mwc)
{
	op_resolver = new MwwOpResolver();
	if (!op_resolver) {
		mwc->error = "op_resolver alloc failed (OOM)";
		return -ENOMEM;
	}

	if (RegisterOps(op_resolver) != 0) {
		mwc->error = "register ops failed";
		return -EINVAL;
	}

	// VAR_HANDLE/ASSIGN_VARIABLE require an explicit MicroResourceVariables
	// instance registered with the interpreter -- without one,
	// VarHandlePrepare()/AssignVariable Eval() fail with kTfLiteError as soon
	// as AllocateTensors() prepares the first VAR_HANDLE node (see
	// tensorflow/lite/micro/kernels/var_handle.cc). Building via the
	// allocator-based MicroInterpreter constructor lets us create the
	// allocator once and share it with MicroResourceVariables::Create().
	allocator = tflite::MicroAllocator::Create(g_arena, kArenaSize);
	if (!allocator) {
		mwc->error = "allocator alloc failed (OOM)";
		delete op_resolver;
		op_resolver = nullptr;
		return -ENOMEM;
	}

	resource_variables = tflite::MicroResourceVariables::Create(allocator, kNumResourceVariables);
	if (!resource_variables) {
		mwc->error = "resource_variables alloc failed (OOM)";
		delete op_resolver;
		op_resolver = nullptr;
		return -ENOMEM;
	}

	// create the interpreter
	interpreter = new tflite::MicroInterpreter(model, *op_resolver,
						   allocator, resource_variables);
	if (!interpreter) {
		mwc->error = "interpreter alloc failed (OOM)";
		delete op_resolver;
		op_resolver = nullptr;
		return -ENOMEM;
	}

	// and allocate the tensors
	if (interpreter->AllocateTensors() != kTfLiteOk) {
		mwc->error = "interpreter tensor allocate failed";
		delete interpreter;
		delete op_resolver;
		interpreter = nullptr;
		op_resolver = nullptr;
		return -EINVAL;
	}

	// fetch input/output tensors + quantization params once; the
	// interpreter/tensors are stable for the lifetime of this instance
	return Init_Interpreter(mwc);
}

static int Init_Interpreter(struct mww_classify *mwc)
{
	input = interpreter->input(0);
	if (!input) {
		mwc->error = "input interpreter NULL";
		return -EINVAL;
	}

	// check input tensor element count is compatible with our feature
	// data size (shape is (1, MWW_FEATURE_SLICE_COUNT, MWW_FEATURE_SIZE),
	// not flattened to a single trailing dim, so check the full product)
	int in_elems = 1;
	for (int i = 0; i < input->dims->size; i++)
		in_elems *= input->dims->data[i];
	if (kFeatureElementCount != in_elems) {
		mwc->error = "input interpreter shape incompatible";
		return -EINVAL;
	}

	output = interpreter->output(0);
	if (!output) {
		mwc->error = "output interpreter NULL";
		return -EINVAL;
	}

	// single sigmoid wake-word probability, quantized int8 or uint8
	if (output->type != kTfLiteInt8 && output->type != kTfLiteUInt8) {
		mwc->error = "output tensor type != int8/uint8";
		return -EINVAL;
	}

	// expose the model's real input quantization params so callers can
	// requantize their features correctly instead of assuming a fixed
	// scale/zero_point.
	mwc->input_scale = input->params.scale;
	mwc->input_zero_point = input->params.zero_point;

	return 0;
}

int MWW_SetModel(struct mww_classify *mwc, unsigned char *model_tflite)
{
#if !CONFIG_COMP_MWW_MODEL_FROM_CONTROL
	if (!model_tflite)
		model_tflite = const_cast<unsigned char *>(mww_model_data);
#endif

	if (!model_tflite) {
		mwc->error = "no model provided";
		return -EINVAL;
	}

	// Map the model into a usable data structure. This doesn't involve any
	// copying or parsing, it's a very lightweight operation.
	model = tflite::GetModel(model_tflite);
	if (model->version() != TFLITE_SCHEMA_VERSION) {
		mwc->error = "failed to load model";
		return -EINVAL;
	}

	return 0;
}

int MWW_ProcessClassify(struct mww_classify *mwc)
{
	float output_scale = output->params.scale;
	int output_zero_point = output->params.zero_point;

	// copy features to input then invoke()
	std::copy_n(mwc->audio_features, kFeatureElementCount,
		    tflite::GetTensorData<int8_t>(input));

	// run the interpreter
	if (interpreter->Invoke() != kTfLiteOk) {
		mwc->error = "invoke failed";
		return -EINVAL;
	}

	// Dequantize the single sigmoid probability output
	float raw_val;
	if (output->type == kTfLiteInt8) {
		int8_t val = tflite::GetTensorData<int8_t>(output)[0];
		raw_val = static_cast<float>(val);
		mwc->raw_output = val;
	} else {
		uint8_t val = tflite::GetTensorData<uint8_t>(output)[0];
		raw_val = static_cast<float>(val);
		mwc->raw_output = val;
	}

	mwc->probability = (raw_val - output_zero_point) * output_scale;
	if (mwc->probability < 0.0f)
		mwc->probability = 0.0f;
	else if (mwc->probability > 1.0f)
		mwc->probability = 1.0f;

	return 0;
}

int MWW_Reset(void)
{
	if (resource_variables)
		resource_variables->ResetAll();
	return 0;
}

void MWW_Free(void)
{
	delete interpreter;
	delete op_resolver;
	interpreter = nullptr;
	op_resolver = nullptr;
	allocator = nullptr;
	resource_variables = nullptr;
	model = nullptr;
	input = nullptr;
	output = nullptr;
}

size_t MWW_ArenaUsedBytes(void)
{
	return interpreter ? interpreter->arena_used_bytes() : 0;
}

size_t MWW_ArenaCapacity(void)
{
	return kArenaSize;
}
