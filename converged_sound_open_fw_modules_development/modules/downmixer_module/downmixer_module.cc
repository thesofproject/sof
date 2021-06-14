// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.

#include "downmixer_module.h"

#include <logger.h>

using namespace intel_adsp;

DECLARE_LOADABLE_MODULE(DownmixerModule,
                        DownmixerModuleFactory)


ErrorCode::Type DownmixerModuleFactory::Create(
        SystemAgentInterface &system_agent,
        ModulePlaceholder *module_placeholder,
        ModuleInitialSettings initial_settings)
{
    // count of input pins formats retrieved from the ModuleInitialSettings container
    const size_t in_pins_format_count   = initial_settings.GetItem<IN_PINS_FORMAT>().GetLength();
    // count of output pins formats retrieved from the ModuleInitialSettings container
    const size_t out_pins_format_count  = initial_settings.GetItem<OUT_PINS_FORMAT>().GetLength();

    // check that at least 1 audio format has been retrieved for 1 input pin
    // and there are no more audio formats than the module input pins count.
    if ((in_pins_format_count < 1) || (in_pins_format_count > DownmixerModule::kInputCount))
    {
        LOG_MESSAGE(    CRITICAL, "Invalid count of input pin formats received (%d)",
                        LOG_ENTRY, in_pins_format_count);
        return ErrorCode::INVALID_SETTINGS;
    }

    // check that one audio format is available for the output pin
    if (out_pins_format_count != 1)
    {
        LOG_MESSAGE(    CRITICAL, "Invalid count of output pin formats received (%d)",
                        LOG_ENTRY, out_pins_format_count);
        return ErrorCode::INVALID_SETTINGS;
    }

    OutputPinFormat const& output_pin_format    = initial_settings.GetItem<OUT_PINS_FORMAT>()[0];
    // check that output audio format is for output pin0
    if (output_pin_format.pin_index != 0)
    {
        LOG_MESSAGE(    CRITICAL, "Retrieved audio format is associated to an invalid output pin index (%d)",
                        LOG_ENTRY, out_pins_format_count);
        return ErrorCode::INVALID_SETTINGS;
    }

    // array of input audio format indexed by the input pin index.
    InputPinFormat input_pin_format[DownmixerModule::kInputCount];

    // initialize each ibs to 0 to indicate that pin format has not yet been configured.
    for (int i = 0 ; i < DownmixerModule::kInputCount ; i++)
        input_pin_format[i].ibs = 0;

    for (int i = 0 ; i < in_pins_format_count ; i++)
    {
        InputPinFormat const& pin_format = initial_settings.GetItem<IN_PINS_FORMAT>()[i];

        // check that audio format retrieved for input is assigned to an existing module pin.
        if (pin_format.pin_index >= DownmixerModule::kInputCount)
        {
            LOG_MESSAGE(    CRITICAL, "Retrieved audio format is associated to an invalid input pin index (%d)",
                            LOG_ENTRY, pin_format.pin_index);
            return ErrorCode::INVALID_SETTINGS;
        }

        input_pin_format[pin_format.pin_index] = pin_format;
    }

    // check that at least input pin0 has an audio format
    if (!input_pin_format[0].ibs)
    {
            LOG_MESSAGE(    CRITICAL, "Input pin 0 is not configured", LOG_ENTRY);
            return ErrorCode::INVALID_SETTINGS;
    }

    // check that input pin 0 and output pin 0 have compatible audio format
    if (    (input_pin_format[0].audio_fmt.sampling_frequency != output_pin_format.audio_fmt.sampling_frequency) ||
            (input_pin_format[0].audio_fmt.bit_depth != output_pin_format.audio_fmt.bit_depth))
    {
        LOG_MESSAGE(    CRITICAL, "Input pin0 and output pin0 formats have incompatible audio format: "
                        "input_freq = %d, output_freq = %d, input_bit_depth = %d, output_bit_depth = %d.",
                        LOG_ENTRY, input_pin_format[0].audio_fmt.sampling_frequency, output_pin_format.audio_fmt.sampling_frequency,
                        input_pin_format[0].audio_fmt.bit_depth, output_pin_format.audio_fmt.bit_depth);
        return ErrorCode::INVALID_SETTINGS;
    }

    // check that input pin 0 has a supported channels count
    if (    (input_pin_format[0].audio_fmt.number_of_channels != 1)
         && (input_pin_format[0].audio_fmt.number_of_channels != 2)
         && (input_pin_format[0].audio_fmt.number_of_channels != 3)
         && (input_pin_format[0].audio_fmt.number_of_channels != 4))
    {
        LOG_MESSAGE(    CRITICAL, "Input pin0 format has unsupported channels count (%d)",
                        LOG_ENTRY, input_pin_format[0].audio_fmt.number_of_channels);
        return ErrorCode::INVALID_SETTINGS;
    }

    // check that bit_depth value is supported
    if (    (output_pin_format.audio_fmt.bit_depth != DEPTH_16BIT) &&
            (output_pin_format.audio_fmt.bit_depth != DEPTH_32BIT))
    {
        LOG_MESSAGE(    CRITICAL, " bit depth in audio format is not supported (%d)",
                        LOG_ENTRY, output_pin_format.audio_fmt.bit_depth);
        return ErrorCode::INVALID_SETTINGS;
    }

    // check that output pin has a supported channels count
    if (   (output_pin_format.audio_fmt.number_of_channels != 1)
        && (output_pin_format.audio_fmt.number_of_channels != 2) )
    {
        LOG_MESSAGE(    CRITICAL, "Output pin format has unsupported channels count (%d)",
                        LOG_ENTRY, output_pin_format.audio_fmt.number_of_channels);
        return ErrorCode::INVALID_SETTINGS;
    }

    // check that pin 0 ibs can be divided by the bytes size of "samples group"
    if ((input_pin_format[0].ibs * 8) % (input_pin_format[0].audio_fmt.bit_depth * input_pin_format[0].audio_fmt.number_of_channels))
    {
        LOG_MESSAGE(    CRITICAL, "ibs0*8 shall be a multiple of samples group value: "
                        "ibs = %d, input_bit_depth = %d.",
                        LOG_ENTRY, input_pin_format[0].ibs, input_pin_format[0].audio_fmt.bit_depth);
        return ErrorCode::INVALID_SETTINGS;
    }

    // check that obs can be divided by the "bit_depth" settings value
    if ((output_pin_format.obs * 8) % (output_pin_format.audio_fmt.bit_depth * output_pin_format.audio_fmt.number_of_channels))
    {
        LOG_MESSAGE(    CRITICAL, "obs0*8 shall be a multiple of samples group value"
                        "obs = %d, output_bit_depth = %d.",
                        LOG_ENTRY, output_pin_format.obs, output_pin_format.audio_fmt.bit_depth);
        return ErrorCode::INVALID_SETTINGS;
    }

    if (input_pin_format[1].ibs)
    {
        // if some audio format is available to configure the input pin 1
        // check that input pin 0 and pin 1 have compatible audio format
        if (    (input_pin_format[0].audio_fmt.sampling_frequency != input_pin_format[1].audio_fmt.sampling_frequency)
            ||  (input_pin_format[0].audio_fmt.bit_depth != input_pin_format[1].audio_fmt.bit_depth))
        {
            LOG_MESSAGE(    CRITICAL, "Input pin0 and input pin1 formats have incompatible audio format: "
                            "input_freq[0] = %d, input_freq[1] = %d, input_bit_depth[0] = %d, input_bit_depth[1] = %d.",
                            LOG_ENTRY, input_pin_format[0].audio_fmt.sampling_frequency, input_pin_format[1].audio_fmt.sampling_frequency,
                            input_pin_format[0].audio_fmt.bit_depth, input_pin_format[1].audio_fmt.bit_depth);
            return ErrorCode::INVALID_SETTINGS;
        }

        // check that input pin 1 has a supported channels count
        if (   (input_pin_format[1].audio_fmt.number_of_channels != 1)
            && (input_pin_format[1].audio_fmt.number_of_channels != 2) )
        {
            LOG_MESSAGE(    CRITICAL, "Input pin1 format has unsupported channels count (%d)",
                            LOG_ENTRY, input_pin_format[1].audio_fmt.number_of_channels);
            return ErrorCode::INVALID_SETTINGS;
        }

        // check that pin 1 ibs can be divided by the bytes size of "samples group"
        if ((input_pin_format[1].ibs * 8) % (input_pin_format[1].audio_fmt.bit_depth * input_pin_format[1].audio_fmt.number_of_channels))
        {
            LOG_MESSAGE(    CRITICAL, "ibs1*8 shall be a multiple of samples group value: "
                            "ibs = %d, input_bit_depth = %d.",
                            LOG_ENTRY, input_pin_format[1].ibs, input_pin_format[1].audio_fmt.bit_depth);
            return ErrorCode::INVALID_SETTINGS;
        }
    }

    const size_t input1_channels_count = (input_pin_format[1].ibs) ? input_pin_format[1].audio_fmt.number_of_channels : 0;

    // Log BaseModuleCfgExt
    LOG_MESSAGE(VERBOSE, "Create, in_pins_format_count = %d, out_pins_format_count = %d", LOG_ENTRY, in_pins_format_count, out_pins_format_count);
    LOG_MESSAGE(VERBOSE, "Create, input_pin_format[0]: pin_index = %d, ibs = %d", LOG_ENTRY, input_pin_format[0].pin_index, input_pin_format[0].ibs);
    LOG_MESSAGE(VERBOSE, "Create, input_pin_format[0]: freq = %d, bit_depth = %d, channel_map = %d, channel_config = %d", LOG_ENTRY, input_pin_format[0].audio_fmt.sampling_frequency, input_pin_format[0].audio_fmt.bit_depth, input_pin_format[0].audio_fmt.channel_map, input_pin_format[0].audio_fmt.channel_config);
    LOG_MESSAGE(VERBOSE, "Create, input_pin_format[0]: interleaving_style = %d, number_of_channels = %d, audio_fmt.valid_bit_depth = %d, sample_type = %d", LOG_ENTRY, input_pin_format[0].audio_fmt.interleaving_style, input_pin_format[0].audio_fmt.number_of_channels, input_pin_format[0].audio_fmt.valid_bit_depth, input_pin_format[0].audio_fmt.sample_type);
    LOG_MESSAGE(VERBOSE, "Create, input_pin_format[1]: pin_index = %d, ibs = %d", LOG_ENTRY, input_pin_format[1].pin_index, input_pin_format[1].ibs);
    LOG_MESSAGE(VERBOSE, "Create, input_pin_format[1]: freq = %d, bit_depth = %d, channel_map = %d, channel_config = %d", LOG_ENTRY, input_pin_format[1].audio_fmt.sampling_frequency, input_pin_format[1].audio_fmt.bit_depth, input_pin_format[1].audio_fmt.channel_map, input_pin_format[1].audio_fmt.channel_config);
    LOG_MESSAGE(VERBOSE, "Create, input_pin_format[1]: interleaving_style = %d, number_of_channels = %d, audio_fmt.valid_bit_depth = %d, sample_type = %d", LOG_ENTRY, input_pin_format[1].audio_fmt.interleaving_style, input_pin_format[1].audio_fmt.number_of_channels, input_pin_format[1].audio_fmt.valid_bit_depth, input_pin_format[1].audio_fmt.sample_type);
    LOG_MESSAGE(VERBOSE, "Create, output_pin_format: pin_index = %d, 0bs = %d", LOG_ENTRY, output_pin_format.pin_index, output_pin_format.obs);
    LOG_MESSAGE(VERBOSE, "Create, output_pin_format: freq = %d, bit_depth = %d, channel_map = %d, channel_config = %d", LOG_ENTRY, output_pin_format.audio_fmt.sampling_frequency, output_pin_format.audio_fmt.bit_depth, output_pin_format.audio_fmt.channel_map, output_pin_format.audio_fmt.channel_config);
    LOG_MESSAGE(VERBOSE, "Create, output_pin_format: interleaving_style = %d, number_of_channels = %d, audio_fmt.valid_bit_depth = %d, sample_type = %d", LOG_ENTRY, output_pin_format.audio_fmt.interleaving_style, output_pin_format.audio_fmt.number_of_channels, output_pin_format.audio_fmt.valid_bit_depth, output_pin_format.audio_fmt.sample_type);

    // Initializes DownmixerModule instance using the "placement syntax" of operator new
    new (module_placeholder) DownmixerModule(
            output_pin_format.audio_fmt.bit_depth,
            input_pin_format[0].audio_fmt.number_of_channels,
            input1_channels_count,
            output_pin_format.audio_fmt.number_of_channels,
            system_agent);

    return ErrorCode::NO_ERROR;
}


// Note that purpose of the source code presented below is to demonstrate usage
// of the ADSP System API.
// It might not be optimized enough for efficient computation.
// Processed output = (Pin0Ch1/Div0 + Pin0Ch2/Div0 + Pin0Ch3/Div0 + Pin0Ch4/Div0) + (Pin1Ch1/Div1 + Pin0Ch2/Div1)
// If module output is configured in 2 channel, output is dual mono
uint32_t DownmixerModule::Process(
        InputStreamBuffer *input_stream_buffers,
        OutputStreamBuffer *output_stream_buffers)
{
    uint8_t const* input_buffer_0 = input_stream_buffers[0].data;
    // if input1_channels_count_ is worth 0, the pin 1 has not been configured and shall be discarded
    uint8_t const* input_buffer_1 = (input1_channels_count_) ? input_stream_buffers[1].data : NULL;
    uint8_t* output_buffer = output_stream_buffers[0].data;
    size_t data_size_0 = input_stream_buffers[0].size;
    size_t data_size_1 = input_stream_buffers[1].size;
    size_t data_size_per_channel = ( (output_stream_buffers[0].size/output_channels_count_) <= (data_size_0/input0_channels_count_)) ?
            output_stream_buffers[0].size/output_channels_count_ : data_size_0/input0_channels_count_;
    size_t output_data_size = output_channels_count_*data_size_per_channel;
    // ref_pin_active value indicates whether ref pin is connected and has been configured
    bool const ref_pin_active = ((input_buffer_1 == NULL) || ((data_size_1/input1_channels_count_) < data_size_per_channel))  ? false : true;

    // If reference pin is not connected or module is in bypass mode,
    // set local_input1_channel_count to 0.
    // This allows to skip reference pin content in the processing loop
    size_t local_input1_channel_count = (ref_pin_active && (processing_mode_ == ProcessingMode::NORMAL) ) ? input1_channels_count_ : 0;

    // Input not connected.
    if (input_buffer_0 == NULL) {
        output_stream_buffers[0].size = 0;
        return PROCESS_SUCCEED;
    }

    // Apply processing of the input chunks and generate the output chunk
    int32_t divider_input_0 = config_.divider_input_0;
    int32_t divider_input_1 = config_.divider_input_1;

    if (processing_mode_ == ProcessingMode::BYPASS) {
        divider_input_0 = input0_channels_count_;
        // local_input1_channel_count is already set to 0 in BYPASS mode
    }

    if (bits_per_sample_ == DEPTH_16BIT)
    {
        int16_t const* input_buffer16_0 = reinterpret_cast<int16_t const*>(input_buffer_0);
        int16_t const* input_buffer16_1 = reinterpret_cast<int16_t const*>(input_buffer_1);
        int16_t* output_buffer16 = reinterpret_cast<int16_t*>(output_buffer);

        for (size_t i = 0 ; i < output_data_size / output_channels_count_ / sizeof(int16_t) ; i++)
        {
            int32_t mixed_sample = 0;
            for (size_t k = 0; k < input0_channels_count_; k++)
                mixed_sample += ((int32_t) input_buffer16_0[input0_channels_count_*i + k]/divider_input_0);
            for (size_t k = 0; k < local_input1_channel_count; k++)
                mixed_sample += ((int32_t) input_buffer16_1[local_input1_channel_count*i + k]/divider_input_1);
            for (size_t k = 0; k < output_channels_count_; k++)
                output_buffer16[output_channels_count_*i + k] = (int16_t) mixed_sample;
        }
    }

    if (bits_per_sample_ == DEPTH_32BIT)
    {
        int32_t const* input_buffer32_0 = reinterpret_cast<int32_t const*>(input_buffer_0);
        int32_t const* input_buffer32_1 = reinterpret_cast<int32_t const*>(input_buffer_1);
        int32_t* output_buffer32 = reinterpret_cast<int32_t*>(output_buffer);

        for (size_t i = 0 ; i < output_data_size / output_channels_count_ / sizeof(int32_t) ; i++)
        {
            int64_t mixed_sample = 0;
            for (size_t k = 0; k < input0_channels_count_; k++)
                mixed_sample += ((int64_t) input_buffer32_0[input0_channels_count_*i + k]/divider_input_0);
            for (size_t k = 0; k < local_input1_channel_count; k++)
                mixed_sample += ((int64_t) input_buffer32_1[local_input1_channel_count*i + k]/divider_input_1);
            for (size_t k = 0; k < output_channels_count_; k++)
                output_buffer32[output_channels_count_*i + k] = (int32_t) mixed_sample;
        }
    }

    // Update output buffer data size
    output_stream_buffers[0].size = output_data_size;

    return PROCESS_SUCCEED;
}

ErrorCode::Type DownmixerModule::SetConfiguration(
        uint32_t config_id,
        ConfigurationFragmentPosition fragment_position,
        uint32_t data_offset_size,
        const uint8_t *fragment_block,
        size_t fragment_size,
        uint8_t *response,
        size_t &response_size)
{
    const DownmixerConfig *cfg =
        reinterpret_cast<const DownmixerConfig *>(fragment_block);

    LOG_MESSAGE(LOW, "SetConfiguration: "
                "config_id = %d, data_offset_size = %d, fragment_size = %d",
                LOG_ENTRY, config_id, data_offset_size, fragment_size);

    if ( (cfg->divider_input_0 == 0) || (cfg->divider_input_1 == 0) )
    {
        return ErrorCode::INVALID_CONFIGURATION;
    }
    else
    {
        config_.divider_input_0 = cfg->divider_input_0;
        config_.divider_input_1 = cfg->divider_input_1;
        LOG_MESSAGE(LOW, "SetConfiguration: divider_input_0 = %d, divider_input_1 = %d", LOG_ENTRY, config_.divider_input_0, config_.divider_input_1);
        return ErrorCode::NO_ERROR;
    }
}

ErrorCode::Type DownmixerModule::GetConfiguration(
        uint32_t config_id,
        ConfigurationFragmentPosition fragment_position,
        uint32_t &data_offset_size,
        uint8_t *fragment_buffer,
        size_t &fragment_size)
{

    DownmixerConfig *cfg =
        reinterpret_cast<DownmixerConfig *>(fragment_buffer);

    LOG_MESSAGE(LOW, "GetConfiguration: config_id(%d)", LOG_ENTRY, config_id);

    cfg->divider_input_0 = config_.divider_input_0;
    cfg->divider_input_1 = config_.divider_input_1;
    data_offset_size = sizeof(DownmixerConfig);
    return ErrorCode::NO_ERROR;

}

void DownmixerModule::SetProcessingMode(ProcessingMode mode)
{
    LOG_MESSAGE(LOW, "SetProcessingMode", LOG_ENTRY);

    // Store module mode
    processing_mode_ = mode;
}

ProcessingMode DownmixerModule::GetProcessingMode()
{
    LOG_MESSAGE(LOW, "GetProcessingMode", LOG_ENTRY);

    return processing_mode_;
}

void DownmixerModule::Reset()
{
    LOG_MESSAGE(LOW, "Reset", LOG_ENTRY);
    processing_mode_ = ProcessingMode::NORMAL;
}
