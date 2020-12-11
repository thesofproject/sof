/*
 * File: drc_ceil.c
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */

/* Include Files */
#include "drc_ceil.h"

/* Function Definitions */

/*
 * function y = drc_ceil_fixpt(x)
 * Arguments    : uint32_T x
 * Return Type  : uint8_T
 */
uint8_T drc_ceil_fixpt(uint32_T x)
{
  return (uint8_T)((int32_T)(((int32_T)((uint32_T)((x >> ((uint32_T)29)) +
    ((uint32_T)(((x & 536870911U) != 0U) ? 1 : 0))))) & 7));
}


/*
 * function x = init_struc_fixpt
 * Arguments    : void
 * Return Type  : uint32_T
 */
uint32_T init_struc_fixpt(void)
{
  return 3733693161U;
}

/*
 * File trailer for drc_ceil.c
 *
 * [EOF]
 */
