/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_AUDIO_FORMAT_HIFI3_H__
#define __SOF_AUDIO_FORMAT_HIFI3_H__

#include <xtensa/config/defs.h>
#include <xtensa/tie/xt_hifi3.h>
#include <stdint.h>

/* Saturation inline functions */

static inline int32_t sat_int32(int64_t x)
{
	return (ae_int32)AE_ROUND32F48SSYM(AE_SLAI64(x, 16));
}

static inline int32_t sat_int24(int32_t x)
{
	return AE_SRAI32(AE_SLAI32S(x, 8), 8);
}

static inline int16_t sat_int16(int32_t x)
{
	return AE_SAT16X4(x, x);
}

#endif /* __SOF_AUDIO_FORMAT_HIFI3_H__ */
