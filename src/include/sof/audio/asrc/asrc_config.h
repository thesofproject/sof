/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2012 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_AUDIO_ASRC_ASRC_CONFIG_H__
#define __SOF_AUDIO_ASRC_ASRC_CONFIG_H__

/* If next define is set to 1 the ASRC is configured automatically. Setting
 * to zero temporarily is useful is for testing needs.
 */
#define ASRC_AUTOARCH    1

/* Select optimized code variant when xt-xcc compiler is used on HiFi3 */
#if ASRC_AUTOARCH == 1
#if defined __XCC__
/* For xt-xcc */
#include <xtensa/config/core-isa.h>
#if XCHAL_HAVE_HIFI3 == 1
/* Version for HiFi3 */
#define ASRC_HIFI3	1
#define ASRC_GENERIC	0
#else
/* Version for e.g. HiFi2EP */
#define ASRC_HIFI3	0
#define ASRC_GENERIC	1
#endif
#else
/* For GCC */
#define ASRC_GENERIC	1
#define ASRC_HIFI3	0
#endif /* XCC */
#else
/* Applied when ASRC_AUTOARCH is set to zero */
#define ASRC_GENERIC	1 /* Enable generic */
#define ASRC_HIFI3	0 /* Disable HiFi3  */
#endif /* Autoarch */

#endif /* __SOF_AUDIO_ASRC_ASRC_CONFIG_H__ */
