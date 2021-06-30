// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.


#ifndef ACA_CONFIG_H_
#define ACA_CONFIG_H_

#define NOTIFICATION_SUPPORT

#include <stdint.h>

#pragma pack(4)

#ifdef NOTIFICATION_SUPPORT
/*!
 * \brief Defines the structure of the notification message which can be sent
 * from the Aca module to driver.
 */
#define ACA_ENVIRONMENT_NOTIFICATION_ID 0
/*! Value in ms, indicating how often Environment notification is send */
#define ACA_ENVIRONMENT_NOTIFICATION_PERIOD 2000

struct ACA_EnvironmentNotificationParams
{
    uint16_t AcaEnvironmentType_;
    uint32_t score_;
};
#define ACA_SOUND_NOTIFICATION_ID 1
struct ACA_SoundNotificationParams
{
    uint16_t AcaEventType_;
    uint32_t score_;
};

//! Example types of sound events.
typedef enum e_AcaEventType{
    kEventBabyCry = 0,
    kEventGlassBreak = 1,
    kEventAlarm = 2,
    kEventScream = 3,
    kEventSpeech = 4,
    kEventGunshot = 5,
    kUnknownEvent = 0xFFFFFFFF
} AcaEventType;

//! Example types of environments.
typedef enum e_AcaEnvironmentType{
    kNormalEnv = 0,
    kUnknownEnv = 0xFFFFFFFF
} AcaEnvironmentType;

//! State of the detector
typedef enum e_AcaDetectionState{
    kEventLowState = 0, //!< set when there's no detection
    kEventBegin = 1, //!< set when the state changes from low to high
    kEventHighState = 2, //!< set in the middle of an event
    kEventEnd = 3 //!< set when the state changes from high to low
} AcaDetectionState;

//! Information about the event specified in Event Type
typedef struct AcaResult_
{
    AcaResult_(): event_type(kUnknownEvent), score(0), detected(false), state(kEventLowState)
    {}
    AcaEventType event_type; //!< selected type of event
    int32_t score; //!< current highest score for selected event in Q23
    bool detected; //!< true if the score is currently over threshold, can remain true for multiple frames
    AcaDetectionState state; //!< state of the detector - detection/no detection or rising/falling edge of event
} AcaResult;

//! Information about the environment specified in environment Type
typedef struct AcaEnvironment_
{

    AcaEnvironment_(): environment_type(kNormalEnv), score(0)
    {}
    AcaEnvironmentType environment_type; //!< selected type of event
    int32_t score; //!< current highest score for selected event in Q23
} AcaEnvironment;



#endif //#ifdef NOTIFICATION_SUPPORT

/*!
 * \brief Defines the structure of the configuration message which can be sent/received
 * to/from the Aca module through SetConfiguration/GetConfiguration.
 */
struct AcaConfig
{
    /* Custom module configuration to be set
     * .... */
};

typedef struct AcaBss
{
    AcaConfig aca_config_;
    AcaEnvironment aca_environment_params_;
    AcaResult aca_detection_result_;
} AcaBss;

#pragma pack()

#endif // ACA_CONFIG_H_
