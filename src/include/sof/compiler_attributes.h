/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 */

#define __packed __attribute__((packed))

#define __aligned(x) __attribute__((__aligned__(x)))

#define __section(x) __attribute__((section(x)))
