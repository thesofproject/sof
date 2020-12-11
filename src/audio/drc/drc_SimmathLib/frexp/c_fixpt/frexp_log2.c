/*
 * File: frexp_log2.c
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */

/* Include Files */
#include "frexp_log2.h"
#include <float.h>
#include <math.h>

/* Function Declarations */
static real_T rt_remd(real_T u0, real_T u1);

/* Function Definitions */

/*
 * Arguments    : real_T u0
 *                real_T u1
 * Return Type  : real_T
 */
static real_T rt_remd(real_T u0, real_T u1)
{
  real_T y;
  real_T q;
  if ((u1 != 0.0) && (u1 != trunc(u1)))
  {
    q = fabs(u0 / u1);
    if (fabs(q - floor(q + 0.5)) <= (DBL_EPSILON * q))
    {
      y = 0.0;
    }
    else
    {
      y = fmod(u0, u1);
    }
  }
  else
  {
    y = fmod(u0, u1);
  }

  return y;
}

/*
 * function [F,E ] = frexp_log2_fixpt(x)
 * Arguments    : uint32_T x
 *                uint32_T *F
 *                uint8_T *E
 * Return Type  : void
 */
void frexp_log2_fixpt(uint32_T x, uint32_T *F, uint8_T *E)
{
  real_T var1;
  int32_T eint;

  var1 = frexp(((real_T)x) * 9.3132257461547852E-10, &eint); /*hardcore this to 0.57499999995343387*/

  var1 *= 4.294967296E+9;
  if (var1 > 0.5)
  {
    if (rt_remd(var1, 2.0) != 0.5)
    {
      var1 += 0.5;
    }

    var1 = floor(var1);
  }
  else if (var1 >= -0.5)
  {
    var1 = 0.0;
  }
  else
  {
    if (rt_remd(var1, 2.0) != -0.5)
    {
      var1 -= 0.5;
    }

    var1 = ceil(var1);
  }

  var1 = fmod(var1, 4.294967296E+9);
  if (var1 < 0.0)
  {
    *F = (uint32_T)((int32_T)(-((int32_T)((uint32_T)((real_T)(-var1))))));
  }
  else
  {
    *F = (uint32_T)var1;
  }

  /* 'frexp_log2_fixpt:25' E = fi(fmo_2, 0, 2, 0, fm); */
  *E = (uint8_T)(((uint8_T)eint) & ((uint8_T)3));

  /*  [yVal]  = log(x)/log(2); % log2(x) yval= log(val)/log(2) */
}

/*
 * function x = init_struc_fixpt
 * Arguments    : void
 * Return Type  : uint32_T
 */
uint32_T init_struc_fixpt(void)
{
  return 2469606195U;
}

/*
 * File trailer for frexp_log2.c
 *
 * [EOF]
 */
