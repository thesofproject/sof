// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Google Inc LLC. All rights reserved.
//
// Author: Curtis Malainey <cujomalainey@chromium.org>
//

#include <sof/audio/MaxxEffect/Initialize/MaxxEffect_Initialize.h>
#include <sof/audio/MaxxEffect/Process/MaxxEffect_Process.h>
#include <sof/audio/MaxxEffect/Process/MaxxEffect_Reset.h>
#include <sof/audio/MaxxEffect/Control/RPC/MaxxEffect_RPC_Server.h>
#include <sof/audio/MaxxEffect/Control/Direct/MaxxEffect_Revision.h>

MaxxStatus_t    MaxxEffect_GetMessageMaxSize(
    MaxxEffect_t*   effect,
    uint32_t*       requestBytes,
    uint32_t*       responseBytes)
{
	return 0;
}

MaxxStatus_t    MaxxEffect_Process(
    MaxxEffect_t*           effect,
    MaxxStream_t* const     inputStreams[],
    MaxxStream_t* const     outputStreams[])
{
	return 0;
}

MaxxStatus_t    MaxxEffect_Reset(
    MaxxEffect_t*           effect)
{
	return 0;
}

MaxxStatus_t    MaxxEffect_GetEffectSize(
    uint32_t*           bytes)
{
    return 0;
}

MaxxStatus_t    MaxxEffect_Initialize(
    MaxxEffect_t*                   effect,
    MaxxStreamFormat_t* const       inputFormats[],
    uint32_t                        inputFormatsCount,
    MaxxStreamFormat_t* const       outputFormats[],
    uint32_t                        outputFormatsCount)
{
    return 0;
}

MaxxStatus_t    MaxxEffect_Message(
    MaxxEffect_t*   effect,
    const void*     request,
    uint32_t        requestBytes,
    void*           response,
    uint32_t*       responseBytes)
{
    return 0;
}

MaxxStatus_t    MaxxEffect_Revision_Get(
    MaxxEffect_t*       effect,
    const char**        revision,
    uint32_t*           bytes)
{
    return 0;
}
