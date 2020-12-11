/*
 * File: main.c
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */


/* Include Files */
#include "main.h"
#include "frexp_log2.h"

/*
 * Arguments    : int32_T argc
 *                const char * const argv[]
 * Return Type  : int32_T
 */
int32_T main(int32_T argc, const char * const argv[])
{
  (void)argc;
  (void)argv;


  uint32_T x,F;
  uint8_T E;
  x = init_struc_fixpt();
  frexp_log2_fixpt(x, &F, &E);
 
  return 0;
}

/*
 * File trailer for main.c
 *
 * [EOF]
 */
