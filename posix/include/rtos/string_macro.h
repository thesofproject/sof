/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Baofeng Tian <Baofeng.tian@intel.com>
 */

#ifndef __POSIX_RTOS_STRING_MACRO_H__
#define __POSIX_RTOS_STRING_MACRO_H__

#define Z_STRINGIFY(x) #x
#define STRINGIFY(s) Z_STRINGIFY(s)

#define _DO_CONCAT(x, y) x ## y
#define _CONCAT(x, y) _DO_CONCAT(x, y)

#endif /* __POSIX_RTOS_STRING_MACRO_H__ */
