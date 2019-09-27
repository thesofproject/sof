/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifdef __SOF_AUDIO_EQ_FIR_H__

#ifndef __ARCH_AUDIO_EQ_FIR_EQ_FIR_H__
#define __ARCH_AUDIO_EQ_FIR_EQ_FIR_H__

/* If next define is set to 1 the EQ is configured automatically.
 * Setting to zero temporarily is useful for testing needs.
 */
#define CONFIG_FIR_AUTOARCH	1

/* Force manually some code variant when CONFIG_FIR_AUTOARCH is set to zero.
 * These are useful in code debugging.
 */
#if CONFIG_FIR_AUTOARCH

#if __XCC__

#include <xtensa/config/core-isa.h>

#if XCHAL_HAVE_HIFI2EP

#define CONFIG_FIR_ARCH_HIFI2EP	1

#include <arch/audio/eq_fir/eq_fir_hifi2ep.h>

#elif XCHAL_HAVE_HIFI3

#define CONFIG_FIR_ARCH_HIFI3	1

#include <arch/audio/eq_fir/eq_fir_hifi3.h>

#endif /* XCHAL_HAVE_HIFI2EP */

#define CONFIG_FIR_ARCH	(CONFIG_FIR_ARCH_HIFI2EP || CONFIG_FIR_ARCH_HIFI3)

#endif /* __XCC__ */

#endif /* CONFIG_FIR_AUTOARCH */

#endif /* __ARCH_AUDIO_EQ_FIR_EQ_FIR_H__ */

#else

#error "This file shouldn't be included from outside of sof/audio/eq_fir.h"

#endif /* __SOF_AUDIO_EQ_FIR_H__ */
