/*
 * File: drc_floor.c
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */

/* Include Files */
#include "drc_floor.h"

/* Function Definitions */

/*
 * function y = drc_floor_fixpt(x)
 * Arguments    : const uint32_T x[5]
 *                uint8_T y[5]
 * Return Type  : void
 */
void drc_floor_fixpt(const uint32_T x[5], uint8_T y[5])
{
  int32_T i;

  for (i = 0; i < 5; i++)
  {
    y[i] = (uint8_T)(x[i] >> ((uint32_T)31));
  }
}

/*
 * function x = init_struc_fixpt
 * Arguments    : uint32_T x[5]
 * Return Type  : void
 */
void init_struc_fixpt(uint32_T x[5])
{
  int32_T i;
  static const uint32_T uv[5] =
  {
    3321224261U, 3535972626U, 3750720991U, 3965469356U, 4180217721U
  };

  for (i = 0; i < 5; i++)
  {
    x[i] = uv[i];
  }

}

/*
 * File trailer for drc_floor.c
 *
 * [EOF]
 */
