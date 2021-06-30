// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.

#if defined(XTENSA_TOOLSCHAIN) || defined(__XCC__)
#include <xtensa/tie/xt_hifi2.h>
#endif

#include "aca_module.h"

#include <logger.h>

using namespace intel_adsp;

DECLARE_LOADABLE_MODULE(AcaModule,
                        AcaModuleFactory)


ErrorCode::Type AcaModuleFactory::Create(
    SystemAgentInterface &system_agent,
    ModulePlaceholder *module_placeholder,
    ModuleInitialSettings initial_settings
    )
{
    // count of input pins formats retrieved from the ModuleInitialSettings container
    const size_t in_pins_format_count = initial_settings.GetItem<IN_PINS_FORMAT>().GetLength();
    // count of output pins formats retrieved from the ModuleInitialSettings container
    const size_t out_pins_format_count = initial_settings.GetItem<OUT_PINS_FORMAT>().GetLength();

    LOG_MESSAGE(    LOW, "Create(in_pins_format_count = %d, out_pins_format_count=%d)",
                    LOG_ENTRY, in_pins_format_count, out_pins_format_count);

    // check that one audio format is available for the input pin
    if (in_pins_format_count < 1 || out_pins_format_count != 1)
    {
        LOG_MESSAGE(    CRITICAL, "Invalid count of input pin formats received (%d)",
                        LOG_ENTRY, in_pins_format_count);
        return ErrorCode::INVALID_SETTINGS;
    }

    InputPinFormat const& input_pin_format = initial_settings.GetItem<IN_PINS_FORMAT>()[0];
    OutputPinFormat const& output_pin_format = initial_settings.GetItem<OUT_PINS_FORMAT>()[0];
    // check that audio format retrieved for input is assigned to an existing module pin.
    if (input_pin_format.pin_index != 0 || output_pin_format.pin_index != 0 || input_pin_format.ibs != output_pin_format.obs )
    {
        LOG_MESSAGE(    CRITICAL, "Retrieved audio format is associated to an invalid input pin index (%d)",
                        LOG_ENTRY, input_pin_format.pin_index);
        return ErrorCode::INVALID_SETTINGS;
    }

    // check that ibs can be divided by the bytes size of "samples group"
    if ((input_pin_format.ibs * 8) % (input_pin_format.audio_fmt.bit_depth * input_pin_format.audio_fmt.number_of_channels))
    {
        LOG_MESSAGE(    CRITICAL, "ibs*8 shall be a multiple of samples group value: "
                        "ibs = %d, input_bit_depth = %d.",
                        LOG_ENTRY, input_pin_format.ibs, input_pin_format.audio_fmt.bit_depth);
         return ErrorCode::INVALID_SETTINGS;
    }

    // Initializes AcaModule instance using the
    // "placement syntax" of operator new
    new (module_placeholder)AcaModule(
        input_pin_format.audio_fmt.number_of_channels,
        input_pin_format.audio_fmt.bit_depth,
        input_pin_format.audio_fmt.sampling_frequency,
        input_pin_format.ibs,
        system_agent);

    return ErrorCode::NO_ERROR;
}

// Note that purpose of the source code presented below is to demonstrate usage
// of the ADSP System API.
// It might not be optimized enough for efficient computation.
uint32_t AcaModule::Process(
    InputStreamBuffer* input_stream_buffers,
    OutputStreamBuffer* output_stream_buffers)
{
    InternalError ec = PROCESS_SUCCEED;
    uint8_t* input_buffer = input_stream_buffers[0].data;
    size_t data_size = input_stream_buffers[0].size;

    if (input_buffer != NULL)
    {
        ec = CheckEnvironment(input_buffer, data_size);
        if(ec != PROCESS_SUCCEED) return ec;

        ec = CheckResult(input_buffer, data_size);
    }
    return (uint32_t)ec;
}

AcaModule::InternalError AcaModule::CheckEnvironment(uint8_t* input_buffer, size_t data_size)
{
    InternalError ec = PROCESS_SUCCEED;

    // ec = ACA_InternalEnvironmentProcess(&bss_, input_buffer, data_size) /* to implement */
    // if (ec != PROCESS_SUCCEED) return ec;

    ACA_Environment_notification_counter_++;
#ifdef NOTIFICATION_SUPPORT
    if( ACA_Environment_notification_counter_ == (ACA_ENVIRONMENT_NOTIFICATION_PERIOD / ms_per_frame_))
    {
        // NOTE: 2 second reached, send environment notification, set notification_counter to 0.
        ACA_Environment_notification_counter_ = 0;

        bss_.aca_environment_params_.score = bss_.aca_environment_params_.score;
        bss_.aca_environment_params_.environment_type = kNormalEnv;

        LOG_MESSAGE(MEDIUM, "Environment (%d) notify score (%d):", LOG_ENTRY, bss_.aca_environment_params_.environment_type, bss_.aca_environment_params_.score);
        ec = Send_ACA_Environment_Notification();
    }
#endif
    return ec;
}

AcaModule::InternalError AcaModule::CheckResult(uint8_t* input_buffer, size_t data_size)
{
    InternalError ec = PROCESS_SUCCEED;
    // ec = ACA_InternalSoundProcess(&bss_, input_buffer, data_size) /* to implement */
    // if (ec != PROCESS_SUCCEED) return ec;
    if(bss_.aca_detection_result_.detected)
    {
        bss_.aca_detection_result_.score = bss_.aca_detection_result_.score + 1;
        bss_.aca_detection_result_.event_type = kEventScream;
        bss_.aca_detection_result_.state = kEventHighState;
        LOG_MESSAGE(HIGH, "Event (%d) notify score (%d):", LOG_ENTRY, bss_.aca_detection_result_.event_type, bss_.aca_detection_result_.score);
        ec = Send_ACA_Sound_Notification();
    }
    return ec;
}
#ifdef NOTIFICATION_SUPPORT
AcaModule::InternalError AcaModule::Send_ACA_Environment_Notification()
{
    InternalError ec = PROCESS_SUCCEED;
    ACA_EnvironmentNotificationParams* notification_data =
        notification_environment_message_.GetNotification<ACA_EnvironmentNotificationParams>
                                                         (ACA_ENVIRONMENT_NOTIFICATION_ID, GetSystemService()/* system_service reference */);
    if (notification_data != NULL)
    {
        notification_data->AcaEnvironmentType_ = bss_.aca_environment_params_.environment_type;
        notification_data->score_ = bss_.aca_environment_params_.score;
        notification_environment_message_.Send(GetSystemService());
        ec = PROCESS_SUCCEED;
    }
    else
    {
        ec = PROCESS_NOTIFICATION_ERROR;
    }

    return ec;
}
AcaModule::InternalError AcaModule::Send_ACA_Sound_Notification()
{
    InternalError ec = PROCESS_SUCCEED;
    ACA_SoundNotificationParams* notification_data =
        notification_event_message_.GetNotification<ACA_SoundNotificationParams>
                                                   (ACA_SOUND_NOTIFICATION_ID, GetSystemService()/* system_service reference */);
    if (notification_data != NULL)
    {
        notification_data->AcaEventType_ = bss_.aca_detection_result_.event_type;
        notification_data->score_ = bss_.aca_detection_result_.score;
        notification_event_message_.Send(GetSystemService());
        ec = PROCESS_SUCCEED;
    }
    else
    {
        ec = PROCESS_NOTIFICATION_ERROR;
    }

    return ec;
}
#endif //#ifdef NOTIFICATION_SUPPORT
ErrorCode::Type AcaModule::SetConfiguration(
    uint32_t config_id,
    ConfigurationFragmentPosition fragment_position,
    uint32_t data_offset_size,
    const uint8_t *fragment_block,
    size_t fragment_size,
    uint8_t *response,
    size_t &response_size)
{
    const AcaConfig *cfg =
        reinterpret_cast<const AcaConfig *>(fragment_block);
    // TODO parse configuration here
    LOG_MESSAGE(LOW, "SetConfiguration()", LOG_ENTRY);
    return ErrorCode::NO_ERROR;
}

ErrorCode::Type AcaModule::GetConfiguration(
    uint32_t config_id,
    ConfigurationFragmentPosition fragment_position,
    uint32_t &data_offset_size,
    uint8_t *fragment_buffer,
    size_t &fragment_size)
{
    LOG_MESSAGE(LOW, "GetConfiguration()", LOG_ENTRY);
    return ErrorCode::NO_ERROR;
}

void AcaModule::SetProcessingMode(ProcessingMode mode)
{
    LOG_MESSAGE(LOW, "SetProcessingMode()", LOG_ENTRY);

    // Store module mode
    processing_mode_ = mode;
}

ProcessingMode AcaModule::GetProcessingMode()
{
    LOG_MESSAGE(LOW, "GetProcessingMode()", LOG_ENTRY);

    return processing_mode_;
}

void AcaModule::Reset()
{
    LOG_MESSAGE(LOW, "Reset", LOG_ENTRY);
    processing_mode_ = ProcessingMode::NORMAL;

    // Leave config_ parameters unchanged.
}
