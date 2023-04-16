/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/**
  * \file xtos/include/sof/lib/dai.h
  * \brief DAI Drivers definition
  * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
  * \author Keyon Jie <yang.jie@linux.intel.com>
  */

#ifndef __SOF_LIB_DAI_H__
#define __SOF_LIB_DAI_H__

#ifdef CONFIG_ZEPHYR_NATIVE_DRIVERS
#include <sof/lib/dai-zephyr.h>
#else
#include <sof/lib/dai-legacy.h>
#endif

#endif /* __SOF_LIB_DAI_H__ */
