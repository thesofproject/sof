/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Waves Audio Ltd. All rights reserved.
 */
/*******************************************************************************
 * @page CoreConcepts_DataPath Data Path
 *
 * Data path is a collection of components and functions used in processing.
 *
 * **Sample** is a signal value at some point, irrespective of the number of
 *   channels. Thus, for mono signal sample value is a single data element but
 *   for multiple channels, sample is a collection of data values,
 *   one per channel.
 *
 * **Frame** is a sequence of **samples** to be processed.
 *
 * ![Interleaved stereo buffer]
 * (CoreConcepts/DataPath/MaxxBuffer_StereoFrame.svg)
 *
 * @section CoreConcepts_DataPath_Streams Data Streams
 * All @ref CoreConcepts_MaxxEffect instances receive frames from input streams
 *   and send processed frames to output streams. Stream example is shown below.
 *
 * ![Data stream and the corresponding format]
 * (CoreConcepts/DataPath/MaxxStream_DeinterleavedStereo.svg)
 *
 * @subsection CoreConcepts_DataPath_Stream Stream
 * @ref MaxxStream_t contains information about all used @ref MaxxBuffer_t,
 *   and available/processed samples count. @ref MaxxEffect_t handler might
 *   require a specific frame length to be available in the stream.
 *
 * @subsection CoreConcepts_DataPath_StreamFormat Stream Format
 * Expected streams formats must be defined during @ref Initialization with
 *   @ref MaxxStreamFormat_t. It holds information about @ref MaxxStream_t
 *   configuration such as
 *   [sampling rate](@ref MaxxStreamFormat_t.sampleRate),
 *   [number of channels](@ref MaxxStreamFormat_t.numChannels),
 *   [format](@ref MaxxStreamFormat_t.samplesFormat), and
 *   [layout](@ref MaxxStreamFormat_t.samplesLayout).
 *
 * @subsection CoreConcepts_DataPath_Buffer Buffer
 * @ref MaxxBuffer_t is a pointer to the continuous memory region which
 *   is used for storing data values. Buffers can contain audio, IV sensors,
 *   head-tracking data, etc.
 *
 * @subsection CoreConcepts_DataPath_MultichannelStreams Multichannel Streams
 * Multichannel data can be represented in interleaved or deinterleaved manner.
 *   In interleaved stream single buffer is used to store all channels, whereas
 *   in deinterleaved stream several buffers are used, one for each channel.
 *   @ref MaxxBuffer_Layout_t defines buffer layout for multichannel data. This
 *   field is ignored for single-channel data.
 ******************************************************************************/
#ifndef MAXX_STREAM_H
#define MAXX_STREAM_H

#include <stdint.h>

/**
 * An array of signal values with the @ref MaxxBuffer_Format_t format.
 *
 * @see @ref CoreConcepts_DataPath_Buffer
 */
typedef void* MaxxBuffer_t;


/**
 * Defines data encoding format of @ref MaxxBuffer_t elements.
 */
typedef enum
{
    MAXX_BUFFER_FORMAT_Q1_15    = 0,  /**< PCM Q15 */
    MAXX_BUFFER_FORMAT_Q9_23    = 1,  /**< PCM Q23 in 32 bit container */
    MAXX_BUFFER_FORMAT_Q1_31    = 2,  /**< PCM Q31 */
    MAXX_BUFFER_FORMAT_FLOAT    = 3,  /**< FLOAT   */
    MAXX_BUFFER_FORMAT_Q5_27    = 4,  /**< PCM Q27 */
    MAXX_BUFFER_FORMAT_Q1_23    = 5,  /**< PCM Q23 */

    MAXX_BUFFER_FORMAT_FORCE_SIZE = INT32_MAX
}   MaxxBuffer_Format_t;

/**
 * Defines buffers layout inside of the @ref MaxxStream_t.
 *
 * @see @ref CoreConcepts_DataPath_MultichannelStreams
 */
typedef enum
{
    MAXX_BUFFER_LAYOUT_INTERLEAVED      = 0, /**< Interleaved buffer */
    MAXX_BUFFER_LAYOUT_DEINTERLEAVED    = 1, /**< Deinterleaved buffer */

    MAXX_BUFFER_LAYOUT_FORCE_SIZE       = INT32_MAX
}   MaxxBuffer_Layout_t;


/**
 * Defines @ref CoreConcepts_DataPath_StreamFormat.
 */
typedef struct
{
    uint32_t            sampleRate;     /**< Sampling rate in Hz */
    uint32_t            numChannels;    /**< Channels count */
    MaxxBuffer_Format_t samplesFormat;  /**< Data format */
    MaxxBuffer_Layout_t samplesLayout;  /**< Data layout */

    uint32_t            frameSize;      /**< Minimum available samples count */
}   MaxxStreamFormat_t;


/**
 * A @ref CoreConcepts_DataPath_Stream with the @ref MaxxStreamFormat_t format.
 */
typedef struct
{
    /**
    * For @ref MAXX_BUFFER_LAYOUT_INTERLEAVED array length must be 1.
    * For @ref MAXX_BUFFER_LAYOUT_DEINTERLEAVED must be equal to the
    * [number of channels](@ref MaxxStreamFormat_t.numChannels).
    */
    MaxxBuffer_t*   buffersArray;

    /** number of available samples in data buffers. */
    uint32_t        numAvailableSamples;

    /** number of processed samples in data buffers. */
    uint32_t        numProcessedSamples;

    /** maximum number of samples which buffers can contain. */
    uint32_t        maxNumSamples;
}   MaxxStream_t;


#endif
