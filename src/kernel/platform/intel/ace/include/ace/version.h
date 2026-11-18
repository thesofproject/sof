/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Warecki <adrian.warecki@intel.com>
 */

#ifndef __ACE_VERSION_H__
#define __ACE_VERSION_H__

#define ACE_VERSION_1_5 0x10500 /* MTL */
#define ACE_VERSION_2_0 0x20000 /* LNL */
#define ACE_VERSION_3_0 0x30000 /* PTL */
#define ACE_VERSION_4_0 0x40000 /* NVL */

/* ACE version defined by CONFIG_ACE_VER_*/
#if defined(CONFIG_ACE_VERSION_1_5)
#define ACE_VERSION ACE_VERSION_1_5
#elif defined(CONFIG_ACE_VERSION_2_0)
#define ACE_VERSION ACE_VERSION_2_0
#elif defined(CONFIG_ACE_VERSION_3_0)
#define ACE_VERSION ACE_VERSION_3_0
#elif defined(CONFIG_ACE_VERSION_4_0)
#define ACE_VERSION ACE_VERSION_4_0
#endif

#define HW_CFG_VERSION		ACE_VERSION

#endif /* __ACE_VERSION_H__ */
