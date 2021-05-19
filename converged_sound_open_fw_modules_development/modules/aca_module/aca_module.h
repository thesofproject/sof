// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.


#ifndef ACA_MODULE_H_
#define ACA_MODULE_H_


#include "loadable_processing_module.h"
#include "build/module_design_config.h"
#include "aca_config.h"

#ifdef NOTIFICATION_SUPPORT
#include <notification_message.h>
#endif

/*!
 * \brief The AcaModule class is an implementation example
 * of ProcessingModuleInterface which simply amplifies the input stream
 * by a constant gain value.
 *
 * The AcaModule is a single input-single output module.
 * It can take any size of the input frame as long as it is suitable with the length of sample word.
 */
class AcaModule : public intel_adsp::ProcessingModule<DESIGN_CONFIG>
{
public:
    /*! \brief Set of error codes value specific to this module
     */
    enum InternalError
    {
        PROCESS_SUCCEED = 0,
#ifdef NOTIFICATION_SUPPORT
        PROCESS_NOTIFICATION_ERROR = 1,
#endif
    };

    /*! Defines alias for the base class */
    typedef intel_adsp::ProcessingModule<DESIGN_CONFIG> ProcessingModule;

    /*! \brief Initializes a new instance of AcaModule
     *
     * \param [in] num_channels             number of channels.
     * \param [in] bits_per_sample          bits per input and output audio sample.
     * \param [in] sampling_frequency       sampling frequency.
     * \param [in] ibs                      input buffer size.
     * \param [in] system_agent             system_agent to check in the instance which is initializing.
     */
    AcaModule(
        uint32_t num_channels,
        size_t bits_per_sample,
        uint32_t sampling_frequency,
        uint32_t ibs,
        intel_adsp::SystemAgentInterface &system_agent):
        ProcessingModule(system_agent),
        ms_per_frame_(ibs / ((bits_per_sample / 8) * num_channels * ( sampling_frequency / 1000 )))
    {
        processing_mode_ = intel_adsp::ProcessingMode::NORMAL;
        ACA_Environment_notification_counter_ = 0;
    }

    virtual uint32_t Process(intel_adsp::InputStreamBuffer *input_stream_buffers,
                             intel_adsp::OutputStreamBuffer *output_stream_buffers) /*override*/;

    virtual ErrorCode::Type SetConfiguration(
        uint32_t config_id,
        intel_adsp::ConfigurationFragmentPosition fragment_position,
        uint32_t data_offset_size,
        const uint8_t *fragment_buffer,
        size_t fragment_size,
        uint8_t *response,
        size_t &response_size
        );                                     /*override*/

    virtual ErrorCode::Type GetConfiguration(
        uint32_t config_id,
        intel_adsp::ConfigurationFragmentPosition fragment_position,
        uint32_t &data_offset_size,
        uint8_t *fragment_buffer,
        size_t &fragment_size
        );                                     /*override*/

    virtual void SetProcessingMode(intel_adsp::ProcessingMode mode);     /*override*/

    virtual intel_adsp::ProcessingMode GetProcessingMode();     /*override*/

    virtual void Reset();     /*override*/

private:
    InternalError CheckEnvironment(uint8_t* input_buffer, size_t data_size);

    InternalError CheckResult(uint8_t* input_buffer, size_t data_size);
#ifdef NOTIFICATION_SUPPORT
    // function used to send notification
    InternalError Send_ACA_Environment_Notification();

    InternalError Send_ACA_Sound_Notification();
#endif //#ifdef NOTIFICATION_SUPPORT

    const uint16_t ms_per_frame_;

    uint16_t ACA_Environment_notification_counter_;
    // current processing mode
    intel_adsp::ProcessingMode processing_mode_;
    // reserves space for module instances' bss
    AcaBss bss_;

#ifdef NOTIFICATION_SUPPORT
    // Notification object used to send message to host
    // NOTE: Template<> is expected to contain max size of the Aca notification messages (if several)
    intel_adsp::ModuleNotificationMessage<sizeof(ACA_SoundNotificationParams)> notification_event_message_;
    intel_adsp::ModuleNotificationMessage<sizeof(ACA_EnvironmentNotificationParams)> notification_environment_message_;
#endif //#ifdef NOTIFICATION_SUPPORT

};     // class AcaModule

class AcaModuleFactory
    : public intel_adsp::ProcessingModuleFactory<AcaModuleFactory,
                                     AcaModule>
{
public:
    AcaModuleFactory(
        intel_adsp::SystemAgentInterface &system_agent)
        :   intel_adsp::ProcessingModuleFactory<AcaModuleFactory,
                                    AcaModule>(
            system_agent)
    {}

    ErrorCode::Type Create(
        intel_adsp::SystemAgentInterface &system_agent,
        intel_adsp::ModulePlaceholder *module_placeholder,
        intel_adsp::ModuleInitialSettings initial_settings
        );
};     // class AcaModuleFactory

#endif // ACA_MODULE_H_
