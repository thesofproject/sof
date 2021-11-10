/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_AUDIO_FORMAT_GENERIC_H__
#define __SOF_AUDIO_FORMAT_GENERIC_H__

#include <stdint.h>

/* Saturation inline functions */

static inline int32_t sat_int32(int64_t x)
{
	if (x > INT32_MAX)
		return INT32_MAX;
	else if (x < INT32_MIN)
		return INT32_MIN;
	else
		return (int32_t)x;
}

static inline int32_t sat_int24(int32_t x)
{
	if (x > INT24_MAXVALUE)
		return INT24_MAXVALUE;
	else if (x < INT24_MINVALUE)
		return INT24_MINVALUE;
	else
		return x;
}

static inline int16_t sat_int16(int32_t x)
{
	if (x > INT16_MAX)
		return INT16_MAX;
	else if (x < INT16_MIN)
		return INT16_MIN;
	else
		return (int16_t)x;
}

#endif /* __SOF_AUDIO_FORMAT_GENERIC_H__ */
