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

#include "mww_model_data.h"
#include "mww_model_data_banana.h"
#include "mww_model_data_orange.h"

static constexpr int kFeatureSize = MWW_FEATURE_SIZE;
static constexpr int kFeatureElementCount = MWW_FEATURE_ELEM_COUNT;
static constexpr int kMaxSlots = 3;
static constexpr int kNumResourceVariables = 6;

// Arena size per instance: 96 KB is sufficient for the streaming MixConv graph
// (12 ops incl. Conv2D/DepthwiseConv2D/FullyConnected plus 6 persistent ring buffers).
static constexpr size_t kArenaSize = 98304;
alignas(16) static uint8_t g_arenas[kMaxSlots][kArenaSize];

using MwwOpResolver = tflite::MicroMutableOpResolver<14>;

struct mww_instance {
	uint8_t slot_id;
	uint8_t *arena;
	size_t arena_size;
	const tflite::Model *model;
	TfLiteTensor *input;
	TfLiteTensor *output;
	tflite::MicroInterpreter *interpreter;
	tflite::MicroAllocator *allocator;
	tflite::MicroResourceVariables *resource_variables;
	MwwOpResolver *op_resolver;

	mww_instance() :
		slot_id(0), arena(nullptr), arena_size(kArenaSize),
		model(nullptr), input(nullptr), output(nullptr),
		interpreter(nullptr), allocator(nullptr),
		resource_variables(nullptr), op_resolver(nullptr)
	{
	}
};

static struct mww_instance g_instances[kMaxSlots];

static int RegisterOps(MwwOpResolver *op_resolver) {
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

const unsigned char *MWW_GetDefaultModel(uint8_t slot_id, size_t *size)
{
	switch (slot_id) {
	case 0:
		if (size)
			*size = mww_model_data_strawberry_size;
		return mww_model_data_strawberry;
	case 1:
		if (size)
			*size = mww_model_data_banana_size;
		return mww_model_data_banana;
	case 2:
	default:
		if (size)
			*size = mww_model_data_orange_size;
		return mww_model_data_orange;
	}
}

int MWW_SetModel(struct mww_classify *mwc, const unsigned char *model_tflite, size_t model_size)
{
	if (!mwc)
		return -EINVAL;

	uint8_t slot = mwc->slot_id;
	if (slot >= kMaxSlots)
		slot = 0;

	struct mww_instance *inst = &g_instances[slot];
	inst->slot_id = slot;
	mwc->instance = inst;

	if (!model_tflite || model_size == 0)
		model_tflite = MWW_GetDefaultModel(slot, &model_size);

	if (!model_tflite) {
		mwc->error = "no model provided";
		return -EINVAL;
	}

	inst->model = tflite::GetModel(model_tflite);
	if (inst->model->version() != TFLITE_SCHEMA_VERSION) {
		mwc->error = "failed to load model (schema mismatch)";
		return -EINVAL;
	}

	return 0;
}

int MWW_InitOps(struct mww_classify *mwc)
{
	if (!mwc || !mwc->instance)
		return -EINVAL;

	struct mww_instance *inst = static_cast<struct mww_instance *>(mwc->instance);
	uint8_t slot = inst->slot_id;

	inst->op_resolver = new MwwOpResolver();
	if (!inst->op_resolver) {
		mwc->error = "op_resolver alloc failed (OOM)";
		return -ENOMEM;
	}

	if (RegisterOps(inst->op_resolver) != 0) {
		mwc->error = "register ops failed";
		delete inst->op_resolver;
		inst->op_resolver = nullptr;
		return -EINVAL;
	}

	inst->arena = g_arenas[slot];
	inst->arena_size = kArenaSize;

	inst->allocator = tflite::MicroAllocator::Create(inst->arena, inst->arena_size);
	if (!inst->allocator) {
		mwc->error = "allocator alloc failed (OOM)";
		delete inst->op_resolver;
		inst->op_resolver = nullptr;
		return -ENOMEM;
	}

	inst->resource_variables = tflite::MicroResourceVariables::Create(inst->allocator, kNumResourceVariables);
	if (!inst->resource_variables) {
		mwc->error = "resource_variables alloc failed (OOM)";
		delete inst->op_resolver;
		inst->op_resolver = nullptr;
		return -ENOMEM;
	}

	inst->interpreter = new tflite::MicroInterpreter(inst->model, *inst->op_resolver,
							  inst->allocator, inst->resource_variables);
	if (!inst->interpreter) {
		mwc->error = "interpreter alloc failed (OOM)";
		delete inst->op_resolver;
		inst->op_resolver = nullptr;
		return -ENOMEM;
	}

	if (inst->interpreter->AllocateTensors() != kTfLiteOk) {
		mwc->error = "interpreter tensor allocate failed";
		delete inst->interpreter;
		delete inst->op_resolver;
		inst->interpreter = nullptr;
		inst->op_resolver = nullptr;
		return -EINVAL;
	}

	inst->input = inst->interpreter->input(0);
	if (!inst->input) {
		mwc->error = "input interpreter NULL";
		return -EINVAL;
	}

	int in_elems = 1;
	for (int i = 0; i < inst->input->dims->size; i++)
		in_elems *= inst->input->dims->data[i];
	if (kFeatureElementCount != in_elems) {
		mwc->error = "input interpreter shape incompatible";
		return -EINVAL;
	}

	inst->output = inst->interpreter->output(0);
	if (!inst->output) {
		mwc->error = "output interpreter NULL";
		return -EINVAL;
	}

	if (inst->output->type != kTfLiteInt8 && inst->output->type != kTfLiteUInt8) {
		mwc->error = "output tensor type != int8/uint8";
		return -EINVAL;
	}

	mwc->input_scale = inst->input->params.scale;
	mwc->input_zero_point = inst->input->params.zero_point;

	return 0;
}

int MWW_ProcessClassify(struct mww_classify *mwc)
{
	if (!mwc || !mwc->instance)
		return -EINVAL;

	struct mww_instance *inst = static_cast<struct mww_instance *>(mwc->instance);

	float output_scale = inst->output->params.scale;
	int output_zero_point = inst->output->params.zero_point;

	std::copy_n(mwc->audio_features, kFeatureElementCount,
		    tflite::GetTensorData<int8_t>(inst->input));

	if (inst->interpreter->Invoke() != kTfLiteOk) {
		mwc->error = "invoke failed";
		return -EINVAL;
	}

	float raw_val;
	if (inst->output->type == kTfLiteInt8) {
		int8_t val = tflite::GetTensorData<int8_t>(inst->output)[0];
		raw_val = static_cast<float>(val);
		mwc->raw_output = val;
	} else {
		uint8_t val = tflite::GetTensorData<uint8_t>(inst->output)[0];
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

int MWW_Reset(struct mww_classify *mwc)
{
	if (!mwc || !mwc->instance)
		return 0;

	struct mww_instance *inst = static_cast<struct mww_instance *>(mwc->instance);
	if (inst->resource_variables)
		inst->resource_variables->ResetAll();
	return 0;
}

void MWW_Free(struct mww_classify *mwc)
{
	if (!mwc || !mwc->instance)
		return;

	struct mww_instance *inst = static_cast<struct mww_instance *>(mwc->instance);
	delete inst->interpreter;
	delete inst->op_resolver;
	inst->interpreter = nullptr;
	inst->op_resolver = nullptr;
	inst->allocator = nullptr;
	inst->resource_variables = nullptr;
	inst->model = nullptr;
	inst->input = nullptr;
	inst->output = nullptr;
	mwc->instance = nullptr;
}

size_t MWW_ArenaUsedBytes(struct mww_classify *mwc)
{
	if (!mwc || !mwc->instance)
		return 0;
	struct mww_instance *inst = static_cast<struct mww_instance *>(mwc->instance);
	return inst->interpreter ? inst->interpreter->arena_used_bytes() : 0;
}

size_t MWW_ArenaCapacity(struct mww_classify *mwc)
{
	(void)mwc;
	return kArenaSize;
}
