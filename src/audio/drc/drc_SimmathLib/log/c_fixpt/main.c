/*
 * File: main.c
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */


/* Include Files */
#include "main.h"
#include "drc_log.h"

/*
 * Arguments    : int32_T argc
 *                const char * const argv[]
 * Return Type  : int32_T
 */
int32_T main(int32_T argc, const char * const argv[])
{
  (void)argc;
  (void)argv;

  cuint32_T y[10];
  uint8_T x[10];
  init_struc_fixpt(x);
  drc_log_fixpt(x, y);
  return 0;
}

/*
 * File trailer for main.c
 *
 * [EOF]
 */
