/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __SOF_AUDIO_SRC_SRC_CONFIG_H__
#define __SOF_AUDIO_SRC_SRC_CONFIG_H__

#include <config.h>

/* If next define is set to 1 the SRC is configured automatically. Setting
 * to zero temporarily is useful is for testing needs.
 */
#define SRC_AUTOARCH    1

/* Force manually some code variant when SRC_AUTODSP is set to zero. These
 * are useful in code debugging.
 */
#if SRC_AUTOARCH == 0
#define SRC_SHORT	0
#define SRC_GENERIC	1
#define SRC_HIFIEP	0
#define SRC_HIFI3	0
#endif

/* Select optimized code variant when xt-xcc compiler is used */
#if SRC_AUTOARCH == 1
#if defined __XCC__
#include <xtensa/config/core-isa.h>
#define SRC_GENERIC	0
#if XCHAL_HAVE_HIFI2EP == 1
#define SRC_SHORT	1  /* Select 16 bit coefficients to save RAM */
#define SRC_HIFIEP	1
#define SRC_HIFI3	0
#endif
#if XCHAL_HAVE_HIFI3 == 1
#define SRC_SHORT	0  /* Select 32 bit default quality coefficients */
#define SRC_HIFI3	1
#define SRC_HIFIEP	0
#endif
#else
/* GCC */
#if CONFIG_LIBRARY
#define SRC_SHORT	0  /* Use high quality 32 bit filter coefficients */
#else
#define SRC_SHORT	1  /* Use 16 bit filter coefficients for speed */
#endif
#define SRC_GENERIC	1
#define SRC_HIFIEP	0
#define SRC_HIFI3	0
#endif
#endif

#endif /* __SOF_AUDIO_SRC_SRC_CONFIG_H__ */
