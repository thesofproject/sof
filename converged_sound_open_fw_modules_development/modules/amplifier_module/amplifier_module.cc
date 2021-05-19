// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.

#if defined(XTENSA_TOOLSCHAIN) || defined(__XCC__)
#include <xtensa/tie/xt_hifi2.h>
#else
#include <q_format.h>
#endif

#include "amplifier_module.h"

#include <logger.h>

using namespace intel_adsp;

const AmplifierConfig bypass_config_ = {0x7FFF, 0x0, 0xFF, AMPLIFIER_GAIN_0DB};

DECLARE_LOADABLE_MODULE(AmplifierModule,
                        AmplifierModuleFactory)


ErrorCode::Type AmplifierModuleFactory::Create(
    SystemAgentInterface &system_agent,
    ModulePlaceholder *module_placeholder,
    ModuleInitialSettings initial_settings
    )
{
    // count of input pins formats retrieved from the ModuleInitialSettings container
    const size_t in_pins_format_count = initial_settings.GetItem<IN_PINS_FORMAT>().GetLength();
    // count of output pins formats retrieved from the ModuleInitialSettings container
    const size_t out_pins_format_count = initial_settings.GetItem<OUT_PINS_FORMAT>().GetLength();

    LOG_MESSAGE(    LOW, "Create, in_pin = %d, out_pins = %d",
                    LOG_ENTRY, in_pins_format_count, out_pins_format_count);

    // check that one audio format is available for the input pin
    if (in_pins_format_count != 1)
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

    OutputPinFormat const& output_pin_format = initial_settings.GetItem<OUT_PINS_FORMAT>()[0];
    // check that output audio format is for output pin0
    if (output_pin_format.pin_index != 0)
    {
        LOG_MESSAGE(    CRITICAL, "Retrieved audio format is associated to an invalid output pin index (%d)",
                        LOG_ENTRY, output_pin_format.pin_index);
        return ErrorCode::INVALID_SETTINGS;
    }

    InputPinFormat const& input_pin_format = initial_settings.GetItem<IN_PINS_FORMAT>()[0];
    // check that audio format retrieved for input is assigned to an existing module pin.
    if (input_pin_format.pin_index != 0)
    {
        LOG_MESSAGE(    CRITICAL, "Retrieved audio format is associated to an invalid input pin index (%d)",
                        LOG_ENTRY, input_pin_format.pin_index);
        return ErrorCode::INVALID_SETTINGS;
    }

    // check that input pin 0 and output pin 0 have compatible audio format
    if ((input_pin_format.audio_fmt.sampling_frequency != output_pin_format.audio_fmt.sampling_frequency) ||
        (input_pin_format.audio_fmt.bit_depth != output_pin_format.audio_fmt.bit_depth))
    {
        LOG_MESSAGE(    CRITICAL, "Input pin0 and output pin0 formats have incompatible audio format:"
                        "input_freq = %d, output_freq = %d, "
                        "input_bit_depth = %d, output_bit_depth = %d."
                        ,
                        LOG_ENTRY, input_pin_format.audio_fmt.sampling_frequency, output_pin_format.audio_fmt.sampling_frequency,
                        input_pin_format.audio_fmt.bit_depth, output_pin_format.audio_fmt.bit_depth);
        return ErrorCode::INVALID_SETTINGS;
    }

    // check that input pin 0 and output pin 0 have same channels count
    if (input_pin_format.audio_fmt.number_of_channels != output_pin_format.audio_fmt.number_of_channels)
    {
        LOG_MESSAGE(    CRITICAL, "Input pin0 and output pin0 formats have different channels counts:"
                        "input_ch_count = %d, output_ch_count = %d.",
                        LOG_ENTRY, input_pin_format.audio_fmt.number_of_channels, output_pin_format.audio_fmt.number_of_channels);
         return ErrorCode::INVALID_SETTINGS;
    }

    // check that bit_depth value is supported
    if ((output_pin_format.audio_fmt.bit_depth != DEPTH_16BIT) &&
        (output_pin_format.audio_fmt.bit_depth != DEPTH_32BIT))
    {
        LOG_MESSAGE(    CRITICAL, " bit depth in audio format is not supported (%d)",
                        LOG_ENTRY, output_pin_format.audio_fmt.bit_depth);
        return ErrorCode::INVALID_SETTINGS;
    }

    // check that ibs can be divided by the bytes size of "samples group"
    if ((input_pin_format.ibs * 8) % (input_pin_format.audio_fmt.bit_depth * input_pin_format.audio_fmt.number_of_channels))
    {
        LOG_MESSAGE(    CRITICAL, "ibs*8 shall be a multiple of samples group value:"
                        "ibs = %d, input_bit_depth = %d.",
                        LOG_ENTRY, input_pin_format.ibs, input_pin_format.audio_fmt.bit_depth);
         return ErrorCode::INVALID_SETTINGS;
    }

    // check that obs can be divided by the bytes size of "samples group"
    if ((output_pin_format.obs * 8) % (output_pin_format.audio_fmt.bit_depth * output_pin_format.audio_fmt.number_of_channels))
    {
        LOG_MESSAGE(    CRITICAL, "obs*8 shall be a multiple of samples group value"
                        "obs = %d, otput_but_depth = %d.",
                        LOG_ENTRY, output_pin_format.obs, output_pin_format.audio_fmt.bit_depth);
        return ErrorCode::INVALID_SETTINGS;
    }

    // Initializes AmplifierModule instance using the
    // "placement syntax" of operator new
    new (module_placeholder)AmplifierModule(
        output_pin_format.audio_fmt.number_of_channels,
        output_pin_format.audio_fmt.bit_depth,
        system_agent);

    return ErrorCode::NO_ERROR;
}

#ifdef NOTIFICATION_SUPPORT
AmplifierModule::InternalError AmplifierModule::SendNotification(const AmplifierConfig &current_config)
{
    InternalError ec = PROCESS_SUCCEED;
    TargetGainNotification* notification_data =
        notification_message_.GetNotification<TargetGainNotification>(0, /* notification id */
                                                                      GetSystemService()); /* system_service reference */
    if (notification_data != NULL)
    {
        notification_data->gain_reached = gain_value_;
        notification_data->factor =(uint32_t)current_config.smoothing_factor;
        notification_data->time_to_reach = time_to_reach_;
        notification_message_.Send(GetSystemService());
    }
    else
    {
        ec = PROCESS_NOTIFICATION_ERROR;
    }

    return ec;
}
#endif //#ifdef NOTIFICATION_SUPPORT

void AmplifierModule::UpdateGain(const AmplifierConfig &current_config)
{
    int new_gain_value;

    // Ensure a smooth convergence to target gain
    if (gain_value_ > current_config.target_gain) {
        new_gain_value = current_config.target_gain * (current_config.smoothing_factor >> 3) +
                         (0x1000u - (current_config.smoothing_factor >> 3)) * gain_value_;
        new_gain_value = new_gain_value >> 12;
        if (new_gain_value == gain_value_) {
            gain_value_--;
        } else {
            gain_value_ = new_gain_value;
        }
    } else if (gain_value_ < current_config.target_gain) {
        new_gain_value = current_config.target_gain * (current_config.smoothing_factor >> 3) +
                         (0x1000u - (current_config.smoothing_factor >> 3)) * gain_value_;
        new_gain_value = new_gain_value >> 12;
        if (new_gain_value == gain_value_) {
            gain_value_++;
        } else {
            gain_value_ = new_gain_value;
        }
    }

    if (gain_value_ == current_config.target_gain) {
        target_gain_reached_ = true;
    }
}

#ifndef GCC_TOOLSCHAIN 
// size is in number of sample groups
void AmplifierModule::Process16(uint8_t **input_buffer, uint8_t **output_buffer, size_t size)
{
    ae_int16x4 d1, d2;
    const ae_int16 *ae_dr_ptr1 = (const ae_int16 *)*input_buffer;
    const ae_int16 *ae_dr_ptr2 = (const ae_int16 *)&gain_value_;
    ae_int32x2 tmp32;
    ae_int16x4 tmp16;
    int16_t result;
    ae_int16 *result_ptr = (ae_int16 *)&result;
    int32_t nb_samples = size * num_channels_;
    int32_t nb_bytes = nb_samples * 2;

    for (int i = 0; i < (int) nb_samples; i++) {
        // Load gain and input in AE_DR register
        d1 = AE_L16_X(ae_dr_ptr1, i * 2);
        d2 = AE_L16_I(ae_dr_ptr2, 0);
        // Four way SIMD 16x16-bit into 32-bit integer signed MAC without saturation
        AE_MUL16X4(tmp32, tmp32, d1, d2);
        // Shift right arithmetic (sign-extending) by 12 due to gain Q3.12 format
        tmp32 =  AE_SRAI32(tmp32, 12);
        // Saturate the four 32-bit integral values in AE_DR registers d0
        // and d1 to a 16-bit integral value
        tmp16 = AE_SAT16X4(tmp32, tmp32);
        // Store the 16-bit 0 element of the AE_DR register to memory
        AE_S16_0_I(tmp16, result_ptr, 0);
        ((int16_t *)*output_buffer)[i] = result;
    }
    *input_buffer += nb_bytes;
    *output_buffer += nb_bytes;
}
#else
void AmplifierModule::Process16(uint8_t **input_buffer, uint8_t **output_buffer, size_t size)
{
    const int16_t *in_buf = (const int16_t *)*input_buffer;
    const int16_t gain = (const int16_t)gain_value_;
    int16_t result;

    int32_t nb_samples = size * num_channels_;
    int32_t nb_bytes = nb_samples * 2;

    for (int i = 0; i < (int) nb_samples; i++) {
        // Q1.15 * Q3.12
        result = q_mults_sat_16x16(*in_buf, gain, Q_SHIFT_BITS_32(15, 12, 15));
        ((int16_t *)*output_buffer)[i] = result;
        in_buf++;
    }

    *input_buffer += nb_bytes;
    *output_buffer += nb_bytes;
}
#endif

#ifndef GCC_TOOLSCHAIN 
// size is in number of sample groups
void AmplifierModule::Process32(uint8_t **input_buffer, uint8_t **output_buffer, size_t size)
{
    ae_int32x2 d1, d2;
    const ae_int32 *ae_dr_ptr1 = (const ae_int32 *)*input_buffer;
    const ae_int32 *ae_dr_ptr2 = (const ae_int32 *)&gain_value_;
    ae_int64 tmp64;
    ae_int32x2 tmp32;
    int a = 32;
    int32_t result;
    ae_int32 *result_ptr = (ae_int32 *)&result;
    int32_t nb_samples = size * num_channels_;
    int32_t nb_bytes = nb_samples * 4;

    for (int i = 0; i < (int) nb_samples; i++) {
        // Load gain and input in AE_DR register
        d1 = AE_L32_X(ae_dr_ptr1, i * 4);
        d2 = AE_L32_I(ae_dr_ptr2, 0);
        // Single 32x32-bit into 64-bit signed integer MAC with no saturation
        tmp64 = AE_MUL32_LL(d1, d2);
        // Shift right arithmetic (sign-extending) by 12 due to gain Q3.12 format
        tmp64 =  AE_SRAI64(tmp64, 12);
        // Shift left 64-bit values from AE_DR register by 'a'
        // Shifted value is saturated 64 bits and the 32 MSBs are stored
        // in the two 32-bit elements of AE_DR register
        tmp32 = AE_TRUNCA32F64S(tmp64, a);
        // Store the 32-bit L element of the AE_DR register to memory
        AE_S32_L_I(tmp32, result_ptr, 0);
        ((int32_t *)*output_buffer)[i] = result;
    }
    *input_buffer += nb_bytes;
    *output_buffer += nb_bytes;
}
#else
void AmplifierModule::Process32(uint8_t **input_buffer, uint8_t **output_buffer, size_t size)
{
    const int32_t *in_buf = (const int32_t *)*input_buffer;
    const int16_t gain = (const int16_t)gain_value_;
    int32_t result;
    int32_t nb_samples = size * num_channels_;
    int32_t nb_bytes = nb_samples * 4;

    for (int i = 0; i < (int) nb_samples; i++) {
        /* Below q_mults_sat_32x32 function will not be compiled with xtensa 
         * compilator as it does not have e.g. ashrdi3 built in functions */
        result = q_mults_sat_32x32(*in_buf, gain, Q_SHIFT_BITS_64(31, 12, 31));
        ((int32_t *)*output_buffer)[i] = result;
        in_buf++;
    }

    *input_buffer += nb_bytes;
    *output_buffer += nb_bytes;
}
#endif

// Note that purpose of the source code presented below is to demonstrate usage
// of the ADSP System API.
// It might not be optimized enough for efficient computation.
uint32_t AmplifierModule::Process(
    InputStreamBuffer *input_stream_buffers,
    OutputStreamBuffer *output_stream_buffers)
{
    InternalError ec = PROCESS_SUCCEED;
    uint8_t *input_buffer = input_stream_buffers[0].data;
    size_t data_size = input_stream_buffers[0].size;
    uint8_t *output_buffer = output_stream_buffers[0].data;
    const AmplifierConfig &current_config  = (processing_mode_ == ProcessingMode::BYPASS) ? bypass_config_ : config_;

    if ((input_buffer != NULL) && (output_buffer != NULL)) {
        // If module is in steady BYPASS mode (gain stabilized to value 1), just copy input to output
        if ((processing_mode_ == ProcessingMode::BYPASS) && (gain_value_ == AMPLIFIER_GAIN_0DB)) {
            for (int i = 0; i < (int)(data_size / sizeof(int8_t)); i++) {
                output_buffer[i] = input_buffer[i];
            }
        }
        // Else apply NORMAL mode amplification
        else {
            // processing is managed on sample groups (sg means sample group)
            size_t data_size_sg = (data_size * 8) / (bits_per_sample_ * num_channels_);
            size_t processed_data_size_sg = 0;

            while (processed_data_size_sg != data_size_sg) {
                size_t nb_sg_to_process;

                if ((!target_gain_reached_) && (position_ == 0)) {
                    //update gain before processing a new window
                    UpdateGain(current_config);
#ifdef NOTIFICATION_SUPPORT
                    //Send Notification when target gain is reached (no notification in bypass)
                    if (target_gain_reached_ && (processing_mode_ != ProcessingMode::BYPASS)) {
                        ec = SendNotification(current_config);
                    }
#endif
                }
                if (target_gain_reached_) {
                    nb_sg_to_process = data_size_sg - processed_data_size_sg;
                } else {
                    nb_sg_to_process = MIN(PROCESSING_BLOCK_SIZE - position_,
                                                        data_size_sg - processed_data_size_sg);
                }

                // Apply processing of the input chunk and generate the output chunk
                if (bits_per_sample_ == 16) {
                    Process16(&input_buffer, &output_buffer, nb_sg_to_process);
                } else {
                    Process32(&input_buffer, &output_buffer, nb_sg_to_process);
                }
                processed_data_size_sg += nb_sg_to_process;
                position_ += nb_sg_to_process;
                position_ = (position_ % PROCESSING_BLOCK_SIZE);
            }
            if (target_gain_reached_) {
                time_to_reach_ = 0;
            } else {
                time_to_reach_++;//number of process data to reach target gain
            }
        }
    }

    // Update output buffer data size
    output_stream_buffers[0].size = data_size;

    return (uint32_t)ec;
}

ErrorCode::Type AmplifierModule::SetConfiguration(
    uint32_t config_id,
    ConfigurationFragmentPosition fragment_position,
    uint32_t data_offset_size,
    const uint8_t *fragment_block,
    size_t fragment_size,
    uint8_t *response,
    size_t &response_size)
{
    const AmplifierConfig *cfg =
        reinterpret_cast<const AmplifierConfig *>(fragment_block);

    LOG_MESSAGE(LOW, "SetConfiguration(config_id = %d, data_offset_size = %d, fragment_size = %d)",
                LOG_ENTRY, config_id, data_offset_size, fragment_size);

    config_.min_gain = cfg->min_gain;
    config_.max_gain = cfg->max_gain;
    config_.smoothing_factor = cfg->smoothing_factor;
    config_.target_gain = (cfg->target_gain > config_.max_gain) ? config_.max_gain :
                          (cfg->target_gain < config_.min_gain) ? config_.min_gain :
                          cfg->target_gain;

    time_to_reach_ = 0;
    target_gain_reached_ = false;
    position_ = 0;

    LOG_MESSAGE(LOW, "SetConfiguration(target_gain = %d)", LOG_ENTRY, config_.target_gain);
    return ErrorCode::NO_ERROR;
}

ErrorCode::Type AmplifierModule::GetConfiguration(
    uint32_t config_id,
    ConfigurationFragmentPosition fragment_position,
    uint32_t &data_offset_size,
    uint8_t *fragment_buffer,
    size_t &fragment_size)
{

    AmplifierConfig *cfg =
        reinterpret_cast<AmplifierConfig *>(fragment_buffer);

    LOG_MESSAGE(LOW, "GetConfiguration(config_id = %d)", LOG_ENTRY, config_id);

    cfg->min_gain = config_.min_gain;
    cfg->max_gain = config_.max_gain;
    cfg->smoothing_factor = config_.smoothing_factor;
    cfg->target_gain = config_.target_gain;
    data_offset_size = sizeof(AmplifierConfig);
    return ErrorCode::NO_ERROR;

}

void AmplifierModule::SetProcessingMode(ProcessingMode mode)
{
    LOG_MESSAGE(LOW, "SetProcessingMode", LOG_ENTRY);

    // Store module mode
    processing_mode_ = mode;
    time_to_reach_ = 0;
    target_gain_reached_ = false;
    position_ = 0;
}

ProcessingMode AmplifierModule::GetProcessingMode()
{
    LOG_MESSAGE(LOW, "GetProcessingMode()", LOG_ENTRY);

    return processing_mode_;
}

void AmplifierModule::Reset()
{
    LOG_MESSAGE(LOW, "Reset", LOG_ENTRY);

    gain_value_ = AMPLIFIER_GAIN_0DB;
    processing_mode_ = ProcessingMode::NORMAL;
    time_to_reach_ = 0;
    target_gain_reached_ = false;
    position_ = 0;

    // Leave config_ parameters unchanged.
}
