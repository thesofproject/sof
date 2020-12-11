/*
 * File: drc_log.c
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */

/* Include Files */
#include "drc_log.h"
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
 * function y = drc_log_fixpt(x)
 * Arguments    : const uint8_T x[10]
 *                cuint32_T y[10]
 * Return Type  : void
 */
void drc_log_fixpt(const uint8_T x[10], cuint32_T y[10])
{
  int32_T k;
  real_T u;

  
  for (k = 0; k < 10; k++)
  {
    u = log((real_T)x[k]) * 1.073741824E+9;
    if (fabs(u) < 4.503599627370496E+15)
    {
      if (u > 0.5)
      {
        if (rt_remd(u, 2.0) != 0.5)
        {
          u += 0.5;
        }

        u = floor(u);
      }
      else if (u >= -0.5)
      {
        u = 0.0;
      }
      else
      {
        if (rt_remd(u, 2.0) != -0.5)
        {
          u -= 0.5;
        }

        u = ceil(u);
      }
    }

    u = fmod(u, 4.294967296E+9);
    if (u < 0.0)
    {
      y[k].re = (uint32_T)((int32_T)(-((int32_T)((uint32_T)((real_T)(-u))))));
    }
    else
    {
      y[k].re = (uint32_T)u;
    }

    y[k].im = 0U;
  }
}


/*
 * function x = init_struc_fixpt
 * Arguments    : uint8_T x[10]
 * Return Type  : void
 */
void init_struc_fixpt(uint8_T x[10])
{
  int32_T i;
  // add/replace data here for testing purpose
  static const uint8_T uv[10] =
  {
    1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U
  };

  for (i = 0; i < 10; i++)
  {
    x[i] = uv[i];
  }

}

/*
 * File trailer for drc_log.c
 *
 * [EOF]
 */
