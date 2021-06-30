// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.

#ifndef ADSP_FW_CALLBACK_INTERFACE_H
#define ADSP_FW_CALLBACK_INTERFACE_H

namespace dsp_fw
{

/**
 * callback interface for any module that needs to have registered callback 
 * */
class CallbackInterface
{
public:
    /**
     * Callback routine with parameter.
     *
     * @param argument generic argument (needs to be properly decoded)
     * */
    virtual void Callback(void* argument = NULL) = 0;
};

/**
 * callback interface for timestamping
 * */
class TimestampCallbackInterface : public CallbackInterface
{
public:
    DwordArray ts_buffer_;
};


}
#endif /* ADSP_FW_CALLBACK_INTERFACE_H */
