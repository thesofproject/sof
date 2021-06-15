// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
/*! \file module_initial_settings.h */


#ifndef _ADSP_IPC_MODULE_INITIAL_SETTINGS_H_
#define _ADSP_IPC_MODULE_INITIAL_SETTINGS_H_


#include <stdint.h>

#ifdef __cplusplus
namespace intel_adsp
{

    /*! \brief List of supported sampling frequencies. */
    enum SamplingFrequency
    {
        FS_8000HZ   = 8000,
        FS_11025HZ  = 11025,
        FS_12000HZ  = 12000, /** Mp3, AAC, SRC only. */
        FS_16000HZ  = 16000,
        FS_18900HZ  = 18900, /** SRC only for 44100 */
        FS_22050HZ  = 22050,
        FS_24000HZ  = 24000, /** Mp3, AAC, SRC only. */
        FS_32000HZ  = 32000,
        FS_37800HZ  = 37800, /** SRC only for 44100 */
        FS_44100HZ  = 44100,
        FS_48000HZ  = 48000, /**< Default. */
        FS_64000HZ  = 64000, /** AAC, SRC only. */
        FS_88200HZ  = 88200, /** AAC, SRC only. */
        FS_96000HZ  = 96000, /** AAC, SRC only. */
        FS_176400HZ = 176400, /** SRC only. */
        FS_192000HZ = 192000, /** SRC only. */
        FS_INVALID
    };


    /*! \brief List of supported bit depths. */
    enum BitDepth
    {
        DEPTH_8BIT  = 8, /**< 8 bits depth */
        DEPTH_16BIT = 16, /**< 16 bits depth */
        DEPTH_24BIT = 24, /**< 24 bits depth - Default */
        DEPTH_32BIT = 32, /**< 32 bits depth */
        DEPTH_64BIT = 64, /**< 64 bits depth */
        DEPTH_INVALID
    };


    /*! \brief List of supported styles of interleaving. */
    enum InterleavingStyle
    {
        CHANNELS_SAMPLES_INTERLEAVING = 0, /*!< Given Si_CHj the ith sample of the jth channel,
                                            * m the count of samples per channel and n the count of channels,
                                            * the layout of the stream frame buffer will look as follow :
                                            * [S1_CH1,...,S1_CHi,...,S1_CHn,S2_CH1,...,S2_CHn,...,Sm_CH1,...,Sm_CHn] */
        CHANNELS_BLOCKS_INTERLEAVING = 1, /*!< \internal [S1_CH1...Sm_CH1,...,S1_chn...SM_CHn]. \Warning Not supported in current release. */
    };

/*! \brief bits field map which helps to describe each channel location a the data stream buffer */
typedef uint32_t ChannelMap;
#if 0 //WIP
    /*! \brief Descriptor about channel assignment per data frame slot
     *
     * Samples in data stream can be gathered in a data frame.
     * A data frame can be divided up to 8 samples slot.
     * Each sample slot is associated to a channel ID.
     */
    class ChannelMap
    {
    private:
        enum
        {
            SLOT_BITS_LENTGH = 4
        };
    public:
        /*! WIP */
        enum
        {
            MAX_CHANNEL_SLOTS = sizeof(uint32_t)*8/SLOT_BITS_LENTGH
        };
        /*! WIP */
        struct SlotPosition
        {
            /*! WIP */
            SlotPosition(char val)
            { value = value; }

            /*! WIP */
            char value;
        };

        /*! WIP */
        ChannelMap(): value_(0xFFFFFFFFu){}
        /*! WIP */
        ChannelMap(ChannelMap const& ref): value_(ref.value_){}
        /*! WIP */
        ChannelMap(uint32_t const& value): value_(value){}
        /*! WIP */
        operator uint32_t(){ return value_; }
        /*! WIP */
        operator uint32_t() const { return value_; }
        /*! WIP */
        ChannelMap& operator= (uint32_t const & value)
        { value_ = value; return *this; }
        /*! WIP */
        ChannelMap& operator>>= (SlotPosition const & slot_shift)
        { value_ >>= slot_shift.value*SLOT_BITS_LENTGH; return *this; }
        /*! WIP */
        unsigned int GetLastChannelId()
        { return ((unsigned int) value_) & ~(-1 << SLOT_BITS_LENTGH); }

    private:
        uint32_t value_;
    };
#endif

    /*! \brief List of supported channel maps. */
    enum ChannelConfig
    {
        CHANNEL_CONFIG_MONO      = 0, /**< One channel only. */
        CHANNEL_CONFIG_STEREO    = 1, /**< L & R. */
        CHANNEL_CONFIG_2_POINT_1 = 2, /**< L, R & LFE; PCM only. */
        CHANNEL_CONFIG_3_POINT_0 = 3, /**< L, C & R; MP3 & AAC only. */
        CHANNEL_CONFIG_3_POINT_1 = 4, /**< L, C, R & LFE; PCM only. */
        CHANNEL_CONFIG_QUATRO    = 5, /**< L, R, Ls & Rs; PCM only. */
        CHANNEL_CONFIG_4_POINT_0 = 6, /**< L, C, R & Cs; MP3 & AAC only. */
        CHANNEL_CONFIG_5_POINT_0 = 7, /**< L, C, R, Ls & Rs. */
        CHANNEL_CONFIG_5_POINT_1 = 8, /**< L, C, R, Ls, Rs & LFE. */
        CHANNEL_CONFIG_DUAL_MONO = 9, /**< One channel replicated in two. */
        CHANNEL_CONFIG_I2S_DUAL_STEREO_0 = 10, /**< Stereo (L,R) in 4 slots, 1st stream: [ L, R, -, - ] */
        CHANNEL_CONFIG_I2S_DUAL_STEREO_1 = 11, /**< Stereo (L,R) in 4 slots, 2nd stream: [ -, -, L, R ] */
        CHANNEL_CONFIG_7_POINT_1 = 12, /**< L, C, R, Ls, Rs & LFE., LS, RS */
        CHANNEL_CONFIG_INVALID
    };


    /*! \brief list possible sample types */
    enum SampleType
    {
        MSB_INTEGER = 0, /*!< integer with Most Significant Byte first */
        LSB_INTEGER = 1, /*!< integer with Least Significant Byte first */
        SIGNED_INTEGER = 2, /*!< signed integer */
        UNSIGNED_INTEGER = 3, /*!< unsigned integer */
        FLOAT = 4 /*!< unsigned integer */
    };

    #pragma pack(4)
    /*!
     * \brief Descriptor of the audio data format which can stream through the ProcessingModuleInterface objects.
     */
    struct AudioFormat
    {
        SamplingFrequency sampling_frequency; /*!< Sampling frequency in Hz */
        BitDepth bit_depth; /*!< Bit depth of audio samples */
        ChannelMap channel_map; /*!< channel ordering in audio stream */
        ChannelConfig channel_config; /*!< Channel configuration. */
        InterleavingStyle interleaving_style; /*!< The way the samples are interleaved */
        uint32_t number_of_channels : 8; /*!< Total number of channels. */
        uint32_t valid_bit_depth : 8; /*!< Valid bit depth in audio samples */
        SampleType sample_type : 8; /*!< sample type:
                                     *  0 - intMSB
                                     *  1 - intLSB
                                     *  2 - intSinged
                                     *  3 - intUnsigned
                                     *  4 - float  */
        uint32_t reserved : 8;  /*!< padding byte */
    };


    /*! \brief The Legacy set of settings for initialization of a Module instance.
     *
     *  \remarks This LegacyModuleInitialSettings struct defines also the structure of the IPC message passed from host to the ProcessingModuleInterface.
     * To avoid machine-dependent mapping of the structure in memory the alignment constraint of the structure is enforced with use of "#pragma pack"
     *  \deprecated This class will be removed in futur version of the API.
     * Client code should now work with one of the other data associated to the ModuleInitialSettingsKey values.
     */
    struct LegacyModuleInitialSettings
    {
        /*!
         * \brief Indicates the max count of Cycles Per Chunk which are granted to a certain module
         * to complete the processing of its input and output buffers.
         * (during ProcessingModuleInterface::Process() execution)
         *
         * \remarks Working with CPC in a custom processing module is quite advanced. One might imagine for example that
         * a module could adapt the complexity of its algorithm based on the CPC value.
         */
        uint32_t cpc;
        uint32_t ibs;   /*!< \brief Input Buffer Size (in bytes) that module shall process (within ProcessingModuleInterface::Process()) from every connected input pin. */
        uint32_t obs;  /*!< \brief Output Buffer Size (in bytes) that module shall produce (within ProcessingModuleInterface::Process()) on every connected output pin. */
        /*!
         * \brief internal purpose.
         * \internal Number of physical pages that needed to be preallocated for module outside of pipeline.
         * Depends on type of module AND target module configuration.
         * Note: some modules require this param to be set to 0.
         */
        uint32_t is_pages;
        AudioFormat audio_fmt; /*!< \brief Specifies the format of the input data stream(s) processed by the module. */
    };


    /*! \brief Trait of a module input pin.
     */
    struct InputPinFormat
    {
        uint32_t            pin_index; /*!< \brief Index of the pin.*/
        uint32_t            ibs;       /*!< \brief Specifies input frame size (in bytes).*/
        AudioFormat         audio_fmt; /*!< \brief Format of the input data.*/
    };

    /*! \brief Trait of a module output pin.
     */
    struct OutputPinFormat
    {
        uint32_t            pin_index; /*!< \brief Index of the pin.*/
        uint32_t            obs;       /*!< \brief Specifies output frame size (in bytes).*/
        AudioFormat         audio_fmt; /*!< \brief Format of the output data.*/
    };
    #pragma pack() //restore default configuration of the packing alignment
}

#endif // #ifdef __cplusplus

#endif //_ADSP_IPC_MODULE_INITIAL_SETTINGS_H_
