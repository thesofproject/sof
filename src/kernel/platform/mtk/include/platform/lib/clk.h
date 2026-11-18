/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Google LLC.  All rights reserved.
 * Author: Andy Ross <andyross@google.com>
 */
#ifndef _SOF_PLATFORM_MTK_LIB_CLK_H
#define _SOF_PLATFORM_MTK_LIB_CLK_H

#define CLK_CPU(x) (x)

// FIXME: set correctly from mtk_adsp layer!
#define CLK_MAX_CPU_HZ CONFIG_XTENSA_CCOUNT_HZ

#endif /* _SOF_PLATFORM_MTK_LIB_CLK_H */
