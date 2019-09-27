/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifdef __SOF_AUDIO_EQ_IIR_H__

#ifndef __ARCH_AUDIO_EQ_IIR_H__
#define __ARCH_AUDIO_EQ_IIR_H__

/* If next define is set to 1 the EQ is configured automatically.
 * Setting to zero temporarily is useful for testing needs.
 */
#define CONFIG_IIR_AUTOARCH	1

/* Force manually some code variant when CONFIG_IIR_AUTOARCH is set to zero.
 * These are useful in code debugging.
 */
#if CONFIG_IIR_AUTOARCH

#if __XCC__

#include <xtensa/config/core-isa.h>

#if XCHAL_HAVE_HIFI3

#define CONFIG_IIR_ARCH	1

#endif /* XCHAL_HAVE_HIFI3 */

#endif /* __XCC__ */

#endif /* CONFIG_IIR_AUTOARCH */

#endif /* __ARCH_AUDIO_EQ_IIR_H__ */

#else

#error "This file shouldn't be included from outside of sof/audio/eq_iir.h"

#endif /* __SOF_AUDIO_EQ_IIR_H__ */
