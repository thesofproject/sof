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

#endif /* __SOF_AUDIO_EQ_FIR_FIR_CONFIG_H__ */
