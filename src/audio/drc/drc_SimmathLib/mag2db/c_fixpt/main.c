/*
 * File: main.c
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */


/* Include Files */
#include "main.h"
#include "drc_mag2db.h"
/*
 * Arguments    : int argc
 *                const char * const argv[]
 * Return Type  : int
 */
int main(int argc, const char * const argv[])
{
  (void)argc;
  (void)argv;
  struct0_T tstruct;
  cint32_T ydB_data[1];
  int ydB_size[2];
  init_struc_fixpt(&tstruct);
  drc_mag2db_fixpt(&tstruct, ydB_data, ydB_size);
  return 0;
}

/*
 * File trailer for main.c
 *
 * [EOF]
 */
