/*
 * File: main.c
 *
 */
 // SPDX - License - Identifier: BSD - 3 - Clause
 //
 //Copyright(c) 2021 Intel Corporation.All rights reserved.
 //
 //Author : Shriram Shastry <malladi.sastry@linux.intel.com>

#pragma warning (disable : 4013)  //warning C4013 : 'strerror' undefined; assuming extern returning int
#pragma warning (disable : 4996)  //error C4996 : 'fopen' : This function or variable may be unsafe

/* Include Files */
#include "main.h"
#include "ref_sin_fixpt.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/*
 * Arguments    : int32_t argc
 *                const char * const argv[]
 * Return Type  : int32_t
 * Input Q12.20, OutputQ3.29
 */
int32_t main(int32_t argc, const char * const argv[])
{
  (void)argc;
  (void)argv;



  int32_t thRadFxp[1024];
  int32_t cdcSinTh[1024];
  int errnum,i;
  mkdir("Results", 0777);
  FILE *fd = fopen("Results/ref_cordsin_fixed.txt", "w");
  fprintf(fd, " %15s %15s %15s\n ", "Index", "Inval-X[q12.20]", "Outval-Y[q3.29]");
  if (fd == NULL) {

      errnum = errno;
      fprintf(stderr, "Value of errno: %d\n", errno);
      perror("Error printed by perror");
      fprintf(stderr, "Error opening file: %d\n", strerror(errnum));

  }
  init_data_fixpt(thRadFxp);
  ref_sine_fixpt(thRadFxp, cdcSinTh);
  for (i = 0; i < 1024; i++)
  {
      fprintf(fd, "%15d %15li %15li\n ", i, thRadFxp[i], cdcSinTh[i]);
  }
  fclose(fd);
  return 0;
}

/*
 * File trailer for main.c
 *
 * [EOF]
 */
