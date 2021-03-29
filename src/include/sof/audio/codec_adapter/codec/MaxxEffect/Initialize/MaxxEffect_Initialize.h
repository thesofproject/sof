/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Waves Audio Ltd. All rights reserved.
 */
/*******************************************************************************
 * @page Initialization_Default Default Initialization
 *
 * Refer to the diagram below for the default initialization workflow.
 *
 * ![Default initialization workflow]
 * (Initialize/Default_Initialization.svg)
 *
 * Example code for the default effect initialization with the interleaved
 *   stereo streams processing at 48 kHz with Q1.31 fixed-point data samples:
 *
 * ~~~{.c}
 * uint32_t bytes = 0;
 * MaxxEffect_t* effect = NULL;
 *
 * // Get required size for storing MaxxEffect_t handler
 * if (0 == MaxxEffect_GetEffectSize(&bytes))
 * {
 *     // Allocate required size for effect
 *     effect = (MaxxEffect_t*)malloc(bytes);
 *     if (NULL != effect)
 *     {
 *         // Prepare expected formats
 *         // Identical for both input and output streams
 *         MaxxStreamFormat_t const expectedFormat = {
 *             .sampleRate = 48000,
 *             .numChannels = 2,
 *             .samplesFormat = MAXX_BUFFER_FORMAT_Q1_31,
 *             .samplesLayout = MAXX_BUFFER_LAYOUT_INTERLEAVED
 *         };
 *
 *         // Prepare I/O stream format arrays
 *         MaxxStreamFormat_t const* inputFormats[1];
 *         MaxxStreamFormat_t const* outputFormats[1];
 *         inputFormats[0] = &expectedFormat;
 *         outputFormats[0] = &expectedFormat;
 *
 *         // Initialize effect with the expected IO formats
 *         if (0 == MaxxEffect_Initialize(effect,
 *                                        inputFormats, 1,
 *                                        outputFormats, 1))
 *         {
 *             // MaxxEffect successfully initialized
 *         }
 *     }
 * }
 * ~~~
 ******************************************************************************/
#ifndef MAXX_EFFECT_INITIALIZE_H
#define MAXX_EFFECT_INITIALIZE_H

#include <stdint.h>
#include "MaxxEffect/MaxxEffect.h"
#include "MaxxEffect/MaxxStatus.h"
#include "MaxxEffect/MaxxStream.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Provides with the required data size for holding @ref MaxxEffect_t handler.
 *   It is caller responsibility to allocate enough memory (bytes) for effect.
 *
 * @see MaxxEffect_Initialize for allocated effect initialization
 *
 * @param[out] bytes   required data size
 *
 * @return 0 if success, otherwise fail
 ******************************************************************************/
MaxxStatus_t    MaxxEffect_GetEffectSize(
    uint32_t*           bytes);


/*******************************************************************************
 * Initializes preallocated @ref MaxxEffect_t handler to the requested scenario.
 *   Required scenario is determined based on the provided streams formats.
 *   Supported streams count is defined by specific product.
 *
 * @see MaxxEffect_GetEffectSize for the required effect size
 *
 * @param[in]  effect               pointer to the pre-allocated handler
 * @param[in]  inputFormats         array of pointers to the input formats
 * @param[in]  inputFormatsCount    number of elements in the inputFormats
 * @param[in]  outputFormats        array of pointers to the output formats
 * @param[in]  outputFormatsCount   number of elements in the outputFormats
 *
 * @return 0 if effect is initialized, non-zero error code otherwise
 ******************************************************************************/
MaxxStatus_t    MaxxEffect_Initialize(
    MaxxEffect_t*                   effect,
    MaxxStreamFormat_t* const       inputFormats[],
    uint32_t                        inputFormatsCount,
    MaxxStreamFormat_t* const       outputFormats[],
    uint32_t                        outputFormatsCount);

#ifdef __cplusplus
}
#endif

#endif
