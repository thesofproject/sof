/*
 * File: main.c
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */


/* Include Files */
#include "main.h"
#include "drc_sin.h"

/*
 * Arguments    : int argc
 *                const char * const argv[]
 * Return Type  : int
 */
int main(int argc, const char * const argv[])
{
  (void)argc;
  (void)argv;

  /*signed char iv[2];*/
  int y[2];
  signed char x[2];
  init_struc_fixpt(x);
  drc_sin_fixpt(x, y);
  return 0;
}

/*
 * File trailer for main.c
 *
 * [EOF]
 */
