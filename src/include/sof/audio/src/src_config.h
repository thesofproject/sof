/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __SOF_AUDIO_SRC_SRC_CONFIG_H__
#define __SOF_AUDIO_SRC_SRC_CONFIG_H__



/* If next define is set to 1 the SRC is configured automatically. Setting
 * to zero temporarily is useful is for testing needs.
 */
#define SRC_AUTOARCH    1

/* Force manually some code variant when SRC_AUTODSP is set to zero. These
 * are useful in code debugging.
 */
#if SRC_AUTOARCH == 0
#define SRC_GENERIC	1
#define SRC_HIFIEP	0
#define SRC_HIFI3	0
#endif

/* Select optimized code variant when xt-xcc compiler is used */
#if SRC_AUTOARCH == 1
#if defined __XCC__
#include <xtensa/config/core-isa.h>
#define SRC_GENERIC	0
#if XCHAL_HAVE_HIFI4 == 1
#define SRC_HIFI4	1
#define SRC_HIFI3	0
#define SRC_HIFIEP	0
#else
#define SRC_HIFI4	0
#if XCHAL_HAVE_HIFI2EP == 1
#define SRC_HIFIEP	1
#define SRC_HIFI3	0
#endif
#if XCHAL_HAVE_HIFI3 == 1
#define SRC_HIFI3	1
#define SRC_HIFIEP	0
#endif
#endif
#else
/* GCC */
#define SRC_GENERIC	1
#define SRC_HIFIEP	0
#define SRC_HIFI3	0
#if CONFIG_LIBRARY
#else
#define SRC_SHORT	1 /* Need to use for generic code version speed */
#endif
#endif
#endif

/* Kconfig option tiny needs 16 bits coefficients, other options use 32 bits */
#if !defined(SRC_SHORT)
#if CONFIG_COMP_SRC_TINY
#define SRC_SHORT	1 /* 16 bit coefficients filter core */
#else
#define SRC_SHORT	0 /* 32 bit coefficients filter core */
#endif
#endif

#endif /* __SOF_AUDIO_SRC_SRC_CONFIG_H__ */
