/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __SOF_AUDIO_SRC_SRC_CONFIG_H__
#define __SOF_AUDIO_SRC_SRC_CONFIG_H__

#include <sof/common.h>

/* Follow kconfig for FILTER in SRC component */
#if SOF_USE_MIN_HIFI(5, FILTER)
#define SRC_HIFI5      1
#elif SOF_USE_MIN_HIFI(4, FILTER)
#define SRC_HIFI4      1
#elif SOF_USE_MIN_HIFI(3, FILTER)
#define SRC_HIFI3      1
#elif SOF_USE_HIFI(2, FILTER)
#define SRC_HIFIEP     1
#else
#define SRC_GENERIC    1
#endif

/* Kconfig option tiny needs 16 bits coefficients, other options use 32 bits,
 * also gcc builds for all platforms and testbench (library)
 */
#if !defined(SRC_SHORT)
#if CONFIG_COMP_SRC_TINY
#define SRC_SHORT	1 /* 16 bit coefficients filter core */
#else
#define SRC_SHORT	0 /* 32 bit coefficients filter core */
#endif
#endif

#endif /* __SOF_AUDIO_SRC_SRC_CONFIG_H__ */
