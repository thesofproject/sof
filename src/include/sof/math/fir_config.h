/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_AUDIO_EQ_FIR_FIR_CONFIG_H__
#define __SOF_AUDIO_EQ_FIR_FIR_CONFIG_H__

/* Prevent xtensa gcc built firmware to be configured for longer
 * filter that it can process. This length limitation (# of taps) is for one
 * channel, for stereo the channel specific limit is this divided by two,
 * etc.
 */
#ifndef __XCC__
#ifdef __XTENSA__
#define FIR_MAX_LENGTH_BUILD_SPECIFIC	80
#endif
#endif

/* If next defines are set to 1 the EQ is configured automatically. Setting
 * to zero temporarily is useful is for testing needs.
 * Setting EQ_FIR_AUTOARCH to 0 allows to manually set the code variant.
 */
#define FIR_AUTOARCH    1

/* Force manually some code variant when EQ_FIR_AUTODSP is set to zero. These
 * are useful in code debugging.
 */
#if FIR_AUTOARCH == 0
#define FIR_GENERIC	0
#define FIR_HIFIEP	0
#define FIR_HIFI3	1
#endif

/* Select optimized code variant when xt-xcc compiler is used */
#if FIR_AUTOARCH == 1
#if defined __XCC__
#include <xtensa/config/core-isa.h>
#define FIR_GENERIC	0
#if XCHAL_HAVE_HIFI2EP == 1
#define FIR_HIFIEP	1
#define FIR_HIFI3	0
#elif XCHAL_HAVE_HIFI3 == 1
#define FIR_HIFI3	1
#define FIR_HIFIEP	0
#else
#error "No HIFIEP or HIFI3 found. Cannot build FIR module."
#endif
#else
/* GCC */
#define FIR_GENERIC	1
#define FIR_HIFIEP	0
#define FIR_HIFI3	0
#endif
#endif

#endif /* __SOF_AUDIO_EQ_FIR_FIR_CONFIG_H__ */
