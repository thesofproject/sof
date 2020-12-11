/*
 * File: main.c
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */


/* Include Files */
#include "main.h"
#include "drc_floor.h"


/*
 * Arguments    : int32_T argc
 *                const char * const argv[]
 * Return Type  : int32_T
 */
int32_T main(int32_T argc, const char * const argv[])
{
  (void)argc;
  (void)argv;


  uint32_T x[5];
  uint8_T y[5];
  init_struc_fixpt(x);

  drc_floor_fixpt(x, y);
  return 0;
}

/*
 * File trailer for main.c
 *
 * [EOF]
 */
