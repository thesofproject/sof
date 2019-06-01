/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

#ifndef __INCLUDE_SOF_UT__
#define __INCLUDE_SOF_UT__

/* UT_STATIC makes function unit-testable (non-static) when built for unit tests
 */
#ifdef UNIT_TEST
#define UT_STATIC
#else
#define UT_STATIC static
#endif

#endif
