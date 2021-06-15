// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.

#ifndef _KPB_WOV_COMMON_DEFS_H
#define _KPB_WOV_COMMON_DEFS_H
#include "adsp_error.h"
namespace dsp_fw
{

enum KpbClientMode
{
    KPB_CLIENT_DEFAULT = 0,
    KPB_CLIENT_STREAMING_ON_DEMAND
};

struct KpbModuleInfo
{
    uint16_t module_id;
    uint16_t instance_id;

    KpbModuleInfo():
        module_id(0),
        instance_id(0)
    { }
};

struct KpbClientData
{
    size_t max_hist_depth;
    size_t current_history_depth;
    uint32_t output_pin;
    KpbClientMode mode;
    KpbModuleInfo module_info;

    KpbClientData() :
        max_hist_depth(0),
        current_history_depth(0),
        output_pin(0),
        mode(KPB_CLIENT_DEFAULT),
        module_info()
    { }
};

typedef void* KpbClientHandle;

struct KpbEventData
{
    KpbClientHandle client;
    ErrorCode status;
    void* data;
};

struct KpdEventPayload
{
    int32_t confidence_level;
    uint32_t startpoint_ms;
    uint32_t endpoint_ms;
    uint32_t hist_begin;
    uint32_t hist_end;
    uint32_t history_size;
    uint32_t event_begin;
    uint32_t event_detection;
};

struct KpbUpdateHistDepthData
{
    size_t new_hist_depth;
};

struct KpbConsumerModuleInfo
{
    KpbModuleInfo module;
    uint16_t output;
    uint16_t mode;
    uint32_t fast_mode_module_count;
    KpbModuleInfo fast_mode_module_array[1];
};

struct KpbDataConsumerInfo
{
    uint32_t consumer_count;
    KpbConsumerModuleInfo consumer_array[1];
};

}

#endif /* KPB_WOV_COMMON_DEFS_H_ */
