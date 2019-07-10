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

/* CAVS version defined by CONFIG_CAVS_VER_*/
#if defined(CONFIG_CAVS_VERSION_1_5)
#define CAVS_VERSION CAVS_VERSION_1_5
#elif defined(CONFIG_CAVS_VERSION_1_8)
#define CAVS_VERSION CAVS_VERSION_1_8
#elif defined(CONFIG_CAVS_VERSION_2_0)
#define CAVS_VERSION CAVS_VERSION_2_0
#endif

#endif /* __CAVS_VERSION_H__ */
