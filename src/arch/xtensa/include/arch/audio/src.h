/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifdef __SOF_AUDIO_SRC_H__

#ifndef __ARCH_AUDIO_SRC_H__
#define __ARCH_AUDIO_SRC_H__

#include <config.h>

/* If next define is set to 1 the SRC is configured automatically.
 * Setting to zero temporarily is useful for testing needs.
 */
#define CONFIG_SRC_AUTOARCH	1

/* Force manually some code variant when CONFIG_SRC_AUTOARCH is set to zero.
 * These are useful in code debugging.
 */
#if CONFIG_SRC_AUTOARCH

#if __XCC__

#include <xtensa/config/core-isa.h>

#if XCHAL_HAVE_HIFI2EP

#define CONFIG_SRC_ARCH_HIFI2EP	1

/* Use 16 bit filter coefficients for speed */
#define CONFIG_SRC_SHORT_COEFF	1

#elif XCHAL_HAVE_HIFI3

#define CONFIG_SRC_ARCH_HIFI3	1

#endif /* XCHAL_HAVE_HIFI2EP */

#define CONFIG_SRC_ARCH	(CONFIG_SRC_ARCH_HIFI2EP || CONFIG_SRC_ARCH_HIFI3)

#else

#if !CONFIG_LIBRARY

/* Use 16 bit filter coefficients for speed */
#define CONFIG_SRC_SHORT_COEFF	1

#endif /* CONFIG_LIBRARY */

#endif /* __XCC__ */

#endif /* CONFIG_SRC_AUTOARCH */

#endif /* __ARCH_AUDIO_SRC_H__ */

#else

#error "This file shouldn't be included from outside of sof/audio/src.h"

#endif /* __SOF_AUDIO_SRC_H__ */
