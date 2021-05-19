// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.

#ifndef _MODEL_SHARING_H_
#define _MODEL_SHARING_H_
#include "adsp_error.h"
#include "adsp_std_defs.h"

namespace dsp_fw
{
namespace model_sharing
{
//! Max supported key phrases model.
static const size_t WOV_WHM_MAX_KP = 4;

struct ModelState
{
    ModelState() : model_id_(0), active_(0)
    {
    }
    uint32_t model_id_;
    uint32_t active_;
};

struct SharedModelDesc
{
    SharedModelDesc() : event_client_(NULL), kpd_sensitivity_(0xFFFF), kpb_output_pin_(0),
        history_buffer_size_idle_(0), history_buffer_size_max_(0)
    {
    }
    void* event_client_;
    ModelState model_state_;
    uint32_t kpd_sensitivity_;
    uint32_t kpb_output_pin_;
    uint32_t history_buffer_size_idle_;
    uint32_t history_buffer_size_max_;
};

struct SharedModelEvent
{
    SharedModelEvent() : status_(ADSP_FAILURE), model_(NULL), model_size_(0), models_desc_cnt_(0)
    {
    }

    ErrorCode status_;
    uint8_t * model_;
    uint32_t model_size_;
    uint32_t models_desc_cnt_;
    SharedModelDesc models_desc_[WOV_WHM_MAX_KP];
};

} // namespace model_sharing

} // namespace dsp_fw
#endif //_MODEL_SHARING_H_
