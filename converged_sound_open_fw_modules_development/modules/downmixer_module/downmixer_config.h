// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.


#ifndef DOWNMIXER_CONFIG_H_
#define DOWNMIXER_CONFIG_H_

#include <stdint.h>

#pragma pack(4)

/*!
 * \brief Defines the structure of the configuration message which can be sent/received
 * to/from the Downmixer module through SetConfiguration/GetConfiguration.
 */
struct DownmixerConfig
{
    /*! \brief Downmixer attenuation for each input */
    uint32_t divider_input_0;
    uint32_t divider_input_1;
};

#pragma pack()

#endif // DOWNMIXER_CONFIG_H_
