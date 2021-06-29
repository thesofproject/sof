// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.


#ifndef AMPLIFIER_MODULE_H_
#define AMPLIFIER_MODULE_H_


#include "loadable_processing_module.h"
#include "build/module_design_config.h"
#include "amplifier_config.h"
#ifdef NOTIFICATION_SUPPORT
#include <notification_message.h>
#endif


/*! Value in sample groups, indicating how often the gain value is updated */
#define PROCESSING_BLOCK_SIZE 48

/*!
 * \brief The AmplifierModule class is an implementation example
 * of ProcessingModuleInterface which simply amplifies the input stream
 * by a constant gain value.
 *
 * The AmplifierModule is a single input-single output module.
 * It can take any size of the input frame as long as it is suitable with the length of sample word.
 */
class AmplifierModule : public intel_adsp::ProcessingModule<DESIGN_CONFIG>
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

    /*! \brief Initializes a new instance of AmplifierModule
     *
     * \param [in] num_channels             number of channels.
     * \param [in] bits_per_sample          bits per input and output audio sample.
     * \param [in] system_agent             system_agent to check in the instance which is initializing.
     */
    AmplifierModule(
        uint32_t num_channels,
        size_t bits_per_sample,
        intel_adsp::SystemAgentInterface &system_agent):
        ProcessingModule(system_agent),
        num_channels_(num_channels),
        bits_per_sample_(bits_per_sample),
        gain_value_(AMPLIFIER_GAIN_0DB),
        position_(0)
    {
        config_.min_gain = 0x0;
        config_.max_gain = 0x7FFF;
        config_.smoothing_factor = 0x7FFF;
        config_.target_gain = gain_value_;
        target_gain_reached_ = true;//gain_value_ is already equal to config_.target_gain
        time_to_reach_ = 0;
        processing_mode_ = intel_adsp::ProcessingMode::NORMAL;
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
    // Indicates the number of channels in the input streams and to produce in the output stream
    const uint32_t num_channels_;
    // Indicates the bits per audio sample in the input streams and to produce in the output stream
    const size_t bits_per_sample_;
    // Gain value
    int gain_value_;
    // Position in processing window of size PROCESSING_BLOCK_SIZE, when == 0 triggers a gain update
    int position_;
    // Indicates the Current active configuration
    AmplifierConfig config_;
    // current processing mode
    intel_adsp::ProcessingMode processing_mode_;
    // bool indicating that target_gain is reached
    bool target_gain_reached_;
    // Number of process data iteration to reach target gain
    uint32_t time_to_reach_;
#ifdef NOTIFICATION_SUPPORT
    // Notification object to send message to driver
    // NOTE: Template<> is expected to contain max size of the Amplifier notification messages (if several)
    intel_adsp::ModuleNotificationMessage<sizeof(TargetGainNotification)> notification_message_;
    // function used to send notification
    InternalError SendNotification(const AmplifierConfig &current_config);
#endif //#ifdef NOTIFICATION_SUPPORT
    // function used to update gain value
    void UpdateGain(const AmplifierConfig &current_config);
    // internal optimized gain function for 16-bits data
    void Process16(uint8_t **input_buffer, uint8_t **output_buffer, size_t size);
    // internal optimized gain function for 32-bits data
    void Process32(uint8_t **input_buffer, uint8_t **output_buffer, size_t size);

};     // class AmplifierModule

class AmplifierModuleFactory
    : public intel_adsp::ProcessingModuleFactory<AmplifierModuleFactory,
                                     AmplifierModule>
{
public:
    AmplifierModuleFactory(
        intel_adsp::SystemAgentInterface &system_agent)
        :   intel_adsp::ProcessingModuleFactory<AmplifierModuleFactory,
                                    AmplifierModule>(
            system_agent)
    {}

    ErrorCode::Type Create(
        intel_adsp::SystemAgentInterface &system_agent,
        intel_adsp::ModulePlaceholder *module_placeholder,
        intel_adsp::ModuleInitialSettings initial_settings
        );
};     // class AmplifierModuleFactory

#endif // AMPLIFIER_MODULE_H_
