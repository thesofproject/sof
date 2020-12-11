/*
 * File: main.c
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */


/* Include Files */
#include "main.h"
#include "drc_ceil.h"

/*
 * Arguments    : int32_T argc
 *                const char * const argv[]
 * Return Type  : int32_T
 */
int32_T main(int32_T argc, const char * const argv[])
{
  (void)argc;
  (void)argv;


  uint32_T x;
  uint8_T y;
  x = init_struc_fixpt();
  y = drc_ceil_fixpt(x);
  return 0;
}

/*
 * File trailer for main.c
 *
 * [EOF]
 */
