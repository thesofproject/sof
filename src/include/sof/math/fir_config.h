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

/* Define SOFM_FIR_FORCEARCH 0/2/3 in build command line or temporarily in
 * this file to override the default auto detection.
 */
#ifdef SOFM_FIR_FORCEARCH
#  if SOFM_FIR_FORCEARCH == 3
#    define FIR_GENERIC	0
#    define FIR_HIFIEP	0
#    define FIR_HIFI3	1
#  elif SOFM_FIR_FORCEARCH == 2
#    define FIR_GENERIC	0
#    define FIR_HIFIEP	1
#    define FIR_HIFI3	0
#  elif SOFM_FIR_FORCEARCH == 0
#    define FIR_GENERIC	1
#    define FIR_HIFIEP	0
#    define FIR_HIFI3	0
#  else
#    error "Unsupported SOFM_FIR_FORCEARCH value."
#  endif
#else
#  if defined __XCC__
#    include <xtensa/config/core-isa.h>
#    define FIR_GENERIC	0
#    if XCHAL_HAVE_HIFI2EP == 1
#      define FIR_HIFIEP	1
#      define FIR_HIFI3	0
#    elif XCHAL_HAVE_HIFI3 == 1 || XCHAL_HAVE_HIFI4 == 1
#      define FIR_HIFI3	1
#      define FIR_HIFIEP	0
#    else
#      error "No HIFIEP or HIFI3 found. Cannot build FIR module."
#    endif
#  else
#    define FIR_GENERIC	1
#    define FIR_HIFI3	0
#  endif /* __XCC__ */
#endif /* SOFM_FIR_FORCEARCH */

#endif /* __SOF_AUDIO_EQ_FIR_FIR_CONFIG_H__ */
