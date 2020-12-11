/*
 * File: main.c
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */



/* Include Files */
#include "main.h"
#include "drc_asin.h"

/*
 * Arguments    : int argc
 *                const char * const argv[]
 * Return Type  : int
 */
int main(int argc, const char * const argv[])
{
  (void)argc;
  (void)argv;


  //main_drc_asin_fixpt();
  //main_init_struc_fixpt();

  signed char x[2];
  int y[2];
  init_struc_fixpt(x);
  drc_asin_fixpt(x, y);
  /* Terminate the application.
     You do not need to do this more than one time. */
  drc_asin_terminate();
  return 0;
}

/*
 * File trailer for main.c
 *
 * [EOF]
 */
