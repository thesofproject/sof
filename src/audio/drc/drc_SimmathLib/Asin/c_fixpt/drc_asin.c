/*
 * File: drc_asin.c
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */

/* Include Files */
#include "drc_asin.h"

/* Function Declarations */
static int asr_s32(int u, unsigned int n);
static signed char iScalarCordicAsin(signed char sinValue, short nIters, const
  signed char LUT[6]);

/* Function Definitions */

/*
 * Arguments    : int u
 *                unsigned int n
 * Return Type  : int
 */
static int asr_s32(int u, unsigned int n)
{
  int y;
  if (u >= 0)
  {
    y = (int)((unsigned int)(((unsigned int)u) >> n));
  }
  else
  {
    y = (-((int)((unsigned int)(((unsigned int)((int)(-1 - u))) >> n)))) - 1;
  }

  return y;
}

/*
 * Arguments    : signed char sinValue
 *                short nIters
 *                const signed char LUT[6]
 * Return Type  : signed char
 */
static signed char iScalarCordicAsin(signed char sinValue, short nIters, const
  signed char LUT[6])
{
  signed char z;
  signed char b_sinValue;
  signed char x;
  signed char y;
  int i;
  int b_i;
  short j;
  short k;
  signed char xShift;
  signed char xDShift;
  signed char yShift;
  signed char yDShift;
  b_sinValue = (signed char)(sinValue * 32);
  if (b_sinValue > 22)
  {
    x = 0;
    y = 64;
    z = 50;
  }
  else
  {
    x = 64;
    y = 0;
    z = 0;
  }

  b_sinValue = (signed char)(b_sinValue * 2);
  i = ((int)nIters) - 1;
  if (i < -32768)
  {
    i = -32768;
  }

  for (b_i = 0; b_i < i; b_i++)
  {
    j = (short)((b_i + 1) * 2);
    if (j >= 7)
    {
      j = 7;
    }

    if (b_i < 7)
    {
      k = (short)b_i;
    }
    else
    {
      k = 7;
    }

    xShift = (signed char)asr_s32((int)x, (unsigned int)k);
    xDShift = (signed char)asr_s32((int)x, (unsigned int)j);
    yShift = (signed char)asr_s32((int)y, (unsigned int)k);
    yDShift = (signed char)asr_s32((int)y, (unsigned int)j);
    if (y == b_sinValue)
    {
      x += xDShift;
      y += yDShift;
    }
    else if (((y >= b_sinValue) && (x >= 0)) || ((y < b_sinValue) && (x < 0)))
    {
      x = (signed char)(((signed char)(x - xDShift)) + yShift);
      y = (signed char)(((signed char)(y - yDShift)) - xShift);
      z -= LUT[b_i];
    }
    else
    {
      x = (signed char)(((signed char)(x - xDShift)) - yShift);
      y = (signed char)(((signed char)(y - yDShift)) + xShift);
      z += LUT[b_i];
    }

    b_sinValue += (signed char)asr_s32((int)b_sinValue, (unsigned int)j);
  }

  if (z < 0)
  {
    z = (signed char)(-z);
  }

  return z;
}

/*
 * function y = drc_asin_fixpt(x)
 * Arguments    : const signed char x[2]
 *                int y[2]
 * Return Type  : void
 */
void drc_asin_fixpt(const signed char x[2], int y[2])
{
  signed char i;
  static const signed char iv[6] =
  {
    29, 15, 7, 3, 1, 0
  };

  int i1;

 
  if (x[0] >= 0)
  {
    i = iScalarCordicAsin(x[0], (short)7, iv);
  }
  else
  {
    i = (signed char)(-x[0]);
    if ((i & 2) != 0)
    {
      i1 = ((int)i) | -2;
    }
    else
    {
      i1 = ((int)i) & 1;
    }

    i = (signed char)(-iScalarCordicAsin((signed char)i1, (short)7, iv));
  }

  y[0] = ((int)i) * 33554432;
  if (x[1] >= 0)
  {
    i = iScalarCordicAsin(x[1], (short)7, iv);
  }
  else
  {
    i = (signed char)(-x[1]);
    if ((i & 2) != 0)
    {
      i1 = ((int)i) | -2;
    }
    else
    {
      i1 = ((int)i) & 1;
    }

    i = (signed char)(-iScalarCordicAsin((signed char)i1, (short)7, iv));
  }

  y[1] = ((int)i) * 33554432;
}

/*
 * Arguments    : void
 * Return Type  : void
 */
void drc_asin_initialize(void)
{
}

/*
 * Arguments    : void
 * Return Type  : void
 */
void drc_asin_terminate(void)
{
  /* (no terminate code required) */
}

/*
 * function x = init_struc_fixpt
 * Arguments    : signed char x[2]
 * Return Type  : void
 */
void init_struc_fixpt(signed char x[2])
{
  x[0] = -1;
  x[1] = 1;
  /* 'init_struc_fixpt:8' fm = get_fimath(); */
  /* 'init_struc_fixpt:10' x = fi([-1,1], 1, 2, 0, fm); */
}

/*
 * File trailer for drc_asin.c
 *
 * [EOF]
 */
