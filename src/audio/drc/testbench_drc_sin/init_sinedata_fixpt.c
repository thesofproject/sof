// SPDX - License - Identifier: BSD - 3 - Clause
//
//Copyright(c) 2020 Intel Corporation.All rights reserved.
//
//Author : Shriram Shastry <malladi.sastry@linux.intel.com>
#pragma once
#include "typdef.h"
#include "init_sinedata_fixpt.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/*
 * function x = init_struc_fixpt
 * Arguments    : int32_t x[21]
 * Return Type  : void
 */
void init_sinedata_fixpt(int32_t x[TEST_VECTOR])
{
  static const int32_t iv[TEST_VECTOR] = { -1073741824, -966367642, -858993459,
    -751619277, -644245094, -536870912, -429496730, -322122547, -214748365,
    -107374182, 0, 107374182, 214748365, 322122547, 429496730, 536870912,
    644245094, 751619277, 858993459, 966367642, 1073741824 };

    memcpy(&x[0], &iv[0], 21U * (sizeof(int32_t)));
  /*  x = fi((-1:0.1:1),1,32,30); */
  /* Q2.30 */
}
/*
 * File trailer for init_sinedata_fixpt.c
 *
 * [EOF]
 */