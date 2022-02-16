/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 */

#ifndef __ACE_VERSION_H__
#define __ACE_VERSION_H__

#define CAVS_VERSION_1_5 0x10500
#define CAVS_VERSION_1_8 0x10800
#define CAVS_VERSION_2_0 0x20000
#define CAVS_VERSION_2_5 0x20500

#if CONFIG_CAVS_VERSION_1_5
#define CAVS_VERSION CAVS_VERSION_1_5
#elif CONFIG_CAVS_VERSION_1_8
#define CAVS_VERSION CAVS_VERSION_1_8
#elif CONFIG_CAVS_VERSION_2_0
#define CAVS_VERSION CAVS_VERSION_2_0
#elif CONFIG_CAVS_VERSION_2_5
#define CAVS_VERSION CAVS_VERSION_2_5
#endif

#ifdef CAVS_VERSION
#ifndef CAVS_VER_HW_CFG
#define CAVS_VER_HW_CFG CAVS_VERSION
#endif
#endif

#define ACE_VERSION_1_0 0x10000

#if CONFIG_ACE_VERSION_1_0
#define ACE_VERSION ACE_VERSION_1_0
#endif

#ifdef ACE_VERSION
#ifndef CAVS_VER_HW_CFG
#define CAVS_VER_HW_CFG ACE_VERSION
#endif
#endif

#endif /* __ACE_VERSION_H__ */
