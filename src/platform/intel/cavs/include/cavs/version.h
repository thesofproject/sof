/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 */

#ifndef __CAVS_VERSION_H__
#define __CAVS_VERSION_H__

#define CAVS_VERSION_1_5 0x10500
#define CAVS_VERSION_1_8 0x10800
#define CAVS_VERSION_2_0 0x20000
#define CAVS_VERSION_2_5 0x20500

/* CAVS version defined by CONFIG_CAVS_VER_*/
#if CONFIG_CAVS_VERSION_1_5
#define CAVS_VERSION CAVS_VERSION_1_5
#elif CONFIG_CAVS_VERSION_1_8
#define CAVS_VERSION CAVS_VERSION_1_8
#elif CONFIG_CAVS_VERSION_2_0
#define CAVS_VERSION CAVS_VERSION_2_0
#elif CONFIG_CAVS_VERSION_2_5
#define CAVS_VERSION CAVS_VERSION_2_5
#endif

#define HW_CFG_VERSION	CAVS_VERSION

#endif /* __CAVS_VERSION_H__ */
