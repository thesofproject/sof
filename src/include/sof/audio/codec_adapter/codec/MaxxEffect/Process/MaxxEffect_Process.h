/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Waves Audio Ltd. All rights reserved.
 */
/*******************************************************************************
 * @page Processing Processing
 *
 * Refer to the diagram below for the processing workflow.
 *
 * ![Processing workflow](Process/MaxxEffect-ProcessingWorkflow.svg)
 *
 * Example code for the interleaved stereo streams processing at 48 kHz with
 *   Q1.31 fixed-point data samples:
 *
 * ~~~{.c}
 * // 10 ms at 48 kHz
 * #define BUFFER_SAMPLES   (480)
 *
 * // Interleaved buffer with 2 Q31 channels
 * int32_t samplesBuffer[2 * BUFFER_SAMPLES];
 *
 * // Format used for initialization
 * // Identical for both input and output streams
 * MaxxStreamFormat_t expectedFormat = {
 *     .sampleRate     = 48000,
 *     .numChannels    = 2,
 *     .samplesFormat  = MAXX_BUFFER_FORMAT_Q1_31,
 *     .samplesLayout  = MAXX_BUFFER_LAYOUT_INTERLEAVED
 * };
 *
 * // Prepare I/O streams
 * MaxxStream_t inputStream = {
 *     .buffersArray        = samplesBuffer,
 *     .numAvailableSamples = 0,
 *     .numProcessedSamples = 0
 * };
 * // Point to the same buffer for in-place processing
 * MaxxStream_t outputStream = inputStream;
 *
 * // Read frame of 480 samples to the input stream
 *
 * // Update available samples count in the input stream
 * inputStream.numAvailableSamples = 480;
 *
 * // Prepare I/O stream arrays
 * MaxxStream_t* inputStreams[1] = { &inputStream };
 * MaxxStream_t* outputStreams[1] = { &outputStream };
 *
 * // Effect must be already initialized to use 1 input stream and 1 output
 * //   stream with the expectedFormat
 *
 * // Process frame
 * MaxxEffect_Process(effect, inputStreams, outputStreams);
 *
 * // Validate result
 * if (inputStream.numAvailableSamples != inputStream.numProcessedSamples)
 * {
 *     // There are unread samples left in the input stream. Might be true if
 *     //   the input frame size is not divisible by the internal frame size.
 * }
 *
 * if (inputStream.numProcessedSamples != outputStream.numAvailableSamples)
 * {
 *     // Sanity check. In the current example number of processed samples must
 *     //   always be equal to the number of consumed samples since the sample
 *     //   rate matches between input and output streams format. Otherwise
 *     //   might differ by the resampling ratio.
 * }
 *
 * ~~~
 ******************************************************************************/
#ifndef MAXX_EFFECT_PROCESS_H
#define MAXX_EFFECT_PROCESS_H

#include "MaxxEffect/MaxxEffect.h"
#include "MaxxEffect/MaxxStatus.h"
#include "MaxxEffect/MaxxStream.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Get available samples from input streams and store processed in output.
 * All streams must have the same format that was used during initialization.
 *
 * Different streams might point to the same buffer for in-place processing, but
 *   only when sample rate is matched for input and output streams.
 *
 * Sets numProcessedSamples in inputStreams to number of read samples.
 * Sets numAvailableSamples in outputStreams to number of written samples.
 *
 * @see @ref CoreConcepts_DataPath_Streams for arrays content.
 *
 * @param[in] effect        initialized effect
 * @param[in] inputStreams  array of input streams
 * @param[in] outputStreams array of output streams
 *
 * @return 0 on success, otherwise fail
 ******************************************************************************/
MaxxStatus_t    MaxxEffect_Process(
    MaxxEffect_t*           effect,
    MaxxStream_t* const     inputStreams[],
    MaxxStream_t* const     outputStreams[]);

#ifdef __cplusplus
};
#endif

#endif
