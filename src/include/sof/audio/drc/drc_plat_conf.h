/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Google LLC. All rights reserved.
 *
 * Author: Pin-chih Lin <johnylin@google.com>
 */
#ifndef __SOF_AUDIO_DRC_DRC_PLAT_CONF_H__
#define __SOF_AUDIO_DRC_DRC_PLAT_CONF_H__

/* Get platforms configuration */

/* If next defines are set to 1 the DRC is configured automatically. Setting
 * to zero temporarily is useful is for testing needs.
 */
#define DRC_AUTOARCH    1

/* Force manually some code variant when DRC_AUTOARCH is set to zero. These
 * are useful in code debugging.
 */
#if DRC_AUTOARCH == 0
#define DRC_GENERIC	1
#define DRC_HIFI3	0
#endif

/* Select optimized code variant when xt-xcc compiler is used */
#if DRC_AUTOARCH == 1
#if defined __XCC__
#include <xtensa/config/core-isa.h>
#if XCHAL_HAVE_HIFI3 == 1
#define DRC_GENERIC	0
#define DRC_HIFI3	1
#else
#define DRC_GENERIC	1
#define DRC_HIFI3	0
#endif /* XCHAL_HAVE_HIFI3 */
#else
/* GCC */
#define DRC_GENERIC	1
#define DRC_HIFI3	0
#endif /* __XCC__ */
#endif /* DRC_AUTOARCH */

#endif /* __SOF_AUDIO_DRC_DRC_PLAT_CONF_H__ */
