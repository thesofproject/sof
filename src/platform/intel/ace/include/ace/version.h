/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Warecki <adrian.warecki@intel.com>
 */

#ifndef __ACE_VERSION_H__
#define __ACE_VERSION_H__

#define ACE_VERSION_1_5		0x10500

/* ACE version defined by CONFIG_ACE_VER_ */
#if CONFIG_ACE_VERSION_1_5
#define ACE_VERSION		ACE_VERSION_1_5
#endif

#define HW_CFG_VERSION		ACE_VERSION

#endif /* __ACE_VERSION_H__ */
