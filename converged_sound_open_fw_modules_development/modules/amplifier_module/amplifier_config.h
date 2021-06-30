// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.


#ifndef AMPLIFIER_CONFIG_H_
#define AMPLIFIER_CONFIG_H_

#define NOTIFICATION_SUPPORT

#include <stdint.h>

#pragma pack(4)

/*! \brief Defines the default value for module gain equal to 0 dB (Q3:12 format) */
#define AMPLIFIER_GAIN_0DB 0x1000
/*!
 * \brief Defines the structure of the configuration message which can be sent/received
 * to/from the Amplifier module through SetConfiguration/GetConfiguration.
 */
struct AmplifierConfig
{
    /*! \brief Amplifier gain high threshold (Q3:12 format) */
    uint16_t max_gain;
    /*! \brief Amplifier gain low threshold (Q3:12 format) */
    uint16_t min_gain;
    /*! \brief Smoothing coefficient to adapt current gain value to target gain value.
     *
     * Algorithm applied : gain_value_ = target_gain_value_*smoothing_factor + (1 - smoothing_factor)*gain_value_
     * Can be set between 0x7FFF (= 1 -> no smoothing) and 0xF (= 2e-12 -> slowest convergence to target gain)
     */
    uint16_t smoothing_factor;
    /*! \brief New gain value (Q3:12 format) to be applied.
     *
     * This new target gain will be valid only if its value is between min_gain and max_gain
     */
    uint16_t target_gain;
};

/*!
 * \brief Defines the structure of the notification message which can be sent
 * from the Amplifier module to driver.
 */
#ifdef NOTIFICATION_SUPPORT
struct TargetGainNotification
{
    /*! \brief Value of the gain reached
     */
    uint32_t gain_reached;
    /*! \brief Value of the smooth factor
     */
    uint32_t factor;
    /*! \brief Number of process data launched to reach the target gain
     */
    uint32_t time_to_reach;
};
#endif //#ifdef NOTIFICATION_SUPPORT

#pragma pack()

#endif // AMPLIFIER_CONFIG_H_
