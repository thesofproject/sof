/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
 */

#ifndef __CAVS_VERSION_H__
#define __CAVS_VERSION_H__

#define CAVS_VERSION_2_5 0x20500

/* CAVS version defined by CONFIG_CAVS_VER_*/
#if CONFIG_CAVS_VERSION_2_5
#define CAVS_VERSION CAVS_VERSION_2_5
#endif

#define HW_CFG_VERSION	CAVS_VERSION

#endif /* __CAVS_VERSION_H__ */
