// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.


#ifndef DOWNMIXER_MODULE_H_
#define DOWNMIXER_MODULE_H_


#include "loadable_processing_module.h"
#include "build/module_design_config.h"
#include "downmixer_config.h"


/*!
 * \brief The DownmixerModule class is an implementation example of the ProcessingModuleInterface
 *  which performs a weighted average of 2 input audio streams.
 *
 * The DownmixerModule is a 2 inputs/single output module. It is also a "sampled-based" module i.e.
 * it can process any size of input buffer as long as it is suitable with (divisible by) the size of a sample group.
 * A sample group corresponds to a frame of samples containing 1 sample per available channel in an input or an output stream.
 *
 * Supported audio settings:
 * * 1 or 2 inputs can be connected. "Master" input settings for pin with index 0 is mandatory.
 * * If only "master" input is connected, reference pin format shall be supplied anyway in case of dynamic connection.
 * * The audio input and output samples can have 16 bits or 32 bits depth
 * (however all inputs and outputs shall be configured with the same bit depth).
 * * Sampling frequency shall be configured identically for all inputs and output.
 * * The input pin 0 can be fed with 1 to 4 channels.
 * * The input pin 1 can be fed with 1 or 2 channels.
 * * The output pin can be fed with 1 or 2 channels independently from input configurations.
 * * The output signal is the sum of all the signals (channels) on Master pin plus all the signals (channels) on reference pin
 * weighted with divider factor meant to avoid saturations. If 2 channels format is required for output, the channel 0 is
 * duplicated into channel 1.
 */

class DownmixerModule : public intel_adsp::ProcessingModule<DESIGN_CONFIG>
{
public:
    /*! \brief Set of error codes value specific to this module
     */
    enum InternalError
    {
        PROCESS_SUCCEED = 0,
        INVALID_IN_BUFFERS_SIZE = 1,
        INVALID_CONFIGURATION = 2
    };

    /*! Defines alias for the base class */
    typedef intel_adsp::ProcessingModule<DESIGN_CONFIG> ProcessingModule;

    /*! \brief Initializes a new instance of DownmixerModule
     *
     * \param [in] bits_per_sample          number of bits per sample for the inputs and output audio samples.
     * \param [in] input0_channels_count    count of channels for the input pin 0.
     * \param [in] input1_channels_count    count of channels for the input pin 1.
     * \param [in] output_channels_count    count of channels for the output pin.
     * \param [in] system_agent             to check in the instance which is initializing.
     */
    DownmixerModule(
            intel_adsp::BitDepth bits_per_sample,
            size_t input0_channels_count,
            size_t input1_channels_count,
            size_t output_channels_count,
            intel_adsp::SystemAgentInterface &system_agent) :
        ProcessingModule(system_agent),
        bits_per_sample_(bits_per_sample),
        input0_channels_count_(input0_channels_count),
        input1_channels_count_(input1_channels_count),
        output_channels_count_(output_channels_count)
    {
        processing_mode_ = intel_adsp::ProcessingMode::NORMAL;
        config_.divider_input_0 = input0_channels_count_ + input1_channels_count_;
        config_.divider_input_1 = config_.divider_input_0;
    }

    virtual uint32_t Process(
        intel_adsp::InputStreamBuffer *input_stream_buffers,
        intel_adsp::OutputStreamBuffer *output_stream_buffers) /*override*/;

    virtual ErrorCode::Type SetConfiguration(
        uint32_t config_id,
        intel_adsp::ConfigurationFragmentPosition fragment_position,
        uint32_t data_offset_size,
        const uint8_t *fragment_buffer,
        size_t fragment_size,
        uint8_t *response,
        size_t &response_size) /*override*/;

    virtual ErrorCode::Type GetConfiguration(
        uint32_t config_id,
        intel_adsp::ConfigurationFragmentPosition fragment_position,
        uint32_t &data_offset_size,
        uint8_t *fragment_buffer,
        size_t &fragment_size) /*override*/;

    /*! \brief SetProcessingMode
     *  BY_PASS mode averages the 2 inputs. (Input_1 + Input_2) / 2.
     *  NORMAL  mode applies the divider values to each input passed by the SetConfiguration() call.
     *  Default mode is BY_PASS mode.
     */
    virtual void SetProcessingMode(intel_adsp::ProcessingMode mode); /*override*/

    virtual intel_adsp::ProcessingMode GetProcessingMode(); /*override*/

    virtual void Reset(); /*override*/

private:

    // Indicates the bits per audio sample in the input streams and to produce in the output stream
    const size_t bits_per_sample_;
    // Indicates the count of channels on the input pin 0.
    const size_t input0_channels_count_;
    // Indicates the count of channels on the input pin 1. It can be worth 0 if the input pin 1 has not been configued.
    // If so, any audio samples reaching the input pin 1 will be discarded.
    const size_t input1_channels_count_;
    // Indicates the count of channels on the output pin.
    const size_t output_channels_count_;
    // Indicates tCurrent active configuration
    DownmixerConfig config_;
    // current processing mode
    intel_adsp::ProcessingMode processing_mode_;

}; // class DownmixerModule

class DownmixerModuleFactory
    : public intel_adsp::ProcessingModuleFactory<DownmixerModuleFactory,
                                     DownmixerModule>
{
public:
    DownmixerModuleFactory(intel_adsp::SystemAgentInterface &system_agent)
        : intel_adsp::ProcessingModuleFactory<DownmixerModuleFactory,
                                  DownmixerModule>(system_agent)
    {}

    ErrorCode::Type Create(
        intel_adsp::SystemAgentInterface &system_agent,
        intel_adsp::ModulePlaceholder *module_placeholder,
        intel_adsp::ModuleInitialSettings initial_settings);
}; // class DownmixerModuleFactory

#endif // DOWNMIXER_MODULE_H_
