/*
 * File: drc_round.c
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */

/* Include Files */
#include "drc_round.h"

/* Function Definitions */

/*
 * function y = drc_round_fixpt(x)
 * Arguments    : unsigned int x
 * Return Type  : unsigned char
 */
unsigned char drc_round_fixpt(unsigned int x)
{
  return (unsigned char)((int)(((int)((unsigned int)(((x >> ((unsigned int)28))
    + 1U) >> ((unsigned int)1)))) & 7));
}



/*
 * function x = init_struc_fixpt
 * Arguments    : void
 * Return Type  : unsigned int
 */
unsigned int init_struc_fixpt(void)
{
  return 3733693161U;
}

/*
 * File trailer for drc_round.c
 *
 * [EOF]
 */
