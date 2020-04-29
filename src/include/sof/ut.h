/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

#ifndef __SOF_UT_H__
#define __SOF_UT_H__

/* UT_STATIC makes function unit-testable (non-static) when built for unit tests
 */
#if defined UNIT_TEST || defined __ZEPHYR__
#define UT_STATIC
#else
#define UT_STATIC static
#endif

#endif /* __SOF_UT_H__ */
