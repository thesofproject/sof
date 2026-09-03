/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

#ifndef __SOF_LIB_DAI_H__
#define __SOF_LIB_DAI_H__

#ifdef CONFIG_ZEPHYR_NATIVE_DRIVERS
#include <sof/lib/dai-zephyr.h>
#else
#include <sof/lib/dai-legacy.h>
#endif

#endif /* __SOF_LIB_DAI_H__ */
