/* SPDX-License-Identifier: BSD-3-Clause
 * 
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 * 
 * Author: Piotr Hoppe <piotr.hoppe@intel.com>
 */

#ifndef __LMDK_RTOS_PANIC_H__
#define __LMDK_RTOS_PANIC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* runtime assertion */
#define assert(x)	do { if (!(x)) while (1); } while (0)

#ifdef __cplusplus
}
#endif

#endif /* __LMDK_RTOS_PANIC_H__ */

