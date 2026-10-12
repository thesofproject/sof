/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 */

/**
 * @file sof/audio/format_generic.h
 * @brief Generic audio sample saturation helpers.
 * @ingroup audio_format_api
 */

#ifndef __SOF_AUDIO_FORMAT_GENERIC_H__
#define __SOF_AUDIO_FORMAT_GENERIC_H__

#include <stdint.h>

/** \addtogroup audio_format_api
 *  @{
 */

/**
 * \brief Saturate a 64-bit integer to signed 32-bit integer range.
 * \param[in] x Input 64-bit integer.
 * \return Saturated 32-bit integer clamped between INT32_MIN and INT32_MAX.
 */
static inline int32_t sat_int32(int64_t x)
{
	if (x > INT32_MAX)
		return INT32_MAX;
	else if (x < INT32_MIN)
		return INT32_MIN;
	else
		return (int32_t)x;
}

/**
 * \brief Saturate a 32-bit integer to signed 24-bit integer range.
 * \param[in] x Input 32-bit integer.
 * \return Saturated 24-bit integer clamped between INT24_MINVALUE and INT24_MAXVALUE.
 */
static inline int32_t sat_int24(int32_t x)
{
	if (x > INT24_MAXVALUE)
		return INT24_MAXVALUE;
	else if (x < INT24_MINVALUE)
		return INT24_MINVALUE;
	else
		return x;
}

/**
 * \brief Saturate a 32-bit integer to signed 16-bit integer range.
 * \param[in] x Input 32-bit integer.
 * \return Saturated 16-bit integer clamped between INT16_MIN and INT16_MAX.
 */
static inline int16_t sat_int16(int32_t x)
{
	if (x > INT16_MAX)
		return INT16_MAX;
	else if (x < INT16_MIN)
		return INT16_MIN;
	else
		return (int16_t)x;
}

/**
 * \brief Saturate a 32-bit integer to signed 8-bit integer range.
 * \param[in] x Input 32-bit integer.
 * \return Saturated 8-bit integer clamped between INT8_MIN and INT8_MAX.
 */
static inline int8_t sat_int8(int32_t x)
{
	if (x > INT8_MAX)
		return INT8_MAX;
	else if (x < INT8_MIN)
		return INT8_MIN;
	else
		return (int8_t)x;
}

/** @}*/

#endif /* __SOF_AUDIO_FORMAT_GENERIC_H__ */
