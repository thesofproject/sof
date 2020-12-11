/*
 * File: main.c
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */


/* Include Files */
#include "main.h"
#include "drc_db2mag.h"


/*
 * Arguments    : int32_T argc
 *                const char * const argv[]
 * Return Type  : int32_T
 */
int32_T main(int32_T argc, const char * const argv[])
{
  (void)argc;
  (void)argv;


  struct0_T tstruct;
  uint32_T y;

  init_struc_fixpt(&tstruct);
  y = drc_db2mag_fixpt(&tstruct);

  return 0;
}

/*
 * File trailer for main.c
 *
 * [EOF]
 */
